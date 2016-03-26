#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "tcp_receiver/worker.h"

static void* worker_main(void* arg);
static int worker_loop(worker_t* worker);
static int handle_connection(worker_t* worker, connection_t* conn, int fd);

int workers_create(const struct sockaddr* addr,
                   socklen_t addrlen,
                   const options_t* options,
                   connections_t* connections,
                   uint8_t* running,
                   uint8_t* ready,
                   workers_t* workers)
{
  worker_t* worker;
  unsigned nworkers;
  struct epoll_event ev;
  unsigned i;

  if (pthread_mutex_init(&workers->mutex, NULL) != 0) {
    return -1;
  }

  if (pthread_cond_init(&workers->cond, NULL) != 0) {
    pthread_mutex_destroy(&workers->mutex);
    return -1;
  }

  /* Single listener? */
  if (options->single_listener) {
    /* Listen. */
    if ((workers->listener = socket_listen(addr, addrlen)) < 0) {
      pthread_cond_destroy(&workers->cond);
      pthread_mutex_destroy(&workers->mutex);

      return -1;
    }

    /* If the epoll fd will be shared among all the threads... */
    if (options->single_epoll_fd) {
      /* Create epoll file descriptor. */
      if ((workers->epfd = epoll_create1(0)) < 0) {
        close(workers->listener);

        pthread_cond_destroy(&workers->cond);
        pthread_mutex_destroy(&workers->mutex);

        return -1;
      }

      /* Add file descriptor to epoll. */
      ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
      ev.data.u64 = workers->listener;

      if (epoll_ctl(workers->epfd, EPOLL_CTL_ADD, workers->listener, &ev) < 0) {
        close(workers->epfd);
        close(workers->listener);

        pthread_cond_destroy(&workers->cond);
        pthread_mutex_destroy(&workers->mutex);

        return -1;
      }
    }
  }

  nworkers = options->nthreads;

  *running = 1;
  *ready = 0;

  /* Create workers. */
  for (i = 0; i < nworkers; i++) {
    worker = &workers->workers[i];

    worker->id = i;

    worker->options = options;

    worker->connections = connections;
    worker->nconnections = 0;

    /* Multiple listeners? */
    if (!options->single_listener) {
      /* Listen. */
      if ((worker->listener = socket_listen(addr, addrlen)) < 0) {
        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }
    } else {
      worker->listener = workers->listener;
    }

    /* If the epoll fd will not be shared among all the threads... */
    if (!options->single_epoll_fd) {
      /* Create epoll file descriptor. */
      if ((worker->epfd = epoll_create1(0)) < 0) {
        if (!options->single_listener) {
          close(worker->listener);
        }

        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }

      /* Add file descriptor to epoll. */
      ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
      ev.data.u64 = worker->listener;

      if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, worker->listener, &ev) < 0) {
        close(worker->epfd);

        if (!options->single_listener) {
          close(worker->listener);
        }

        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }
    } else {
      worker->epfd = workers->epfd;
    }

    if ((worker->events =
         (struct epoll_event*) malloc(connections->nconnections *
                                      sizeof(struct epoll_event))) == NULL) {
      if (!options->single_epoll_fd) {
        close(worker->epfd);
      }

      if (!options->single_listener) {
        close(worker->listener);
      }

      workers->nworkers = i;
      workers_free(workers, options, running);

      return -1;
    }

    /* If the epoll fd will not be shared among all the threads... */
    if (!options->single_epoll_fd) {
      if ((worker->fds = (int*) malloc(connections->nconnections *
                                       sizeof(int))) == NULL) {
        free(worker->events);

        close(worker->epfd);

        if (!options->single_listener) {
          close(worker->listener);
        }

        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }
    }

    worker->sent = 0;
    worker->received = 0;

    worker->errors = 0;

    worker->mutex = &workers->mutex;
    worker->cond = &workers->cond;

    worker->running = running;
    worker->ready = ready;

    /* Start thread. */
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
      if (!options->single_epoll_fd) {
        free(worker->fds);
        close(worker->epfd);
      }

      free(worker->events);

      if (!options->single_listener) {
        close(worker->listener);
      }

      workers->nworkers = i;
      workers_free(workers, options, running);

      return -1;
    }
  }

  workers->nworkers = i;

  /* Notify threads that now they can start their job. */
  pthread_mutex_lock(&workers->mutex);

  *ready = 1;
  pthread_cond_broadcast(&workers->cond);

  pthread_mutex_unlock(&workers->mutex);

  return 0;
}

