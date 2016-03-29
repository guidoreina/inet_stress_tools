#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/uio.h>
#include "udp_sender/options.h"
#include "common/socket.h"

typedef struct {
  /* Thread id. */
  pthread_t thread;

  /* Thread number. */
  unsigned id;

  const options_t* options;

  /* Socket. */
  int fd;

  /* Array of messages to be sent. */
  struct mmsghdr** msgvec;

  /* Index of the next messages to be sent. */
  size_t idx;

  /* Number of sends this worker will perform. */
  uint64_t nsends;

  /* Number of messages sent by this worker. */
  uint64_t nmsgs;

  /* Number of bytes sent by this worker. */
  uint64_t sent;

  /* Error while sending? */
  int error;

  /* Duration in microseconds. */
  suseconds_t duration;

  pthread_mutex_t* mutex;
  pthread_cond_t* cond;

  const uint8_t* running;
  const uint8_t* ready;
} worker_t;

typedef struct {
  worker_t workers[MAX_THREADS];
  unsigned nworkers;

  struct iovec* iov;
  size_t iovlen;

  pthread_mutex_t mutex;
  pthread_cond_t cond;
} workers_t;

int workers_create(const struct sockaddr* addr,
                   socklen_t addrlen,
                   const options_t* options,
                   uint8_t* running,
                   uint8_t* ready,
                   workers_t* workers);

void workers_free(workers_t* workers, uint8_t* running);
void workers_join(workers_t* workers, uint8_t* running);
void workers_statistics(const workers_t* workers);

#endif /* WORKER_H */
