#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>

static inline uint64_t clock_step(void)
{
  LARGE_INTEGER f, c;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return (uint64_t)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
}

#else
#include <time.h>

static inline uint64_t clock_step(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif

#endif

/*
Clock
  clock_step: monotonic nanoseconds, for deadlines (engine_pump, the idle tick) and for callers' own timing.
  not an event's id: two calls can return the same value, and an id must be unique - the engine numbers its events
  with a counter of its own.
*/
