#ifndef CAP_H
#define CAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Context Context;

typedef struct Cap
{
  int slots;
  int used;
  Context **waiting;
  size_t len;
  size_t capacity;
  bool (*before_fn)(void *a, void *b, void *arg);
  void *before_arg;
} Cap;

Cap *cap_create(int slots, bool (*before_fn)(void *a, void *b, void *arg), void *before_arg);
void cap_destroy(Cap *cap);

void cap_submit(Cap *cap, Context *ctx);
void cap_release(Cap *cap);
size_t cap_drop(Cap *cap, bool (*match_fn)(void *scope, void *arg), void *arg);

#endif

/*
Cap
  `slots` of something at once (a model's conversation states, the GPUs for one prompt read); past them, the work
  waits - it is not refused.  the consumer's only, as Own is.

cap_submit
  the work runs now if a slot is free (its context resolved, the slot its own), else it waits.  the context is cap's:
  destroyed once it ran, or at cap_destroy / cap_drop if it never did.

cap_release
  a slot frees: the first waiting one runs in it.  first: the one before every other by before_fn(a scope, another's
  scope) - asked at release, so an order that changes as they wait (aging) is the order then - or the earliest
  submitted, before_fn NULL.  ties: the earliest.

cap_drop
  the waiting ones match_fn says yes to leave (a STOP of a request that never began); how many.
*/
