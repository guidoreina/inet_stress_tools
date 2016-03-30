#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "udp_receiver/worker.h"

static void* worker_main(void* arg);
static int worker_loop(worker_t* worker);

int workers_create(const struct sockaddr* addr,
                   socklen_t addrlen,
                   const options_t* options,
                   uint8_t* running,
                   uint8_t* ready,
                   workers_t* workers)
{
  worker_t* worker;
  struct mmsghdr* hdr;
  uint8_t* buf;
  unsigned nworkers;
  unsigned i, j;

  if (pthread_mutex_init(&workers->mutex, NULL) != 0) {
    return -1;
  }

  if (pthread_cond_init(&workers->cond, NULL) != 0) {
    pthread_mutex_destroy(&workers->mutex);
    return -1;
  }

  /* Single receiver socket? */
  if (options->single_receiver_socket) {
    /* Create socket. */
    if ((workers->fd = socket_create(addr->sa_family, SOCK_DGRAM)) < 0) {
      pthread_cond_destroy(&workers->cond);
      pthread_mutex_destroy(&workers->mutex);

      return -1;
    }

    /* Bind. */
    if (socket_bind(workers->fd, addr, addrlen) < 0) {
      close(workers->fd);

      pthread_cond_destroy(&workers->cond);
      pthread_mutex_destroy(&workers->mutex);

      return -1;
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

    /* Multiple receiver sockets? */
    if (!options->single_receiver_socket) {
      /* Create socket. */
      if ((worker->fd = socket_create(addr->sa_family, SOCK_DGRAM)) < 0) {
        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }

      /* Bind. */
      if (socket_bind(worker->fd, addr, addrlen) < 0) {
        close(worker->fd);

        workers->nworkers = i;
        workers_free(workers, options, running);

        return -1;
      }
    } else {
      worker->fd = workers->fd;
    }

    /* Allocate memory for holding received data. */
    if ((worker->buf =
         (uint8_t*) malloc(options->number_messages_per_receive *
                           MAX_MESSAGE_SIZE)) == NULL) {
      if (!options->single_receiver_socket) {
        close(worker->fd);
      }

      workers->nworkers = i;
      workers_free(workers, options, running);

      return -1;
    }

    if ((worker->iov =
         (struct iovec*) malloc(options->number_messages_per_receive *
                                sizeof(struct iovec))) == NULL) {
      free(worker->buf);

      if (!options->single_receiver_socket) {
        close(worker->fd);
      }

      workers->nworkers = i;
      workers_free(workers, options, running);

      return -1;
    }

    /* Create messages to be received. */
    if ((worker->msgvec =
         (struct mmsghdr*) malloc(options->number_messages_per_receive *
                                  sizeof(struct mmsghdr))) == NULL) {
      free(worker->iov);
      free(worker->buf);

      if (!options->single_receiver_socket) {
        close(worker->fd);
      }

      workers->nworkers = i;
      workers_free(workers, options, running);

      return -1;
    }

    buf = worker->buf;

    for (j = 0; j < options->number_messages_per_receive; j++) {
      worker->iov[j].iov_base = buf;
      worker->iov[j].iov_len = MAX_MESSAGE_SIZE;

      buf += MAX_MESSAGE_SIZE;

      hdr = &worker->msgvec[j];

      hdr->msg_hdr.msg_name = NULL;
      hdr->msg_hdr.msg_namelen = 0;
      hdr->msg_hdr.msg_iov = &worker->iov[j];
      hdr->msg_hdr.msg_iovlen = 1;
      hdr->msg_hdr.msg_control = NULL;
      hdr->msg_hdr.msg_controllen = 0;
      hdr->msg_hdr.msg_flags = 0;

      hdr->msg_len = 0;
    }

    worker->nmsgs = 0;

    worker->received = 0;

    worker->error = 0;

    worker->mutex = &workers->mutex;
    worker->cond = &workers->cond;

    worker->running = running;
    worker->ready = ready;

    /* Start thread. */
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
      free(worker->msgvec);
      free(worker->iov);
      free(worker->buf);

      if (!options->single_receiver_socket) {
        close(worker->fd);
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

    free(worker->msgvec);
    free(worker->iov);
    free(worker->buf);

    if (!options->single_receiver_socket) {
      close(worker->fd);
    }
  }

  if (options->single_receiver_socket) {
    close(workers->fd);
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_join(workers_t* workers,
                  const options_t* options,
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

    free(worker->msgvec);
    free(worker->iov);
    free(worker->buf);

    if (!options->single_receiver_socket) {
      close(worker->fd);
    }
  }

  if (options->single_receiver_socket) {
    close(workers->fd);
  }

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_statistics(const workers_t* workers)
{
  const worker_t* worker;
  uint64_t count;
  uint64_t received;
  unsigned i;

  count = 0;
  received = 0;

  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    printf("Worker %u: received: %lu packet%s (%lu byte%s), %s.\n",
           worker->id + 1,
           worker->nmsgs,
           (worker->nmsgs != 1) ? "s" : "",
           worker->received,
           (worker->received != 1) ? "s" : "",
           worker->error ? "error" : "no errors");

    count += worker->nmsgs;
    received += worker->received;
  }

  printf("Received: %lu packet%s (%lu byte%s).\n",
         count,
         (count != 1) ? "s" : "",
         received,
         (received != 1) ? "s" : "");
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
  int ret, nevents, i;

  /* If we have to set the CPU affinity for this thread... */
  if (worker->id < worker->options->nprocessors) {
    CPU_ZERO(&cpuset);
    CPU_SET(worker->options->processors[worker->id], &cpuset);

    if (pthread_setaffinity_np(worker->thread,
                               sizeof(cpu_set_t),
                               &cpuset) != 0) {
      worker->error = 1;
      return -1;
    }
  }

  while (*(worker->running)) {
    if ((ret = socket_recvmmsg(worker->fd,
                               worker->msgvec,
                               worker->options->number_messages_per_receive,
                               NULL)) < 0) {
      if (errno == EAGAIN) {
        do {
          if ((nevents = socket_wait_readable(worker->fd, 100)) < 0) {
            if (errno != EINTR) {
              worker->error = 1;
              return -1;
            }
          } else if (nevents > 0) {
              break;
          }
        } while (*(worker->running));
      } else {
        worker->error = 1;
        return -1;
      }
    } else {
      worker->nmsgs += ret;

      for (i = 0; i < ret; i++) {
        worker->received += worker->msgvec[i].msg_len;
      }
    }
  }

  return 0;
}
