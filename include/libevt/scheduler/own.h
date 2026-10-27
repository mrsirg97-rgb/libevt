#ifndef OWN_H
#define OWN_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Context Context;

typedef struct Own
{
  size_t out;
  Context **wants;
  size_t len;
  size_t capacity;
  bool owning;
  void (*resume_fn)(void *arg);
  void *resume_arg;
} Own;

Own *own_create(void (*resume_fn)(void *arg), void *resume_arg);
void own_destroy(Own *own);

bool own_open(const Own *own);
void own_out(Own *own);
void own_back(Own *own);
void own_want(Own *own, Context *ctx);

#endif

/*
Own
  a resource's owner: work that needs the resource alone (a prompt's read needs every GPU; the expert cache's
  boundary needs nothing of the decode in flight).  the shared work goes out in pieces (a token's window) and comes
  back as events; an owner runs once none is out, then the shared work goes out again.  the consumer's only: every
  call is made inside an event, so nothing here locks.

own_open
  new shared work may go out: no owner waiting or running.  otherwise the caller parks it (and sends it at resume).

own_out / own_back
  a piece of shared work went out / came back.  own_back settles: once none is out, the owners run.

own_want
  an owner, in the order asked.  runs at once if none is out; asked inside an owner (or while one runs), it runs
  after that one.  the context is own's: destroyed once it ran, or at own_destroy if it never did.

resume_fn
  after the owners ran (none left, none out): the parked shared work goes out again.  an owner may send shared work
  out itself (a read's gap decodes the answers): the next owner then waits until it is back.
*/
