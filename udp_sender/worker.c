#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "udp_sender/worker.h"
#include "common/util.h"
#include "common/stopwatch.h"

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
  unsigned nworkers;
  uint64_t nsends_per_worker, diff;
  unsigned i, k;
  uint64_t idx;
  size_t j;

  if (pthread_mutex_init(&workers->mutex, NULL) != 0) {
    return -1;
  }

  if (pthread_cond_init(&workers->cond, NULL) != 0) {
    pthread_mutex_destroy(&workers->mutex);
    return -1;
  }

  if ((workers->iov = (struct iovec*) malloc(options->files.used *
                                            sizeof(struct iovec))) == NULL) {
    pthread_cond_destroy(&workers->cond);
    pthread_mutex_destroy(&workers->mutex);

    return -1;
  }

  workers->iovlen = options->files.used;

  for (j = 0; j < workers->iovlen; j++) {
    workers->iov[j].iov_base = options->files.files[j].data;
    workers->iov[j].iov_len = options->files.files[j].len;
  }

  nworkers = options->nthreads;
  nsends_per_worker = options->number_sends / nworkers;
  diff = options->number_sends - (nworkers * nsends_per_worker);

  *running = 1;
  *ready = 0;

  idx = 0;

  /* Create workers. */
  for (i = 0; i < nworkers; i++) {
    worker = &workers->workers[i];

    worker->id = i;

    worker->options = options;

    /* Create socket. */
    if ((worker->fd = socket_create(addr->sa_family, SOCK_DGRAM)) < 0) {
      workers->nworkers = i;
      workers_free(workers, running);

      return -1;
    }

    /* Create messages to be sent. */
    if ((worker->msgvec =
         (struct mmsghdr**) malloc(workers->iovlen *
                                   sizeof(struct mmsghdr*))) == NULL) {
      close(worker->fd);

      workers->nworkers = i;
      workers_free(workers, running);

      return -1;
    }

    for (j = 0; j < workers->iovlen; j++) {
      if ((worker->msgvec[j] =
           (struct mmsghdr*) malloc(options->number_messages_per_send *
                                    sizeof(struct mmsghdr))) == NULL) {
        for (; j > 0; j--) {
          free(worker->msgvec[j - 1]);
        }

        free(worker->msgvec);

        close(worker->fd);

        workers->nworkers = i;
        workers_free(workers, running);

        return -1;
      }

      for (k = 0; k < options->number_messages_per_send; k++) {
        hdr = &worker->msgvec[j][k];

        hdr->msg_hdr.msg_name = (void*) addr;
        hdr->msg_hdr.msg_namelen = addrlen;
        hdr->msg_hdr.msg_iov = &workers->iov[j];
        hdr->msg_hdr.msg_iovlen = 1;
        hdr->msg_hdr.msg_control = NULL;
        hdr->msg_hdr.msg_controllen = 0;
        hdr->msg_hdr.msg_flags = 0;

        hdr->msg_len = 0;
      }
    }

    worker->idx = (size_t) idx;

    worker->nsends = nsends_per_worker;

    if (diff > 0) {
      worker->nsends++;
      diff--;
    }

    idx = (idx + worker->nsends) % workers->iovlen;

    worker->nmsgs = 0;

    worker->sent = 0;

    worker->error = 0;

    worker->duration = 0;

    worker->mutex = &workers->mutex;
    worker->cond = &workers->cond;

    worker->running = running;
    worker->ready = ready;

    /* Start thread. */
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
      for (j = 0; j < workers->iovlen; j++) {
        free(worker->msgvec[j]);
      }

      free(worker->msgvec);

      close(worker->fd);

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
  size_t j;

  /* Notify threads that they have to stop. */
  pthread_mutex_lock(&workers->mutex);

  *running = 0;
  pthread_cond_broadcast(&workers->cond);

  pthread_mutex_unlock(&workers->mutex);

  /* Join threads. */
  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    pthread_join(worker->thread, NULL);

    for (j = 0; j < workers->iovlen; j++) {
      free(worker->msgvec[j]);
    }

    free(worker->msgvec);

    close(worker->fd);
  }

  free(workers->iov);

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_join(workers_t* workers, uint8_t* running)
{
  worker_t* worker;
  struct timespec ts, timeout;
  sigset_t set;
  unsigned i;
  size_t j;
  int sig;

  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);

  /* Join threads. */
  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    do {
      clock_gettime(CLOCK_REALTIME, &ts);

      if ((ts.tv_nsec += 100000000) >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
      }

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

            for (j = 0; j < workers->iovlen; j++) {
              free(worker->msgvec[j]);
            }

            free(worker->msgvec);

            close(worker->fd);
          }

          free(workers->iov);

          pthread_cond_destroy(&workers->cond);
          pthread_mutex_destroy(&workers->mutex);

          return;
        }
      } else {
        for (j = 0; j < workers->iovlen; j++) {
          free(worker->msgvec[j]);
        }

        free(worker->msgvec);

        close(worker->fd);

        break;
      }
    } while (1);
  }

  free(workers->iov);

  pthread_cond_destroy(&workers->cond);
  pthread_mutex_destroy(&workers->mutex);
}

