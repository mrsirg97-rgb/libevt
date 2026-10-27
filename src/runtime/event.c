#include <stdbool.h>
#include <stdlib.h>

#include "libevt/runtime/context.h"
#include "libevt/runtime/event.h"

Event *event_create(uint64_t id, int priority, Context *ctx)
{
  if (!ctx)
    return NULL;
  Event *evt = malloc(sizeof(Event));
  if (!evt)
    return NULL;
  evt->id = id;
  evt->priority = priority;
  evt->ctx = ctx;
  if (!evt->ctx)
  {
    free(evt);
    return NULL;
  }
  return evt;
}

void event_destroy(Event *evt)
{
  if (!evt)
    return;
  if (evt->ctx)
    context_destroy(evt->ctx);
  free(evt);
}

int event_update_priority(Event *evt, int priority)
{
  if (!evt)
    return -1;
  evt->priority = priority;
  return evt->priority;
}

void event_execute(Event *evt)
{
  if (evt && evt->ctx)
    context_resolve(evt->ctx);
}
