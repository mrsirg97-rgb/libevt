#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libevt/runtime/context.h"
#include "libevt/runtime/event.h"
#include "libevt/runtime/queue.h"

#define N 512
#define STEPS 20000

static int noop_scope;

/* the reference: every event's priority while it is queued, and whether it is */
static int prio[N];
static int queued[N];
static Event *ev[N];

/* the event the queue must pop next: the highest priority, of those the earliest id */
static int expected(void)
{
  int best = -1;
  for (int i = 0; i < N; i++)
    if (queued[i] && (best < 0 || prio[i] > prio[best] || (prio[i] == prio[best] && i < best)))
      best = i;
  return best;
}

/* each queued event's index is where it sits */
static void check_index(Queue *q)
{
  size_t n = 0;
  for (int i = 0; i < N; i++)
    if (queued[i])
    {
      assert(ev[i]->index < q->len && q->evts[ev[i]->index] == ev[i]);
      n++;
    }
  assert(n == q->len);
}

int main(void)
{
  printf("[test_queue:update] start\n");
  Queue *q = queue_create(4);
  assert(q);
  srand(7);
  int pops = 0, updates = 0;
  for (int i = 0; i < N; i++) /* ids 1..N, each event made once */
    ev[i] = event_create((uint64_t)i + 1, 0, context_create(&noop_scope, NULL));
  for (int step = 0; step < STEPS; step++)
  {
    const int i = rand() % N, r = rand() % 20, op = r < 9 ? 0 : r < 17 ? 1 : 2; /* the queue grows to hundreds */
    if (op == 0 && !queued[i])
    {
      prio[i] = rand() % 16;
      ev[i]->priority = prio[i];
      assert(queue_push(q, ev[i]));
      queued[i] = 1;
    }
    else if (op == 1)
    {
      const int p = rand() % 16;
      assert(queue_update(q, ev[i], p) == (bool)queued[i]); /* an event not in the queue: refused */
      if (queued[i])
        prio[i] = p, updates++;
    }
    else if (q->len)
    {
      Event *out = NULL;
      queue_pop(q, &out);
      const int want = expected();
      assert(out && out == ev[want] && out->index == SIZE_MAX);
      queued[want] = 0, pops++;
      assert(!queue_update(q, out, 99)); /* popped: no longer the queue's */
    }
    check_index(q);
  }
  for (Event *out = NULL; q->len;)
  {
    queue_pop(q, &out);
    const int want = expected();
    assert(out == ev[want]);
    queued[want] = 0;
  }
  assert(!queue_update(NULL, ev[0], 1) && !queue_update(q, NULL, 1) && !queue_push(NULL, ev[0]) && !queue_push(q, NULL));
  for (int i = 0; i < N; i++)
    event_destroy(ev[i]);
  queue_destroy(q);
  printf("[test_queue:update] %d steps (%d pops, %d updates): every pop the highest by the priorities then, every index "
         "its place; an update of an event not queued refused\n",
         STEPS, pops, updates);
  return 0;
}
