#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "libevt/runtime/clock.h"
#include "libevt/runtime/context.h"
#include "libevt/runtime/event.h"
#include "libevt/runtime/queue.h"
#include "libevt/scheduler/engine.h"

#define ENGINE_CAP 64

Engine *engine_create(void (*idle_fn)(void *arg), void *idle_arg, int idle_ms)
{
  Engine *engine = malloc(sizeof(Engine));
  if (!engine)
    return NULL;
  engine->q = queue_create(ENGINE_CAP);
  if (!engine->q)
  {
    LOG("[engine:create] -> unable to initialize queue for event loop.\n");
    free(engine);
    return NULL;
  }

  evt_mutex_init(&engine->mu);
  evt_cond_init(&engine->cv);
  engine->clock = 0;
  engine->idle_fn = idle_fn;
  engine->idle_arg = idle_arg;
  engine->idle_ms = idle_ms < 0 ? 0 : idle_ms;
  engine->stopped = false;
  LOG("[engine:create] -> ready.\n");
  return engine;
}

void engine_destroy(Engine *engine)
{
  if (!engine)
    return;
  engine_stop(engine);
  Event *evt = NULL;
  for (queue_pop(engine->q, &evt); evt; queue_pop(engine->q, &evt))
    event_destroy(evt);
  queue_destroy(engine->q);
  evt_cond_destroy(&engine->cv);
  evt_mutex_destroy(&engine->mu);
  free(engine);
  LOG("[engine:destroy] -> pending events freed, engine freed.\n");
}

uint64_t engine_add(Engine *engine, Context *ctx, int priority)
{
  if (!(engine && ctx))
    return 0;
  evt_mutex_lock(&engine->mu);
  if (engine->stopped)
  {
    evt_mutex_unlock(&engine->mu);
    return 0;
  }

  const uint64_t id = ++engine->clock;
  Event *evt = event_create(id, priority > 0 ? priority : 0, ctx);
  if (!evt)
  {
    evt_mutex_unlock(&engine->mu);
    return 0;
  }

  queue_push(engine->q, evt);
  evt_cond_signal(&engine->cv);
  evt_mutex_unlock(&engine->mu);
  return id;
}

bool engine_pump(Engine *engine, int ms)
{
  if (!engine)
    return false;
  Event *evt = NULL;
  evt_mutex_lock(&engine->mu);
  if (ms < 0)
  {
    while (!engine->stopped && !queue_peek(engine->q))
      evt_cond_wait(&engine->cv, &engine->mu);
  }
  else if (ms > 0)
  {
    const uint64_t until = clock_step() + (uint64_t)ms * 1000000ULL;
    for (;;)
    {
      if (engine->stopped || queue_peek(engine->q))
        break;
      const uint64_t now = clock_step();
      if (now >= until)
        break;
      evt_cond_wait_ns(&engine->cv, &engine->mu, until - now);
    }
  }

  if (!engine->stopped)
    queue_pop(engine->q, &evt);
  evt_mutex_unlock(&engine->mu);
  if (!evt)
    return false;
  event_execute(evt);
  event_destroy(evt);
  return true;
}

void engine_run(Engine *engine)
{
  if (!engine)
    return;
  LOG("[engine:run] -> starting event loop...\n");
  while (!engine_stopped(engine))
  {
    if (engine_pump(engine, engine->idle_fn ? engine->idle_ms : -1))
      continue;
    if (engine->idle_fn && !engine_stopped(engine))
      engine->idle_fn(engine->idle_arg);
  }
  LOG("[engine:run] -> stopped.\n");
}

void engine_stop(Engine *engine)
{
  if (!engine)
    return;
  evt_mutex_lock(&engine->mu);
  engine->stopped = true;
  evt_cond_broadcast(&engine->cv);
  evt_mutex_unlock(&engine->mu);
}

bool engine_stopped(Engine *engine)
{
  if (!engine)
    return true;
  evt_mutex_lock(&engine->mu);
  const bool s = engine->stopped;
  evt_mutex_unlock(&engine->mu);
  return s;
}

size_t engine_pending(Engine *engine)
{
  if (!engine)
    return 0;
  evt_mutex_lock(&engine->mu);
  const size_t n = engine->q->len;
  evt_mutex_unlock(&engine->mu);
  return n;
}
