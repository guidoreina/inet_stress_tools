#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common/processors.h"
#include "common/util.h"

int parse_processors(const char* str,
                     unsigned* processors,
                     unsigned* nprocessors)
{
  long n, i;
  unsigned idx;

  /* Get the number of processors currently online. */
  if ((n = sysconf(_SC_NPROCESSORS_ONLN)) < 0) {
    return -1;
  }

  if (strcasecmp(str, "even") == 0) {
    idx = 0;
    for (i = 0; (i < n) && (i < MAX_PROCESSORS); i += 2) {
      processors[idx++] = i;
    }

    *processors = idx;
  } else if (strcasecmp(str, "odd") == 0) {
    idx = 0;
    for (i = 1; (i < n) && (i < MAX_PROCESSORS); i += 2) {
      processors[idx++] = i;
    }

    *processors = idx;
  } else {
    return parse_unsigned_list(str,
                               1,
                               MAX_PROCESSORS,
                               0,
                               n - 1,
                               processors,
                               nprocessors);
  }

  return 0;
}