void workers_free(workers_t* workers,
                  const options_t* options,
                  uint8_t* running)
{
  worker_t* worker;
  unsigned i;

  /* Notify threads that they have to stop. */
  pthread_mutex_lock(&workers->mutex);

  *running = 0;
  pthread_cond_broadcast(&workers->cond);

  pthread_mutex_unlock(&workers->mutex);

  /* Join threads. */
  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    pthread_join(worker->thread, NULL);

    if (!options->single_epoll_fd) {
      free(worker->fds);
      close(worker->epfd);
    }

    free(worker->events);

    if (!options->single_listener) {
      close(worker->listener);
    }
  }

  if (options->single_listener) {
    close(workers->listener);

    if (options->single_epoll_fd) {
      close(workers->epfd);
    }
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_join(workers_t* workers,
                  const options_t* options,
                  connections_t* connections,
                  uint8_t* running)
{
  worker_t* worker;
  sigset_t set;
  unsigned i;
  int sig;

  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);

  /* Wait for a signal to arrive. */
  sigwait(&set, &sig);

  printf("Received signal %d.\n", sig);

  *running = 0;

  /* Join threads. */
  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    pthread_join(worker->thread, NULL);

    if (!options->single_epoll_fd) {
      free(worker->fds);
      close(worker->epfd);
    }

    free(worker->events);

    if (!options->single_listener) {
      close(worker->listener);
    }
  }

  if (options->single_listener) {
    close(workers->listener);

    if (options->single_epoll_fd) {
      close(workers->epfd);

      for (i = 0; i < connections->nconnections; i++) {
        if (connections->connections[i].state != STATE_NOT_CONNECTED) {
          close(i);
        }
      }
    }
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_statistics(const workers_t* workers)
{
  const worker_t* worker;
  uint64_t sent, received;
  unsigned i;

  sent = 0;
  received = 0;

  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    printf("Worker %u: sent: %lu, received: %lu, %u connection%s accepted, "
           "%u error%s.\n",
           worker->id + 1,
           worker->sent,
           worker->received,
           worker->nconnections,
           (worker->nconnections != 1) ? "s" : "",
           worker->errors,
           (worker->errors != 1) ? "s" : "");

    sent += worker->sent;
    received += worker->received;
  }

  if (sent > 0) {
    printf("Sent: %lu byte%s.\n", sent, (sent != 1) ? "s" : "");
  }

  printf("Received: %lu byte%s.\n", received, (received != 1) ? "s" : "");
}

void* worker_main(void* arg)
{
  worker_t* worker;

  worker = (worker_t*) arg;

  /* Wait for all the threads to be ready. */
  pthread_mutex_lock(worker->mutex);

  while ((*(worker->running)) && (!*(worker->ready))) {
    pthread_cond_wait(worker->cond, worker->mutex);
  }

  if (!*(worker->running)) {
    pthread_mutex_unlock(worker->mutex);
    return NULL;
  }

  pthread_mutex_unlock(worker->mutex);

  worker_loop(worker);

  return NULL;
}

