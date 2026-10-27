#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/engine.h"

#define THREADS 8
#define EACH 20000

typedef struct
{
  Engine *engine;
  int *seen;
  atomic_int *ran;
  int base;
} Producer;

typedef struct
{
  int *seen;
  atomic_int *ran;
  int k;
} Job;

static void job_fn(void *scope)
{
  Job *j = scope;
  j->seen[j->k]++;
  atomic_fetch_add(j->ran, 1);
  free(j);
}

static void *producer(void *arg)
{
  Producer *p = arg;
  for (int i = 0; i < EACH; i++)
  {
    Job *j = malloc(sizeof(Job));
    j->seen = p->seen;
    j->ran = p->ran;
    j->k = p->base + i;
    assert(engine_add(p->engine, context_create(j, job_fn), i % 4) != 0);
  }
  return NULL;
}

static void stop_fn(void *scope) { engine_stop((Engine *)scope); }

static void *consumer(void *arg)
{
  engine_run((Engine *)arg);
  return NULL;
}

static void threads(void)
{
  Engine *e = engine_create(NULL, NULL, 0);
  int *seen = calloc((size_t)THREADS * EACH, sizeof(int));
  atomic_int ran = 0;
  pthread_t c, ps[THREADS];
  Producer args[THREADS];
  pthread_create(&c, NULL, consumer, e);
  for (int t = 0; t < THREADS; t++)
  {
    args[t] = (Producer){e, seen, &ran, t * EACH};
    pthread_create(&ps[t], NULL, producer, &args[t]);
  }
  for (int t = 0; t < THREADS; t++)
    pthread_join(ps[t], NULL);
  engine_add(e, context_create(e, stop_fn), 0);
  pthread_join(c, NULL);
  assert(atomic_load(&ran) == THREADS * EACH);
  for (int i = 0; i < THREADS * EACH; i++)
    assert(seen[i] == 1);
  int dummy = 0;
  Context *refused = context_create(&dummy, NULL);
  assert(engine_add(e, refused, 1) == 0);
  context_destroy(refused);
  free(seen);
  engine_destroy(e);
}

typedef struct
{
  int order[8];
  int n;
} Log;

typedef struct
{
  Log *log;
  int v;
} Note;

static void note_fn(void *scope)
{
  Note *n = scope;
  n->log->order[n->log->n++] = n->v;
}

static void priority(void)
{
  Engine *e = engine_create(NULL, NULL, 0);
  Log log = {{0}, 0};
  Note a = {&log, 1}, b = {&log, 2}, c = {&log, 3}, d = {&log, 4};
  engine_add(e, context_create(&a, note_fn), 0);
  engine_add(e, context_create(&b, note_fn), 3);
  engine_add(e, context_create(&c, note_fn), 3);
  engine_add(e, context_create(&d, note_fn), 7);
  engine_add(e, context_create(e, stop_fn), -5);
  engine_run(e);
  assert(log.n == 4);
  assert(log.order[0] == 4 && log.order[1] == 2 && log.order[2] == 3 && log.order[3] == 1);
  engine_destroy(e);
}

typedef struct
{
  Engine *e;
  Log *log;
} Nest;

static void nest_fn(void *scope)
{
  Nest *n = scope;
  n->log->order[n->log->n++] = 10;
  while (engine_pump(n->e, 0))
    ;
}

static void *late_add(void *arg)
{
  Engine *e = ((Nest *)arg)->e;
  struct timespec ts = {0, 30 * 1000000L};
  nanosleep(&ts, NULL);
  static Note late;
  late = (Note){((Nest *)arg)->log, 30};
  engine_add(e, context_create(&late, note_fn), 0);
  return NULL;
}

static int ticks;
static void tick_fn(void *arg)
{
  (void)arg;
  ticks++;
}

static void *stop_later(void *arg)
{
  struct timespec ts = {0, 110 * 1000000L};
  nanosleep(&ts, NULL);
  engine_stop((Engine *)arg);
  return NULL;
}

static void pump(void)
{
  Engine *e = engine_create(NULL, NULL, 0);
  assert(!engine_pump(e, 0));
  Log log = {{0}, 0};
  Nest n = {e, &log};
  Note after = {&log, 20};
  engine_add(e, context_create(&n, nest_fn), 1);
  engine_add(e, context_create(&after, note_fn), 0);
  assert(engine_pump(e, -1));
  assert(log.n == 2 && log.order[0] == 10 && log.order[1] == 20);
  pthread_t t;
  pthread_create(&t, NULL, late_add, &n);
  assert(engine_pump(e, 2000));
  pthread_join(t, NULL);
  assert(log.order[log.n - 1] == 30);
  engine_destroy(e);

  Engine *idle = engine_create(tick_fn, NULL, 20);
  ticks = 0;
  pthread_t s;
  pthread_create(&s, NULL, stop_later, idle);
  engine_run(idle);
  pthread_join(s, NULL);
  assert(ticks >= 3 && ticks <= 6);
  engine_destroy(idle);
}

static int freed;
static void count_fn(void *scope)
{
  (void)scope;
  freed = -1000;
}

static void destroy(void)
{
  Engine *e = engine_create(NULL, NULL, 0);
  int s = 0;
  for (int i = 0; i < 5; i++)
    engine_add(e, context_create(&s, count_fn), i);
  assert(engine_pending(e) == 5);
  freed = 0;
  engine_destroy(e);
  assert(freed == 0);
}

int main(void)
{
  printf("[test_engine] start\n");
  threads();
  priority();
  pump();
  destroy();
  printf("[test_engine] threads, priority, pump, idle and destroy passed.\n");
  return 0;
}
