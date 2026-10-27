#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libevt/runtime/thread.h"

typedef struct Context Context;
typedef struct Queue Queue;

typedef struct Engine
{
  Queue *q;
  evt_mutex_t mu;
  evt_cond_t cv;
  uint64_t clock;
  void (*idle_fn)(void *arg);
  void *idle_arg;
  int idle_ms;
  bool stopped;
} Engine;

Engine *engine_create(void (*idle_fn)(void *arg), void *idle_arg, int idle_ms);
void engine_destroy(Engine *engine);

uint64_t engine_add(Engine *engine, Context *ctx, int priority);
bool engine_pump(Engine *engine, int ms);
void engine_run(Engine *engine);
void engine_stop(Engine *engine);
bool engine_stopped(Engine *engine);
size_t engine_pending(Engine *engine);

#endif

/*
Engine
  the event loop: a 4-ary max heap of events by (priority, id), run one at a time by the one thread that calls
  engine_run (or engine_pump) - the consumer, which owns whatever the events touch.  nothing polls: the consumer waits
  on the condition until an event comes.

engine_create
  idle_fn (optional) runs on the consumer when no event came for idle_ms (0: whenever the queue is empty).

engine_add
  any thread.  numbers the event (a counter: unique, increasing - the tie between equal priorities is the order they
  were added in) and wakes the consumer.  a priority below 0 is 0.  returns the id, or 0: refused (stopped, no
  context) - the context is the caller's then.  an added context is the engine's: destroyed once it ran, or at
  engine_destroy if it never did.

engine_pump
  the consumer: one event, waiting up to ms for it (< 0: until one comes or the engine stops).  false: none came.
  an event may pump too - the consumer waiting inside an event for the ones it needs (a prompt read's gap: the
  answers' tokens come back as events while the read is paused).

engine_run
  the consumer: pump until engine_stop, idle_fn when the wait ends with none.

engine_stop
  any thread.  the run ends after the event it is in; adds are refused from here on.

engine_destroy
  after the run: the events never run have their contexts destroyed, not resolved.
*/
