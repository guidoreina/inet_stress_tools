#include <stdlib.h>
#include <string.h>
#include "udp_sender/options.h"
#include "common/util.h"
#include "common/macros.h"

int options_parse(int argc, const char** argv, options_t* options)
{
  uint64_t val;
  file_t* file;
  int i;

  /* Initialize options to default values. */
  options->number_messages_per_send = DEFAULT_MESSAGES_PER_SEND;
  options->number_sends = DEFAULT_SENDS;
  options->delay = DEFAULT_DELAY;
  options->nthreads = DEFAULT_THREADS;
  options->nprocessors = 0;

  files_init(&options->files);

  /* Last parameter is not an option. */
  argc--;

  i = 1;
  while (i < argc) {
    if (strcasecmp(argv[i], "--messages-per-send") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_MESSAGES_PER_SEND,
                       MAX_MESSAGES_PER_SEND,
                       &val) < 0) {
        files_free(&options->files);
        return -1;
      }

      options->number_messages_per_send = (unsigned) val;

      i += 2;
    } else if (strcasecmp(argv[i], "--sends") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_SENDS,
                       MAX_SENDS,
                       &options->number_sends) < 0) {
        files_free(&options->files);
        return -1;
      }

      i += 2;
    } else if (strcasecmp(argv[i], "--delay") == 0) {
      /* Last parameter? */
      if (i + 1 == argc) {
        files_free(&options->files);
        return -1;
      }

      if (parse_uint64(argv[i + 1],
                       MIN_DELAY,
                       MAX_DELAY,
                       &val) < 0) {
        files_free(&options->files);
        return -1;
      }

      options->delay = (unsigned) val;

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

      if (options->files.used > 0) {
        /* If the file is too big... */
        file = &options->files.files[options->files.used - 1];
        if (file->len > MAX_MESSAGE_SIZE) {
          file->len = MAX_MESSAGE_SIZE;
        }
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

  options->number_sends = MAX(options->number_sends, options->files.used);
  options->nthreads = MIN(options->nthreads, options->number_sends);

  return 0;
}

void options_free(options_t* options)
{
  files_free(&options->files);
}
