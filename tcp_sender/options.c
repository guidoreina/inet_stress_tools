#include <stdlib.h>
#include <string.h>
#include "tcp_sender/options.h"
#include "common/util.h"
#include "common/macros.h"

int options_parse(int argc, const char** argv, options_t* options)
{
  uint64_t val;
  int i;

  /* Initialize options to default values. */
  options->nconnections = DEFAULT_CONNECTIONS;
  options->nthreads = DEFAULT_THREADS;
  options->receive = DEFAULT_RECEIVE;
  options->number_thread_loops = DEFAULT_LOOPS;
  options->number_connection_loops = DEFAULT_LOOPS;
  options->client_sends_first = CLIENT_SENDS_FIRST;
  options->set_read_write_event = SET_READ_WRITE_EVENT;
  options->nprocessors = 0;

  files_init(&options->files);

  /* Last parameter is not an option. */
  argc--;

  i = 1;
  while (i < argc) {
    if (strcasecmp(argv[i], "--connections") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_CONNECTIONS,
                       MAX_CONNECTIONS,
                       &val) < 0) {
        files_free(&options->files);
        return -1;
      }

      options->nconnections = (unsigned) val;

      i += 2;
    } else if (strcasecmp(argv[i], "--threads") == 0) {
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
    } else if (strcasecmp(argv[i], "--thread-loops") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_LOOPS,
                       MAX_LOOPS,
                       &options->number_thread_loops) < 0) {
        files_free(&options->files);
        return -1;
      }

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

  /* If no files have been specified... */
  if (options->files.used == 0) {
    if (files_add_dummy(&options->files) < 0) {
      files_free(&options->files);
      return -1;
    }
  }

  options->nthreads = MIN(options->nthreads, options->nconnections);

  if (options->receive == 0) {
    options->client_sends_first = 1;
  }

  return 0;
}

void options_free(options_t* options)
{
  files_free(&options->files);
}
