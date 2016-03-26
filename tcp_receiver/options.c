#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include "tcp_receiver/options.h"
#include "common/util.h"

int options_parse(int argc, const char** argv, options_t* options)
{
  uint64_t val;
  int i;

  /* Initialize options to default values. */
  options->nthreads = DEFAULT_THREADS;
  options->receive = DEFAULT_RECEIVE;
  options->number_connection_loops = DEFAULT_LOOPS;
  options->client_sends_first = CLIENT_SENDS_FIRST;
  options->set_read_write_event = SET_READ_WRITE_EVENT;

#ifdef SO_REUSEPORT
  options->single_listener = SINGLE_LISTENER;
#else
  options->single_listener = 1;
#endif

  options->single_epoll_fd = SINGLE_EPOLL_FD;
  options->nprocessors = 0;

  files_init(&options->files);

  /* Last parameter is not an option. */
  argc--;

  i = 1;
  while (i < argc) {
    if (strcasecmp(argv[i], "--threads") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1], MIN_THREADS, MAX_THREADS, &val) < 0) {
        files_free(&options->files);
        return -1;
      }

      options->nthreads = (unsigned) val;

      i += 2;
    } else if (strcasecmp(argv[i], "--receive") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1], MIN_RECEIVE, MAX_RECEIVE, &val) < 0) {
        files_free(&options->files);
        return -1;
      }

      options->receive = (unsigned) val;

      i += 2;
    } else if (strcasecmp(argv[i], "--connection-loops") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_LOOPS,
                       MAX_LOOPS,
                       &options->number_connection_loops) < 0) {
        files_free(&options->files);
        return -1;
      }

      i += 2;
    } else if (strcasecmp(argv[i], "--client-sends-first") == 0) {
      options->client_sends_first = 1;

      i++;
    } else if (strcasecmp(argv[i], "--server-sends-first") == 0) {
      options->client_sends_first = 0;

      i++;
    } else if (strcasecmp(argv[i], "--set-read-write-event") == 0) {
      options->set_read_write_event = 1;

      i++;
    } else if (strcasecmp(argv[i], "--do-not-set-read-write-event") == 0) {
      options->set_read_write_event = 0;

      i++;
#ifdef SO_REUSEPORT
    } else if (strcasecmp(argv[i], "--single-listener") == 0) {
      options->single_listener = 1;

      i++;
    } else if (strcasecmp(argv[i], "--one-listener-per-thread") == 0) {
      options->single_listener = 0;

      i++;
#endif /* SO_REUSEPORT */
    } else if (strcasecmp(argv[i], "--single-epoll-fd") == 0) {
      options->single_epoll_fd = 1;

      i++;
    } else if (strcasecmp(argv[i], "--one-epoll-fd-per-thread") == 0) {
      options->single_epoll_fd = 0;

      i++;
    } else if (strcasecmp(argv[i], "--processors") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_processors(argv[i + 1],
                           options->processors,
                           &options->nprocessors) < 0) {
        files_free(&options->files);
        return -1;
      }

      i += 2;
    } else if (strcasecmp(argv[i], "--file") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (files_add(&options->files, argv[i + 1]) < 0) {
        files_free(&options->files);
        return -1;
      }

      i += 2;
    } else {
      files_free(&options->files);
      return -1;
    }
  }

  if (options->files.used == 0) {
    options->client_sends_first = 1;
  }

#ifdef SO_REUSEPORT
  if ((!options->single_listener) && (options->single_epoll_fd)) {
    fprintf(stderr, "Incompatible options --one-listener-per-thread and "
                    "--single-epoll-fd.\n\n");

    return -1;
  }
#endif /* SO_REUSEPORT */

  return 0;
}

void options_free(options_t* options)
{
  files_free(&options->files);
}