void workers_statistics(const workers_t* workers)
{
  const worker_t* worker;
  uint64_t count;
  uint64_t sent;
  suseconds_t duration;
  float tx_speed;
  unsigned i;

  count = 0;
  sent = 0;
  duration = 0;
  tx_speed = 0.0;

  for (i = 0; i < workers->nworkers; i++) {
    worker = &workers->workers[i];

    printf("Worker %u: sends: %lu, sent: %lu packet%s (%.2f pkt/s, "
           "%lu byte%s), %s, TX: %s, duration: %s.\n",
           worker->id + 1,
           worker->nsends,
           worker->nmsgs,
           (worker->nmsgs != 1) ? "s" : "",
           ((float) worker->nmsgs * 1000000.0) / (float) worker->duration,
           worker->sent,
           (worker->sent != 1) ? "s" : "",
           worker->error ? "error" : "no errors",
           transfer_speed_as_string(compute_transfer_speed(worker->sent,
                                                           worker->duration)),
           duration_as_string(worker->duration));

    count += worker->nmsgs;
    sent += worker->sent;

    tx_speed += (((float) worker->sent / (float) worker->duration) * 1000000.0);

    if (worker->duration > duration) {
      duration = worker->duration;
    }
  }

  printf("Duration: %s.\n", duration_as_string(duration));

  printf("Sent: %lu packet%s (%.2f pkt/s, %lu byte%s [%s]).\n",
         count,
         (count != 1) ? "s" : "",
         ((float) count * 1000000.0) / (float) duration,
         sent,
         (sent != 1) ? "s" : "",
         transfer_speed_as_string(tx_speed));
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
  stopwatch_t stopwatch;
  struct mmsghdr* msgvec;
  unsigned vlen;
  uint64_t sent;
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

  stopwatch_start(&stopwatch);

  for (sent = 0; (sent < worker->nsends) && (*(worker->running)); sent++) {
    msgvec = worker->msgvec[worker->idx];
    vlen = worker->options->number_messages_per_send;

    do {
      if ((ret = socket_sendmmsg(worker->fd, msgvec, vlen)) < 0) {
        if (errno == EAGAIN) {
          do {
            if ((nevents = socket_wait_writable(worker->fd, 100)) < 0) {
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
        if ((worker->options->delay > 0) &&
            (*(worker->running)) &&
            (sent + 1 < worker->nsends)) {
          delay(worker->options->delay);
        }

        worker->nmsgs += ret;

        for (i = 0; i < ret; i++) {
          worker->sent += msgvec[i].msg_len;
        }

        if ((vlen -= ret) == 0) {
          break;
        }

        msgvec += ret;
      }
    } while (*(worker->running));

    worker->idx = (worker->idx + 1) % worker->options->files.used;
  }

  worker->duration = stopwatch_stop(&stopwatch);

  return 0;
}
