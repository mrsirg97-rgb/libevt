#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct Event Event;
typedef struct Map Map;

typedef struct Queue
{
  Event **evts;
  size_t len;
  size_t capacity;
} Queue;

Queue *queue_create(size_t initial_capacity);
void queue_destroy(Queue *queue);

bool queue_view(Queue *q, Event **evts, size_t *evts_size);
bool queue_push(Queue *q, Event *evt);
void queue_pop(Queue *q, Event **evt);
Event *queue_peek(Queue *q);
bool queue_update(Queue *q, Event *evt, int priority);

#endif

/*
Queue
  Event Priority Queue implemented with Max Heap.
  Event are processed by
    1. Priority descending
    2. Id ascending (the order the engine numbered them in)

  queue_view clones the current heap and returns back the sorted transactions by priority, id.
  queue_push
    false: refused (no queue, no event, or no memory to grow) - the event is still the caller's.
  queue_update
    a queued event's priority moved in place: the heap kept, the event where its new priority puts it (an event
    waited on is first now, one wanted later goes back). false: the event is not in this queue (never pushed, or
    popped). each event knows its place (Event.index), so it is O(log n), no search.
*/