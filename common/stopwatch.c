#include <stdlib.h>
#include "common/stopwatch.h"

suseconds_t stopwatch_stop(stopwatch_t* stopwatch)
{
  time_t sec;

  gettimeofday(&stopwatch->end, NULL);

  sec = stopwatch->end.tv_sec - stopwatch->start.tv_sec;

  return (sec * 1000000) + stopwatch->end.tv_usec - stopwatch->start.tv_usec;
}
