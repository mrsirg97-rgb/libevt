#ifndef THREAD_H
#define THREAD_H

#include <stdbool.h>
#include <stdint.h>

#include "libevt/runtime/clock.h"

#if defined(_WIN32)
#include <windows.h>

typedef SRWLOCK evt_mutex_t;
typedef CONDITION_VARIABLE evt_cond_t;

static inline void evt_mutex_init(evt_mutex_t *m) { InitializeSRWLock(m); }
static inline void evt_mutex_destroy(evt_mutex_t *m) { (void)m; }
static inline void evt_mutex_lock(evt_mutex_t *m) { AcquireSRWLockExclusive(m); }
static inline void evt_mutex_unlock(evt_mutex_t *m) { ReleaseSRWLockExclusive(m); }
static inline void evt_cond_init(evt_cond_t *c) { InitializeConditionVariable(c); }
static inline void evt_cond_destroy(evt_cond_t *c) { (void)c; }
static inline void evt_cond_signal(evt_cond_t *c) { WakeConditionVariable(c); }
static inline void evt_cond_broadcast(evt_cond_t *c) { WakeAllConditionVariable(c); }
static inline void evt_cond_wait(evt_cond_t *c, evt_mutex_t *m) { SleepConditionVariableSRW(c, m, INFINITE, 0); }
static inline void evt_cond_wait_ns(evt_cond_t *c, evt_mutex_t *m, uint64_t ns)
{
  DWORD ms = (DWORD)((ns + 999999ULL) / 1000000ULL);
  SleepConditionVariableSRW(c, m, ms, 0);
}

#else
#include <errno.h>
#include <pthread.h>
#include <time.h>

typedef pthread_mutex_t evt_mutex_t;
typedef pthread_cond_t evt_cond_t;

static inline void evt_mutex_init(evt_mutex_t *m) { pthread_mutex_init(m, NULL); }
static inline void evt_mutex_destroy(evt_mutex_t *m) { pthread_mutex_destroy(m); }
static inline void evt_mutex_lock(evt_mutex_t *m) { pthread_mutex_lock(m); }
static inline void evt_mutex_unlock(evt_mutex_t *m) { pthread_mutex_unlock(m); }
static inline void evt_cond_init(evt_cond_t *c)
{
#if defined(__APPLE__)
  pthread_cond_init(c, NULL);
#else
  pthread_condattr_t a;
  pthread_condattr_init(&a);
  pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
  pthread_cond_init(c, &a);
  pthread_condattr_destroy(&a);
#endif
}
static inline void evt_cond_destroy(evt_cond_t *c) { pthread_cond_destroy(c); }
static inline void evt_cond_signal(evt_cond_t *c) { pthread_cond_signal(c); }
static inline void evt_cond_broadcast(evt_cond_t *c) { pthread_cond_broadcast(c); }
static inline void evt_cond_wait(evt_cond_t *c, evt_mutex_t *m) { pthread_cond_wait(c, m); }
static inline void evt_cond_wait_ns(evt_cond_t *c, evt_mutex_t *m, uint64_t ns)
{
  struct timespec ts;
#if defined(__APPLE__)
  ts.tv_sec = (time_t)(ns / 1000000000ULL);
  ts.tv_nsec = (long)(ns % 1000000000ULL);
  pthread_cond_timedwait_relative_np(c, m, &ts);
#else
  const uint64_t at = clock_step() + ns;
  ts.tv_sec = (time_t)(at / 1000000000ULL);
  ts.tv_nsec = (long)(at % 1000000000ULL);
  pthread_cond_timedwait(c, m, &ts);
#endif
}

#endif

#endif

/*
Thread
  the mutex and condition the engine waits on, one shape on every platform:
    POSIX: pthreads, the condition on CLOCK_MONOTONIC (macOS: its relative wait)
    Windows: SRWLOCK and CONDITION_VARIABLE

evt_cond_wait_ns
  waits at most ns for a signal; a wakeup may come early or spuriously, so a caller waits again on its own predicate
  and deadline (engine_pump does).
*/
