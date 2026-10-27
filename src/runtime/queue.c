#include <stdio.h>
#include <stdlib.h>

#include "libevt/runtime/event.h"
#include "libevt/runtime/queue.h"

#define D 4 // 4-ary max-heap (4 children per node)
#define MIN_CAP 8

static inline int queue_cmp_fn(const Event *evt1, const Event *evt2)
{
  if (evt1->priority != evt2->priority)
    return evt1->priority > evt2->priority ? 1 : -1;
  if (evt1->id != evt2->id)
    return evt1->id < evt2->id ? 1 : -1;
  return 0;
}

static inline bool queue_has_evts(Queue *q)
{
  return q && q->len > 0;
}

static inline void queue_resize(Queue *q)
{
  if (q->len == q->capacity)
  {
    size_t new_cap = q->capacity ? q->capacity * 2 : MIN_CAP;
    Event **evts = realloc(q->evts, new_cap * sizeof *q->evts);
    if (!evts)
      return;
    q->evts = evts;
    q->capacity = new_cap;
  }
  else if (q->len >= MIN_CAP && q->len < q->capacity / 2)
  {
    size_t new_cap = (q->len * 3) / 2;
    Event **evts = realloc(q->evts, new_cap * sizeof *q->evts);
    if (!evts)
      return;
    q->evts = evts;
    q->capacity = new_cap;
  }
}

static inline void queue_swap_evts(Queue *q, size_t i, size_t j)
{
  Event *evt = q->evts[i];
  q->evts[i] = q->evts[j];
  q->evts[j] = evt;
}

static inline void queue_heapify_down(Queue *q, size_t index)
{
  for (;;)
  {
    size_t first = index * D + 1;
    if (first >= q->len)
      break;
    size_t best = first;
    size_t last = best + (D - 1);
    if (last >= q->len)
      last = q->len - 1;
    for (size_t child = first + 1; child <= last; child++)
    {
      if (queue_cmp_fn(q->evts[child], q->evts[best]) > 0)
        best = child;
    }

    if (queue_cmp_fn(q->evts[best], q->evts[index]) <= 0)
      break;
    queue_swap_evts(q, index, best);
    index = best;
  }
}

static inline void queue_heapify_up(Queue *q, size_t index)
{
  while (index > 0)
  {
    size_t parent = (index - 1) / D;
    if (queue_cmp_fn(q->evts[index], q->evts[parent]) <= 0)
      break;
    queue_swap_evts(q, index, parent);
    index = parent;
  }
}

Queue *queue_create(size_t initial_capacity)
{
  Queue *q = malloc(sizeof(Queue));
  if (!q)
    return NULL;
  q->capacity = initial_capacity > 0 ? initial_capacity : 8;
  q->evts = malloc(q->capacity * sizeof *q->evts);
  if (!q->evts)
  {
    free(q);
    return NULL;
  }
  q->len = 0;
  return q;
}

void queue_destroy(Queue *q)
{
  if (!q)
    return;
  if (q->evts)
    free(q->evts);
  free(q);
}

bool queue_view(Queue *q, Event **evts, size_t *evts_size)
{
  if (!(queue_has_evts(q) && evts))
    return false;
  if (evts_size && q->len < *evts_size)
    return false;
  size_t count = (evts_size && *evts_size > 0) ? *evts_size : q->len;
  if (count > q->len)
    count = q->len;
  for (size_t i = 0; i < count; i++)
    evts[i] = q->evts[i];
  if (evts_size)
    *evts_size = count;
  return true;
}

void queue_push(Queue *q, Event *evt)
{
  if (!(q && q->evts && evt))
    return;
  queue_resize(q);
  size_t index = q->len++;
  q->evts[index] = evt;
  queue_heapify_up(q, index);
}

void queue_pop(Queue *q, Event **evt)
{
  if (!queue_has_evts(q))
  {
    if (evt)
      *evt = NULL;
    return;
  }

  if (evt)
    *evt = q->evts[0];
  q->len--;
  if (q->len > 0)
  {
    q->evts[0] = q->evts[q->len];
    queue_heapify_down(q, 0);
  }
}

Event *queue_peek(Queue *q)
{
  if (!queue_has_evts(q))
    return NULL;
  return q->evts[0];
}
