#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "tcp_sender/worker.h"
#include "common/util.h"
#include "common/stopwatch.h"

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
  unsigned nconnections_per_worker, diff;
  unsigned i;

  if (pthread_mutex_init(&workers->mutex, NULL) != 0) {
    return -1;
  }

  if (pthread_cond_init(&workers->cond, NULL) != 0) {
    pthread_mutex_destroy(&workers->mutex);
    return -1;
  }

  nworkers = options->nthreads;
  nconnections_per_worker = options->nconnections / nworkers;
  diff = options->nconnections - (nworkers * nconnections_per_worker);

  *running = 1;
  *ready = 0;

  /* Create workers. */
  for (i = 0; i < nworkers; i++) {
    worker = &workers->workers[i];

    worker->id = i;

    worker->addr = addr;
    worker->addrlen = addrlen;

    worker->options = options;

    worker->connections = connections;
    worker->nconnections = nconnections_per_worker;

    if (diff > 0) {
      worker->nconnections++;
      diff--;
    }

    /* Create epoll file descriptor. */
    if ((worker->epfd = epoll_create1(0)) < 0) {
      workers->nworkers = i;
      workers_free(workers, running);

      return -1;
    }

    if ((worker->events =
         (struct epoll_event*) malloc(connections->nconnections *
                                      sizeof(struct epoll_event))) == NULL) {
      close(worker->epfd);

      workers->nworkers = i;
      workers_free(workers, running);

      return -1;
    }

    if ((worker->fds = (int*) malloc(connections->nconnections *
                                     sizeof(int))) == NULL) {
      free(worker->events);
      close(worker->epfd);

      workers->nworkers = i;
      workers_free(workers, running);

      return -1;
    }

    worker->sent = 0;
    worker->received = 0;

    worker->errors = 0;

    worker->duration = 0;

    worker->mutex = &workers->mutex;
    worker->cond = &workers->cond;

    worker->running = running;
    worker->ready = ready;

    /* Start thread. */
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
      free(worker->fds);
      free(worker->events);
      close(worker->epfd);

      workers->nworkers = i;
      workers_free(workers, running);

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

void workers_free(workers_t* workers, uint8_t* running)
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

    free(worker->fds);
    free(worker->events);
    close(worker->epfd);
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_join(workers_t* workers, uint8_t* running)
{
  worker_t* worker;
  struct timespec ts, timeout;
  sigset_t set;
  unsigned i;
  int sig;

  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);

  /* Join threads. */
  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    do {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec++;

      if (pthread_timedjoin_np(worker->thread, NULL, &ts) != 0) {
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;

        /* If a signal has been received... */
        if ((sig = sigtimedwait(&set, NULL, &timeout)) != -1) {
          printf("Received signal %d.\n", sig);

          *running = 0;

          for (; i < workers->nworkers; i++) {
            worker = &workers->workers[i];

            pthread_join(worker->thread, NULL);

            free(worker->fds);
            free(worker->events);
            close(worker->epfd);
          }

          pthread_cond_destroy(&workers->cond);
          pthread_mutex_destroy(&workers->mutex);

          return;
        }
      } else {
        free(worker->fds);
        free(worker->events);
        close(worker->epfd);

        break;
      }
    } while (1);
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_statistics(const workers_t* workers)
{
  const worker_t* worker;
  uint64_t sent, received;
  suseconds_t duration;
  float tx_speed, rx_speed;
  unsigned i;

  sent = 0;
  received = 0;
  duration = 0;
  tx_speed = 0.0;
  rx_speed = 0.0;

  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    printf("Worker %u: sent: %lu, received: %lu, %u connection%s, %u "
           "error%s, TX: %s, RX: %s, duration: %s.\n",
           worker->id + 1,
           worker->sent,
           worker->received,
           worker->nconnections,
           (worker->nconnections != 1) ? "s" : "",
           worker->errors,
           (worker->errors != 1) ? "s" : "",
           transfer_speed_as_string(compute_transfer_speed(worker->sent,
                                                           worker->duration)),
           transfer_speed_as_string(compute_transfer_speed(worker->received,
                                                           worker->duration)),
           duration_as_string(worker->duration));

    sent += worker->sent;
    received += worker->received;

    tx_speed += (((float) worker->sent / (float) worker->duration) * 1000000.0);

    rx_speed += (((float) worker->received / (float) worker->duration) *
                 1000000.0);

    if (worker->duration > duration) {
      duration = worker->duration;
    }
  }

  printf("Duration: %s.\n", duration_as_string(duration));

  printf("Sent: %lu byte%s (%s).\n",
         sent,
         (sent != 1) ? "s" : "",
         transfer_speed_as_string(tx_speed));

  if (received > 0) {
    printf("Received: %lu byte%s (%s).\n",
           received,
           (received != 1) ? "s" : "",
           transfer_speed_as_string(rx_speed));
  }
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
  stopwatch_t stopwatch;
  int nevents, i;
  int fd;

  /* If we have to set the CPU affinity for this thread... */
  if (worker->id < worker->options->nprocessors) {
    CPU_ZERO(&cpuset);
    CPU_SET(worker->options->processors[worker->id], &cpuset);

    if (pthread_setaffinity_np(worker->thread,
                               sizeof(cpu_set_t),
                               &cpuset) != 0) {
      worker->errors = worker->nconnections;
      return -1;
    }
  }

  connections = worker->connections->connections;
  maxevents = worker->connections->nconnections;

  stopwatch_start(&stopwatch);

  /* For each connection... */
  for (nconnections = 0;
       (nconnections < worker->nconnections) && (*(worker->running));
       nconnections++) {
    /* Connect. */
    if ((fd = socket_connect(worker->addr, worker->addrlen)) < 0) {
      worker->duration = stopwatch_stop(&stopwatch);

      for (; nconnections > 0; nconnections--) {
        close(worker->fds[nconnections - 1]);
      }

      worker->errors += (worker->nconnections - nconnections);

      return -1;
    }

    /* Add file descriptor to epoll. */
    ev.events = EPOLLRDHUP | EPOLLET | worker->options->set_read_write_event ?
                                         EPOLLIN | EPOLLOUT :
                                         EPOLLOUT;

    ev.data.u64 = fd;

    if (epoll_ctl(worker->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
      worker->duration = stopwatch_stop(&stopwatch);

      close(fd);

      for (; nconnections > 0; nconnections--) {
        close(worker->fds[nconnections - 1]);
      }

      worker->errors += (worker->nconnections - nconnections);

      return -1;
    }

    /* Save file descriptor. */
    worker->fds[nconnections] = fd;

    /* Set connection's state. */
    connections[fd].state = STATE_CONNECTING;

    /* Set connection's index. */
    connections[fd].index = nconnections;
  }

  while ((*(worker->running)) && (nconnections > 0)) {
    /* Wait for events. */
    nevents = epoll_wait(worker->epfd, worker->events, maxevents, 1000);

    /* Process events. */
    for (i = 0; i < nevents; i++) {
      fd = worker->events[i].data.fd;
      conn = &connections[fd];

      if (worker->events[i].events & EPOLLIN) {
        conn->readable = 1;
      }

      if (worker->events[i].events & EPOLLOUT) {
        conn->writable = 1;
      }

      if (handle_connection(worker, conn, fd) < 0) {
        /* If not the last connection... */
        if (conn->index + 1 < nconnections) {
          worker->fds[conn->index] = worker->fds[nconnections - 1];
          connections[worker->fds[nconnections - 1]].index = conn->index;
        }

        /* Clear connection. */
        connection_clear(conn);

        /* Close connection. */
        close(fd);

        nconnections--;
      }
    }
  }

  for (j = 0; j < nconnections; j++) {
    close(worker->fds[j]);
  }

  worker->duration = stopwatch_stop(&stopwatch);

  return 0;
}

int handle_connection(worker_t* worker, connection_t* conn, int fd)
{
  const file_t* file;
  struct epoll_event ev;
  int sockerr;
  size_t to_send;
  size_t to_receive;
  ssize_t ret;

  do {
    switch (conn->state) {
      case STATE_CONNECTING:
        if (!conn->writable) {
          return 0;
        }

        if ((socket_get_error(fd, &sockerr) < 0) || (sockerr != 0)) {
          worker->errors++;
          return -1;
        }

        if (conn->options->client_sends_first) {
          conn->state = STATE_SENDING;
        } else {
          if (!worker->options->set_read_write_event) {
            /* Modify file descriptor in epoll. */
            ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
            ev.data.u64 = fd;

            if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
              worker->errors++;
              return -1;
            }
          }

          conn->state = STATE_RECEIVING;
        }

        break;
      case STATE_SENDING:
        if (!conn->writable) {
          return 0;
        }

        file = &conn->options->files.files[conn->nfile];

        /* Send. */
        to_send = file->len - conn->sent;
        if ((ret = socket_send(fd, file->data + conn->sent, to_send)) < 0) {
          if (errno == EAGAIN) {
            /* Mark the connection as not writable. */
            conn->writable = 0;

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
            /* If we are not expected to receive data... */
            if (conn->options->receive == 0) {
              /* Last file? */
              if (++conn->nfile == conn->options->files.used) {
                /* Last loop? */
                if (++conn->nloops == conn->options->nloops) {
                  /* We are done with this connection. */
                  return -1;
                }

                conn->nfile = 0;
              }
            } else {
              if (!worker->options->set_read_write_event) {
                /* Modify file descriptor in epoll. */
                ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
                ev.data.u64 = fd;

                if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                  worker->errors++;
                  return -1;
                }
              }

              conn->state = STATE_RECEIVING;
            }
          } else {
            /* Last file? */
            if (++conn->nfile == conn->options->files.used) {
              /* Last loop? */
              if (++conn->nloops == conn->options->nloops) {
                /* We are done with this connection. */
                return -1;
              }

              conn->nfile = 0;
            }

            if (!worker->options->set_read_write_event) {
              /* Modify file descriptor in epoll. */
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLIN;
              ev.data.u64 = fd;

              if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                worker->errors++;
                return -1;
              }
            }

            conn->state = STATE_RECEIVING;
          }

          /* Reset number of bytes sent. */
          conn->sent = 0;
        } else {
          /* Mark the connection as not writable. */
          conn->writable = 0;
        }

        break;
      case STATE_RECEIVING:
        if (!conn->readable) {
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
            /* Last file? */
            if (++conn->nfile == conn->options->files.used) {
              /* Last loop? */
              if (++conn->nloops == conn->options->nloops) {
                /* We are done with this connection. */
                return -1;
              }

              conn->nfile = 0;
            }

            if (!worker->options->set_read_write_event) {
              /* Modify file descriptor in epoll. */
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLOUT;
              ev.data.u64 = fd;

              if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                worker->errors++;
                return -1;
              }
            }

            conn->state = STATE_SENDING;
          } else {
            if (!worker->options->set_read_write_event) {
              /* Modify file descriptor in epoll. */
              ev.events = EPOLLRDHUP | EPOLLET | EPOLLOUT;
              ev.data.u64 = fd;

              if (epoll_ctl(worker->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                worker->errors++;
                return -1;
              }
            }

            conn->state = STATE_SENDING;
          }

          /* Reset number of bytes received. */
          conn->received = 0;
        } else {
          /* Mark the connection as not readable. */
          conn->readable = 0;
        }

        break;
      default:
        ;
    }
  } while (*(worker->running));

  return -1;
}
