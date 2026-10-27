#ifndef EVENT_H
#define EVENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct Context Context;

typedef struct Event
{
  uint64_t id;
  int priority;
  Context *ctx;
} Event;

Event *event_create(uint64_t id, int priority, Context *ctx);
void event_destroy(Event *evt);

int event_update_priority(Event *evt, int priority);
void event_execute(Event *evt);

#endif

/*
Event
  Events are the data structure in the Queue for the event loop.
  Id should be treated as something stable and unique, in this case a timestamp.
  This structure allows for the deferred processing of the Context object based on priorioty.

  event_execute
    resolve the underlying context.
*/
