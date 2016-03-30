#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>
#include <limits.h>
#include "common/processors.h"

#define MAX_MESSAGE_SIZE             USHRT_MAX

#define MIN_MESSAGES_PER_RECEIVE     1
#define MAX_MESSAGES_PER_RECEIVE     UINT_MAX
#define DEFAULT_MESSAGES_PER_RECEIVE 1

#define MIN_THREADS                  1
#define MAX_THREADS                  32
#define DEFAULT_THREADS              1

#define SINGLE_RECEIVER_SOCKET       0

typedef struct {
  unsigned number_messages_per_receive;
  unsigned nthreads;
  int single_receiver_socket;

  unsigned processors[MAX_PROCESSORS];
  unsigned nprocessors;
} options_t;

int options_parse(int argc, const char** argv, options_t* options);

#endif /* OPTIONS_H */
