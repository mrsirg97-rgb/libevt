#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "libevt/runtime/context.h"
#include "libevt/runtime/event.h"
#include "libevt/runtime/queue.h"

static int noop_scope;

static void push(Queue *q, uint64_t id, int priority)
{
  queue_push(q, event_create(id, priority, context_create(&noop_scope, NULL)));
}

int main(void)
{
  printf("[test_queue:order] start\n");
  Queue *q = queue_create(4);
  assert(q);
  const int prio[] = {1, 5, 3, 5, 0, 3, 9, 1, 5, 2};
  for (int i = 0; i < 10; i++)
    push(q, (uint64_t)i + 1, prio[i]);
  const uint64_t want[] = {7, 2, 4, 9, 3, 6, 10, 1, 8, 5};
  Event *out = NULL;
  for (int i = 0; i < 10; i++)
  {
    queue_pop(q, &out);
    assert(out && out->id == want[i]);
    event_destroy(out);
  }
  queue_pop(q, &out);
  assert(!out);

  // equal priorities, ids further apart than an int holds: still the earliest first
  push(q, 5000000000ULL, 2);
  push(q, 1, 2);
  push(q, 3000000000ULL, 2);
  const uint64_t far[] = {1, 3000000000ULL, 5000000000ULL};
  for (int i = 0; i < 3; i++)
  {
    queue_pop(q, &out);
    assert(out && out->id == far[i]);
    event_destroy(out);
  }

  queue_destroy(q);
  printf("[test_queue:order] passed.\n");
  return 0;
}
