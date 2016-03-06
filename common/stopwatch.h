#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <sys/time.h>

typedef struct {
  struct timeval start;
  struct timeval end;
} stopwatch_t;

static inline void stopwatch_start(stopwatch_t* stopwatch)
{
  gettimeofday(&stopwatch->start, NULL);
}

suseconds_t stopwatch_stop(stopwatch_t* stopwatch);

#endif /* STOPWATCH_H */
