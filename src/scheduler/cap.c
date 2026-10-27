#include <stdlib.h>
#include <string.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/cap.h"

#define CAP_WAIT 8

static void cap_run(Cap *cap, Context *ctx);

Cap *cap_create(int slots, bool (*before_fn)(void *a, void *b, void *arg), void *before_arg)
{
  Cap *cap = malloc(sizeof(Cap));
  if (!cap)
    return NULL;
  cap->waiting = malloc(CAP_WAIT * sizeof *cap->waiting);
  if (!cap->waiting)
  {
    free(cap);
    return NULL;
  }
  cap->slots = slots > 0 ? slots : 1;
  cap->used = 0;
  cap->len = 0;
  cap->capacity = CAP_WAIT;
  cap->before_fn = before_fn;
  cap->before_arg = before_arg;
  return cap;
}

void cap_destroy(Cap *cap)
{
  if (!cap)
    return;
  for (size_t i = 0; i < cap->len; i++)
    context_destroy(cap->waiting[i]);
  free(cap->waiting);
  free(cap);
}

void cap_submit(Cap *cap, Context *ctx)
{
  if (!(cap && ctx))
    return;
  if (cap->used < cap->slots)
  {
    cap_run(cap, ctx);
    return;
  }
  if (cap->len == cap->capacity)
  {
    Context **w = realloc(cap->waiting, cap->capacity * 2 * sizeof *cap->waiting);
    if (!w)
    {
      context_destroy(ctx);
      return;
    }
    cap->waiting = w;
    cap->capacity *= 2;
  }
  cap->waiting[cap->len++] = ctx;
}

void cap_release(Cap *cap)
{
  if (!cap)
    return;
  if (cap->used > 0)
    cap->used--;
  while (cap->used < cap->slots && cap->len > 0)
  {
    size_t best = 0;
    if (cap->before_fn)
      for (size_t i = 1; i < cap->len; i++)
        if (cap->before_fn(cap->waiting[i]->scope, cap->waiting[best]->scope, cap->before_arg))
          best = i;
    Context *ctx = cap->waiting[best];
    cap->len--;
    memmove(cap->waiting + best, cap->waiting + best + 1, (cap->len - best) * sizeof *cap->waiting);
    cap_run(cap, ctx);
  }
}

size_t cap_drop(Cap *cap, bool (*match_fn)(void *scope, void *arg), void *arg)
{
  if (!(cap && match_fn))
    return 0;
  size_t kept = 0, dropped = 0;
  for (size_t i = 0; i < cap->len; i++)
  {
    if (match_fn(cap->waiting[i]->scope, arg))
    {
      context_destroy(cap->waiting[i]);
      dropped++;
    }
    else
      cap->waiting[kept++] = cap->waiting[i];
  }
  cap->len = kept;
  return dropped;
}

static void cap_run(Cap *cap, Context *ctx)
{
  cap->used++;
  context_resolve(ctx);
  context_destroy(ctx);
}
