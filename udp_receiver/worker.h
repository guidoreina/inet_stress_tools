#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <pthread.h>
#include <sys/uio.h>
#include "udp_receiver/options.h"
#include "common/socket.h"

typedef struct {
  /* Thread id. */
  pthread_t thread;

  /* Thread number. */
  unsigned id;

  const options_t* options;

  /* Socket. */
  int fd;

  /* Buffer for holding received data. */
  uint8_t* buf;

  struct iovec* iov;

  /* Messages to be received. */
  struct mmsghdr* msgvec;

  /* Number of messages received by this worker. */
  uint64_t nmsgs;

  /* Number of bytes received by this worker. */
  uint64_t received;

  /* Error while receiving? */
  int error;

  pthread_mutex_t* mutex;
  pthread_cond_t* cond;

  const uint8_t* running;
  const uint8_t* ready;
} worker_t;

typedef struct {
  worker_t workers[MAX_THREADS];
  unsigned nworkers;

  /* Socket. */
  int fd;

  pthread_mutex_t mutex;
  pthread_cond_t cond;
} workers_t;

int workers_create(const struct sockaddr* addr,
                   socklen_t addrlen,
                   const options_t* options,
                   uint8_t* running,
                   uint8_t* ready,
                   workers_t* workers);

void workers_free(workers_t* workers,
                  const options_t* options,
                  uint8_t* running);

void workers_join(workers_t* workers,
                  const options_t* options,
                  uint8_t* running);

void workers_statistics(const workers_t* workers);

#endif /* WORKER_H */