int worker_loop(worker_t* worker)
{
  cpu_set_t cpuset;
  unsigned nconnections, j;
  connection_t* connections;
  connection_t* conn;
  struct epoll_event ev;
  unsigned maxevents;
  int nevents, i;
  int fd, client;

  /* If we have to set the CPU affinity for this thread... */
  if (worker->id < worker->options->nprocessors) {
    CPU_ZERO(&cpuset);
    CPU_SET(worker->options->processors[worker->id], &cpuset);

    if (pthread_setaffinity_np(worker->thread,
                               sizeof(cpu_set_t),
                               &cpuset) != 0) {
      worker->errors++;
      return -1;
    }
  }

  nconnections = 0;
  connections = worker->connections->connections;
  maxevents = worker->connections->nconnections;

  while (*(worker->running)) {
    /* Wait for events. */
    nevents = epoll_wait(worker->epfd, worker->events, maxevents, 1000);

    /* Process events. */
    for (i = 0; i < nevents; i++) {
      fd = worker->events[i].data.fd;

      /* New connection? */
      if (fd == worker->listener) {
        do {
          /* Accept new connection. */
          if ((client = socket_accept(fd, NULL, NULL)) != -1) {
            /* If the client sends first... */
            if (worker->options->client_sends_first) {
              /* Set connection's state. */
              connections[client].state = STATE_RECEIVING;

              if (worker->options->set_read_write_event) {
                if (!worker->options->single_epoll_fd) {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN | EPOLLOUT;
                } else {
                  ev.events = EPOLLRDHUP |
                              EPOLLET |
                              EPOLLONESHOT |
                              EPOLLIN |
                              EPOLLOUT;
                }
              } else {
                if (!worker->options->single_epoll_fd) {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
                } else {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
                }
              }
            } else {
              /* Set connection's state. */
              connections[client].state = STATE_SENDING;

              if (worker->options->set_read_write_event) {
                if (!worker->options->single_epoll_fd) {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN | EPOLLOUT;
                } else {
                  ev.events = EPOLLRDHUP |
                              EPOLLET |
                              EPOLLONESHOT |
                              EPOLLIN |
                              EPOLLOUT;
                }
              } else {
                if (!worker->options->single_epoll_fd) {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLOUT;
                } else {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLOUT;
                }
              }
            }

            if (!worker->options->single_epoll_fd) {
              /* Save file descriptor. */
              worker->fds[nconnections] = client;

              /* Set connection's index. */
              connections[client].index = nconnections++;
            }

            /* Increment number of connections accepted by this worker. */
            worker->nconnections++;

            /* Add file descriptor to epoll. */
            ev.data.u64 = client;

            if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, client, &ev) < 0) {
              if (!worker->options->single_epoll_fd) {
                for (; nconnections > 0; nconnections--) {
                  close(worker->fds[nconnections - 1]);
                }
              }

              worker->errors++;

              return -1;
            }
          } else {
            if (errno == EAGAIN) {
              break;
            } else {
              if (!worker->options->single_epoll_fd) {
                for (; nconnections > 0; nconnections--) {
                  close(worker->fds[nconnections - 1]);
                }
              }

              worker->errors++;

              return -1;
            }
          }
        } while (1);

        if (worker->options->single_epoll_fd) {
          /* Rearm file descriptor in epoll. */
          ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
          ev.data.u64 = worker->listener;

          if (epoll_ctl(worker->epfd,
                        EPOLL_CTL_MOD,
                        worker->listener,
                        &ev) < 0) {
            worker->errors++;
            return -1;
          }
        }
      } else {
        conn = &connections[fd];

        /* If the connection has been already closed... */
        if (conn->state == STATE_NOT_CONNECTED) {
          continue;
        }

        if (worker->events[i].events & EPOLLIN) {
          conn->readable = 1;
        }

        if (worker->events[i].events & EPOLLOUT) {
          conn->writable = 1;
        }

        if (handle_connection(worker, conn, fd) < 0) {
          if (!worker->options->single_epoll_fd) {
            /* If not the last connection... */
            if (conn->index + 1 < nconnections) {
              worker->fds[conn->index] = worker->fds[nconnections - 1];
              connections[worker->fds[nconnections - 1]].index = conn->index;
            }

            nconnections--;
          }

          /* Clear connection. */
          connection_clear(conn);

          /* Close connection. */
          close(fd);
        }
      }
    }
  }

  if (!worker->options->single_epoll_fd) {
    for (j = 0; j < nconnections; j++) {
      close(worker->fds[j]);
    }
  }

  return 0;
}

