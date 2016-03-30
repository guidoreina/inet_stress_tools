#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <sys/time.h>

int parse_uint64(const char* str, uint64_t min, uint64_t max, uint64_t* val);
int parse_unsigned_list(const char* str,
                        unsigned min,
                        unsigned max,
                        unsigned minval,
                        unsigned maxval,
                        unsigned* numbers,
                        unsigned* count);

const char* duration_as_string(suseconds_t usec);

static inline float compute_transfer_speed(uint64_t transferred,
                                           suseconds_t usec)
{
  return (((float) transferred / (float) usec) * 1000000.0);
}

const char* transfer_speed_as_string(float bytes_per_second);

int delay(unsigned usec);

#endif /* UTIL_H */
