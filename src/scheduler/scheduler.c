#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "libevt/runtime/context.h"
#include "libevt/scheduler/engine.h"
#include "libevt/scheduler/scheduler.h"

static bool scheduler_engine_exists(Scheduler *s);

#if defined(_WIN32)
static DWORD WINAPI scheduler_loop_thread(LPVOID arg)
{
  engine_run((Engine *)arg);
  return 0;
}
#else
static void *scheduler_loop_thread(void *arg)
{
  engine_run((Engine *)arg);
  return NULL;
}
#endif

Scheduler *scheduler_create(void (*idle_fn)(void *arg), void *idle_arg, int idle_ms)
{
  Scheduler *s = malloc(sizeof(Scheduler));
  if (!s)
    return NULL;
  s->engine = engine_create(idle_fn, idle_arg, idle_ms);
  if (!scheduler_engine_exists(s))
  {
    LOG("[scheduler:create] -> scheduler failed to create engine, freeing resources.\n");
    free(s);
    return NULL;
  }

  s->engine_th_spawned = false;
  evt_mutex_init(&s->spawn_mu);
  LOG("[scheduler:create] -> scheduler initialized, ready to start.\n");
  return s;
}

void scheduler_destroy(Scheduler *s)
{
  if (!s)
    return;
  scheduler_stop_loop(s);
  if (scheduler_engine_exists(s))
    engine_destroy(s->engine);
  evt_mutex_destroy(&s->spawn_mu);
  free(s);
  LOG("[scheduler:destroy] -> engine resources and scheduler freed.\n");
}

int scheduler_start_loop(Scheduler *s)
{
  if (!scheduler_engine_exists(s))
    return -1;
  evt_mutex_lock(&s->spawn_mu);
  if (s->engine_th_spawned)
  {
    evt_mutex_unlock(&s->spawn_mu);
    return -2;
  }

#if defined(_WIN32)
  s->engine_th = CreateThread(NULL, 0, scheduler_loop_thread, s->engine, 0, NULL);
  const bool ok = s->engine_th != NULL;
#else
  const bool ok = pthread_create(&s->engine_th, NULL, scheduler_loop_thread, s->engine) == 0;
#endif
  if (!ok)
  {
    evt_mutex_unlock(&s->spawn_mu);
    LOG("[scheduler:start_loop] -> failed to start engine thread.\n");
    return -3;
  }

  s->engine_th_spawned = true;
  evt_mutex_unlock(&s->spawn_mu);
  LOG("[scheduler:start_loop] -> engine thread started, scheduler ready.\n");
  return 0;
}

uint64_t scheduler_schedule_evt(Scheduler *s, Context *ctx, int priority)
{
  if (!(scheduler_engine_exists(s) && ctx))
    return 0;
  return engine_add(s->engine, ctx, priority);
}

void scheduler_stop_loop(Scheduler *s)
{
  if (!s)
    return;
  evt_mutex_lock(&s->spawn_mu);
  if (!s->engine_th_spawned)
  {
    evt_mutex_unlock(&s->spawn_mu);
    return;
  }

  engine_stop(s->engine);
#if defined(_WIN32)
  WaitForSingleObject(s->engine_th, INFINITE);
  CloseHandle(s->engine_th);
#else
  pthread_join(s->engine_th, NULL);
#endif
  s->engine_th_spawned = false;
  evt_mutex_unlock(&s->spawn_mu);
  LOG("[scheduler:stop_loop] -> engine stopped, thread joined.\n");
}

static bool scheduler_engine_exists(Scheduler *s)
{
  return s && s->engine;
}
