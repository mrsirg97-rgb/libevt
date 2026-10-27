#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE evt_thread_t;
#else
#include <pthread.h>
typedef pthread_t evt_thread_t;
#endif

#include "libevt/runtime/thread.h"

typedef struct Context Context;
typedef struct Engine Engine;

typedef struct Scheduler
{
  Engine *engine;
  evt_thread_t engine_th;
  bool engine_th_spawned;
  evt_mutex_t spawn_mu;
} Scheduler;

Scheduler *scheduler_create(void (*idle_fn)(void *arg), void *idle_arg, int idle_ms);
void scheduler_destroy(Scheduler *s);

int scheduler_start_loop(Scheduler *s);
uint64_t scheduler_schedule_evt(Scheduler *s, Context *ctx, int priority);
void scheduler_stop_loop(Scheduler *s);

#endif

/*
Scheduler
  the engine on a thread of its own: the client facing event loop for callers that do not run one themselves (an
  engine's own thread calls engine_run instead - a serve loop that is the consumer).

scheduler_start_loop
  starts the engine's run on its thread.  return values are:
    -1 -> err: engine does not exist
    -2 -> err: engine already started, thread exists
    -3 -> err: engine could not spawn thread
    0  -> success: engine started

scheduler_schedule_evt
  any thread (engine_add): the context runs on the engine's thread at its priority.  returns the event's id, 0:
  refused.

scheduler_stop_loop
  stops the engine and joins its thread.  an engine runs once: a stopped one is not started again.
*/
