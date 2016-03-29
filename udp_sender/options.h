#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>
#include <limits.h>
#include "common/processors.h"
#include "common/file.h"

#define MAX_MESSAGE_SIZE          USHRT_MAX

#define MIN_MESSAGES_PER_SEND     1
#define MAX_MESSAGES_PER_SEND     UINT_MAX
#define DEFAULT_MESSAGES_PER_SEND 1

#define MIN_SENDS                 1llu
#define MAX_SENDS                 ULLONG_MAX
#define DEFAULT_SENDS             1llu

#define MIN_THREADS               1
#define MAX_THREADS               32
#define DEFAULT_THREADS           1

typedef struct {
  unsigned number_messages_per_send;
  uint64_t number_sends;
  unsigned nthreads;

  unsigned processors[MAX_PROCESSORS];
  unsigned nprocessors;

  files_t files;
} options_t;

int options_parse(int argc, const char** argv, options_t* options);
void options_free(options_t* options);

#endif /* OPTIONS_H */
