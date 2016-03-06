#include <stdlib.h>
#include <stdio.h>
#include "common/util.h"
#include "common/macros.h"

int parse_uint64(const char* str, uint64_t min, uint64_t max, uint64_t* val)
{
  const char* s;
  uint64_t v, tmp;

  s = str;
  v = 0;

  while (*s) {
    if (!IS_DIGIT(*s)) {
      return -1;
    }

    /* Overflow? */
    if ((tmp = (v * 10) + (*s - '0')) < v) {
      return -1;
    }

    v = tmp;

    s++;
  }

  /* If the string was empty... */
  if (s == str) {
    return -1;
  }

  if ((v < min) || (v > max)) {
    return -1;
  }

  *val = v;

  return 0;
}

int parse_unsigned_list(const char* str,
                        unsigned min,
                        unsigned max,
                        unsigned minval,
                        unsigned maxval,
                        unsigned* numbers,
                        unsigned* count)
{
  unsigned cnt;
  uint64_t n;
  int state;

  cnt = 0;
  n = 0;
  state = 0; /* Initial state. */

  while (*str) {
    switch (state) {
      case 0: /* Initial state. */
        if (IS_DIGIT(*str)) {
          /* Too many numbers? */
          if (cnt == max) {
            return -1;
          }

          /* If the value is too big... */
          if ((n = *str - '0') > maxval) {
            return -1;
          }

          state = 1; /* Parsing number. */
        } else if (!IS_WHITE_SPACE(*str)) {
          return -1;
        }

        break;
      case 1: /* Parsing number. */
        if (IS_DIGIT(*str)) {
          /* If the value is too big... */
          if ((n = (n * 10) + (*str - '0')) > maxval) {
            return -1;
          }
        } else if (IS_WHITE_SPACE(*str)) {
          /* If the value is too small... */
          if (n < minval) {
            return -1;
          }

          numbers[cnt++] = (unsigned) n;

          state = 2; /* After number. */
        } else if ((*str == ',') || (*str == ';')) {
          /* If the value is too small... */
          if (n < minval) {
            return -1;
          }

          numbers[cnt++] = (unsigned) n;

          state = 3; /* After separator. */
        } else {
          return -1;
        }

        break;
      case 2: /* After number. */
        if ((*str == ',') || (*str == ';')) {
          state = 3; /* After separator. */
        } else if (!IS_WHITE_SPACE(*str)) {
          return -1;
        }

        break;
      default: /* After separator. */
        if (IS_DIGIT(*str)) {
          /* Too many numbers? */
          if (cnt == max) {
            return -1;
          }

          /* If the value is too big... */
          if ((n = *str - '0') > maxval) {
            return -1;
          }

          state = 1; /* Parsing number. */
        } else if (!IS_WHITE_SPACE(*str)) {
          return -1;
        }
    }

    str++;
  }

  switch (state) {
    case 0: /* Initial state. */
      break;
    case 1: /* Parsing number. */
      /* If the value is too small... */
      if (n < minval) {
        return -1;
      }

      numbers[cnt++] = (unsigned) n;

      break;
    case 2: /* After number. */
      break;
    default:
      return -1;
  }

  /* Too few numbers? */
  if (cnt < min) {
    return -1;
  }

  *count = cnt;

  return 0;
}

const char* duration_as_string(suseconds_t usec)
{
  static char buf[64];
  suseconds_t sec, msec;

  if (usec < 1000) {
    snprintf(buf,
             sizeof(buf),
             "%lu microsecond%s",
             usec,
             (usec != 1) ? "s" : "");
  } else if (usec < 1000000) {
    msec = usec / 1000;
    usec %= 1000;

    snprintf(buf,
             sizeof(buf),
             "%lu.%03lu millisecond%s",
             msec,
             usec,
             ((msec != 1) || (usec != 0)) ? "s" : "");
  } else {
    sec = usec / 1000000;
    msec = (usec % 1000000) / 1000;

    snprintf(buf,
             sizeof(buf),
             "%lu.%03lu second%s",
             sec,
             msec,
             ((sec != 1) || (msec != 0)) ? "s" : "");
  }

  return buf;
}

const char* transfer_speed_as_string(float bytes_per_second)
{
  static char buf[2][64];
  static size_t idx = 1;

  idx = !idx;

  if (bytes_per_second < 1000.0) {
    snprintf(buf[idx], sizeof(buf[idx]), "%.2f bytes/s", bytes_per_second);
  } else if (bytes_per_second < 1000000.0) {
    snprintf(buf[idx],
             sizeof(buf[idx]),
             "%.2f KB/s",
             bytes_per_second / 1000.0);
  } else if (bytes_per_second < 1000000000.0) {
    snprintf(buf[idx],
             sizeof(buf[idx]),
             "%.2f MB/s",
             bytes_per_second / 1000000.0);
  } else {
    snprintf(buf[idx],
             sizeof(buf[idx]),
             "%.2f GB/s",
             bytes_per_second / 1000000000.0);
  }

  return buf[idx];
}
