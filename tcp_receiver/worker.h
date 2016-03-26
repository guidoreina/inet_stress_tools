#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <pthread.h>
#include <sys/epoll.h>
#include "tcp_receiver/options.h"
#include "tcp_receiver/connection.h"
#include "common/socket.h"

typedef struct {
  /* Thread id. */
  pthread_t thread;

  /* Thread number. */
  unsigned id;

  const options_t* options;

  /* Array of connections indexed by file descriptor (shared among all the
     workers). */
  connections_t* connections;

  /* Number of connections this worker has accepted. */
  unsigned nconnections;

  /* Listener socket. */
  int listener;

  /* epoll file descriptor. */
  int epfd;

  struct epoll_event* events;
  int* fds;

  /* Number of bytes sent by this worker. */
  uint64_t sent;

  /* Number of bytes received by this worker. */
  uint64_t received;

  /* Number of failed connections. */
  unsigned errors;

  pthread_mutex_t* mutex;
  pthread_cond_t* cond;

  const uint8_t* running;
  const uint8_t* ready;
} worker_t;

typedef struct {
  worker_t workers[MAX_THREADS];
  unsigned nworkers;

  /* Listener socket. */
  int listener;

  /* epoll file descriptor. */
  int epfd;

  pthread_mutex_t mutex;
  pthread_cond_t cond;
} workers_t;

int workers_create(const struct sockaddr* addr,
                   socklen_t addrlen,
                   const options_t* options,
                   connections_t* connections,
                   uint8_t* running,
                   uint8_t* ready,
                   workers_t* workers);

void workers_free(workers_t* workers,
                  const options_t* options,
                  uint8_t* running);

void workers_join(workers_t* workers,
                  const options_t* options,
                  connections_t* connections,
                  uint8_t* running);

void workers_statistics(const workers_t* workers);

#endif /* WORKER_H */
