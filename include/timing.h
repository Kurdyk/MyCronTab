#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

typedef struct timing_struct {
  uint64_t minutes;
  uint32_t hours;
  uint8_t daysofweek;
} TIMING;

#endif // TIMING_H
