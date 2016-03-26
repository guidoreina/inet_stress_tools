#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>
#include <limits.h>
#include "common/processors.h"
#include "common/file.h"

#define MIN_THREADS          1
#define MAX_THREADS          32
#define DEFAULT_THREADS      1

#define MIN_RECEIVE          1
#define MAX_RECEIVE          (32 * 1024)
#define DEFAULT_RECEIVE      DUMMY_FILESIZE

#define MIN_LOOPS            1llu
#define MAX_LOOPS            ULLONG_MAX
#define DEFAULT_LOOPS        1llu

#define CLIENT_SENDS_FIRST   1
#define SET_READ_WRITE_EVENT 1
#define SINGLE_LISTENER      0
#define SINGLE_EPOLL_FD      0

typedef struct {
  unsigned nthreads;
  unsigned receive;
  uint64_t number_connection_loops;
  int client_sends_first;
  int set_read_write_event;
  int single_listener;
  int single_epoll_fd;

  unsigned processors[MAX_PROCESSORS];
  unsigned nprocessors;

  files_t files;
} options_t;

int options_parse(int argc, const char** argv, options_t* options);
void options_free(options_t* options);

#endif /* OPTIONS_H */
