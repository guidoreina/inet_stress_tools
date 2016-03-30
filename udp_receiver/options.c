#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "udp_receiver/options.h"
#include "common/util.h"

int options_parse(int argc, const char** argv, options_t* options)
{
  uint64_t val;
  int i;

  /* Initialize options to default values. */
  options->number_messages_per_receive = DEFAULT_MESSAGES_PER_RECEIVE;
  options->nthreads = DEFAULT_THREADS;

#ifdef SO_REUSEPORT
  options->single_receiver_socket = SINGLE_RECEIVER_SOCKET;
#else
  options->single_receiver_socket = 1;
#endif

  options->nprocessors = 0;

  /* Last parameter is not an option. */
  argc--;

  i = 1;
  while (i < argc) {
    if (strcasecmp(argv[i], "--messages-per-receive") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_MESSAGES_PER_RECEIVE,
                       MAX_MESSAGES_PER_RECEIVE,
                       &val) < 0) {
        return -1;
      }

      options->number_messages_per_receive = (unsigned) val;

      i += 2;
    } else if (strcasecmp(argv[i], "--threads") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        return -1;
      }

      if (parse_uint64(argv[i + 1], MIN_THREADS, MAX_THREADS, &val) < 0) {
        return -1;
      }

      options->nthreads = (unsigned) val;

      i += 2;
#ifdef SO_REUSEPORT
    } else if (strcasecmp(argv[i], "--single-receiver-socket") == 0) {
      options->single_receiver_socket = 1;

      i++;
    } else if (strcasecmp(argv[i], "--one-receiver-socket-per-thread") == 0) {
      options->single_receiver_socket = 0;

      i++;
#endif /* SO_REUSEPORT */
    } else if (strcasecmp(argv[i], "--processors") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        return -1;
      }

      if (parse_processors(argv[i + 1],
                           options->processors,
                           &options->nprocessors) < 0) {
        return -1;
      }

      i += 2;
    } else {
      return -1;
    }
  }

  return 0;
}
