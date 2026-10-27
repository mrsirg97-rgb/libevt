#include <stdlib.h>
#include <string.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/own.h"

#define OWN_CAP 8

static void own_settle(Own *own);

Own *own_create(void (*resume_fn)(void *arg), void *resume_arg)
{
  Own *own = malloc(sizeof(Own));
  if (!own)
    return NULL;
  own->wants = malloc(OWN_CAP * sizeof *own->wants);
  if (!own->wants)
  {
    free(own);
    return NULL;
  }
  own->out = 0;
  own->len = 0;
  own->capacity = OWN_CAP;
  own->owning = false;
  own->resume_fn = resume_fn;
  own->resume_arg = resume_arg;
  return own;
}

void own_destroy(Own *own)
{
  if (!own)
    return;
  for (size_t i = 0; i < own->len; i++)
    context_destroy(own->wants[i]);
  free(own->wants);
  free(own);
}

bool own_open(const Own *own)
{
  return own && !own->owning && own->len == 0;
}

void own_out(Own *own)
{
  if (own)
    own->out++;
}

void own_back(Own *own)
{
  if (!own)
    return;
  if (own->out > 0)
    own->out--;
  own_settle(own);
}

void own_want(Own *own, Context *ctx)
{
  if (!(own && ctx))
    return;
  if (own->len == own->capacity)
  {
    Context **wants = realloc(own->wants, own->capacity * 2 * sizeof *own->wants);
    if (!wants)
    {
      context_destroy(ctx);
      return;
    }
    own->wants = wants;
    own->capacity *= 2;
  }
  own->wants[own->len++] = ctx;
  own_settle(own);
}

static void own_settle(Own *own)
{
  if (own->owning || own->out > 0 || own->len == 0)
    return;
  own->owning = true;
  while (own->len > 0 && own->out == 0)
  {
    Context *ctx = own->wants[0];
    own->len--;
    memmove(own->wants, own->wants + 1, own->len * sizeof *own->wants);
    context_resolve(ctx);
    context_destroy(ctx);
  }
  own->owning = false;
  if (own->len == 0 && own->out == 0 && own->resume_fn)
    own->resume_fn(own->resume_arg);
}