int handle_connection(worker_t* worker, connection_t* conn, int fd)
{
  const file_t* file;
  struct epoll_event ev;
  size_t to_send;
  size_t to_receive;
  ssize_t ret;

  do {
    switch (conn->state) {
      case STATE_RECEIVING:
        if (!conn->readable) {
          if (worker->options->single_epoll_fd) {
            /* Rearm file descriptor in epoll. */
            if (worker->options->set_read_write_event) {
              ev.events = EPOLLRDHUP |
                          EPOLLET |
                          EPOLLONESHOT |
                          EPOLLIN |
                          EPOLLOUT;
            } else {
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
            }

            ev.data.u64 = fd;

            if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
              worker->errors++;
              return -1;
            }
          }

          return 0;
        }

        /* Receive. */
        to_receive = conn->options->receive - conn->received;
        switch (ret = socket_recv(fd,
                                  conn->recvbuf + conn->received,
                                  to_receive)) {
          case -1:
            if (errno == EAGAIN) {
              /* Mark the connection as not readable. */
              conn->readable = 0;

              if (worker->options->single_epoll_fd) {
                /* Rearm file descriptor in epoll. */
                if (worker->options->set_read_write_event) {
                  ev.events = EPOLLRDHUP |
                              EPOLLET |
                              EPOLLONESHOT |
                              EPOLLIN |
                              EPOLLOUT;
                } else {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
                }

                ev.data.u64 = fd;

                if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                  worker->errors++;
                  return -1;
                }
              }

              return 0;
            } else {
              worker->errors++;
              return -1;
            }

            break;
          case 0:
            /* Peer closed the connection. */
            worker->errors++;
            return -1;
        }

        /* Increment number of bytes received by the worker. */
        worker->received += ret;

        /* If we have received all the data... */
        if ((conn->received += ret) == conn->options->receive) {
          /* If the client sends first... */
          if (conn->options->client_sends_first) {
            /* If we are not expected to send data... */
            if (conn->options->files.used == 0) {
              /* Last loop? */
              if (++conn->nloops == conn->options->number_connection_loops) {
                /* We are done with this connection. */
                return -1;
              }
            } else {
              conn->state = STATE_SENDING;

              if (!worker->options->set_read_write_event) {
                /* Modify file descriptor in epoll. */
                if (!worker->options->single_epoll_fd) {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLOUT;
                } else {
                  ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLOUT;
                }

                ev.data.u64 = fd;

                if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                  worker->errors++;
                  return -1;
                }

                if (!conn->writable) {
                  return 0;
                }
              }
            }
          } else {
            /* Last file? */
            if (++conn->nfile == conn->options->files.used) {
              /* Last loop? */
              if (++conn->nloops == conn->options->number_connection_loops) {
                /* We are done with this connection. */
                return -1;
              }

              conn->nfile = 0;
            }

            conn->state = STATE_SENDING;

            if (!worker->options->set_read_write_event) {
              /* Modify file descriptor in epoll. */
              if (!worker->options->single_epoll_fd) {
                ev.events = EPOLLRDHUP | EPOLLET | EPOLLOUT;
              } else {
                ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLOUT;
              }

              ev.data.u64 = fd;

              if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                worker->errors++;
                return -1;
              }

              if (!conn->writable) {
                return 0;
              }
            }
          }

          /* Reset number of bytes received. */
          conn->received = 0;
        } else {
          /* Mark the connection as not readable. */
          conn->readable = 0;
        }

        break;
      case STATE_SENDING:
        if (!conn->writable) {
          if (worker->options->single_epoll_fd) {
            /* Rearm file descriptor in epoll. */
            if (worker->options->set_read_write_event) {
              ev.events = EPOLLRDHUP |
                          EPOLLET |
                          EPOLLONESHOT |
                          EPOLLIN |
                          EPOLLOUT;
            } else {
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLOUT;
            }

            ev.data.u64 = fd;

            if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
              worker->errors++;
              return -1;
            }
          }

          return 0;
        }

        file = &conn->options->files.files[conn->nfile];

        /* Send. */
        to_send = file->len - conn->sent;
        if ((ret = socket_send(fd, file->data + conn->sent, to_send)) < 0) {
          if (errno == EAGAIN) {
            /* Mark the connection as not writable. */
            conn->writable = 0;

            if (worker->options->single_epoll_fd) {
              /* Rearm file descriptor in epoll. */
              if (worker->options->set_read_write_event) {
                ev.events = EPOLLRDHUP |
                            EPOLLET |
                            EPOLLONESHOT |
                            EPOLLIN |
                            EPOLLOUT;
              } else {
                ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLOUT;
              }

              ev.data.u64 = fd;

              if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                worker->errors++;
                return -1;
              }
            }

            return 0;
          } else {
            worker->errors++;
            return -1;
          }
        }

        /* Increment number of bytes sent by the worker. */
        worker->sent += ret;

        /* If we have sent all the data... */
        if ((conn->sent += ret) == file->len) {
          /* If the client sends first... */
          if (conn->options->client_sends_first) {
            /* Last file? */
            if (++conn->nfile == conn->options->files.used) {
              /* Last loop? */
              if (++conn->nloops == conn->options->number_connection_loops) {
                /* We are done with this connection. */
                return -1;
              }

              conn->nfile = 0;
            }
          }

          conn->state = STATE_RECEIVING;

          if (!worker->options->set_read_write_event) {
            /* Modify file descriptor in epoll. */
            if (!worker->options->single_epoll_fd) {
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
            } else {
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLONESHOT | EPOLLIN;
            }

            ev.data.u64 = fd;

            if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
              worker->errors++;
              return -1;
            }

            if (!conn->readable) {
              return 0;
            }
          }

          /* Reset number of bytes sent. */
          conn->sent = 0;
        } else {
          /* Mark the connection as not writable. */
          conn->writable = 0;
        }

        break;
      default:
        ;
    }
  } while (*(worker->running));

  return -1;
}
