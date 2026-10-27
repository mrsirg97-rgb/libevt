#include <assert.h>
#include <stdio.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/own.h"

typedef struct
{
  Own *own;
  char trace[32];
  int n;
  int resumed;
} World;

static World w;

static void mark(char c) { w.trace[w.n++] = c; }

static void owner_a(void *scope)
{
  (void)scope;
  mark('A');
  assert(!own_open(w.own));
}

static void owner_inner(void *scope)
{
  (void)scope;
  mark('I');
}

static void owner_b(void *scope)
{
  (void)scope;
  mark('B');
  own_want(w.own, context_create(&w, owner_inner));
  assert(w.trace[w.n - 1] == 'B');
}

static void resume(void *arg)
{
  (void)arg;
  w.resumed++;
  mark('R');
}

static void owner_sends(void *scope)
{
  (void)scope;
  mark('S');
  own_out(w.own);
}

int main(void)
{
  printf("[test_own] start\n");
  w.own = own_create(resume, NULL);
  assert(own_open(w.own));

  own_out(w.own);
  own_out(w.own);
  own_want(w.own, context_create(&w, owner_a));
  own_want(w.own, context_create(&w, owner_b));
  assert(!own_open(w.own));
  assert(w.n == 0);
  own_back(w.own);
  assert(w.n == 0);
  own_back(w.own);
  w.trace[w.n] = 0;
  assert(w.n == 4 && w.trace[0] == 'A' && w.trace[1] == 'B' && w.trace[2] == 'I' && w.trace[3] == 'R');
  assert(own_open(w.own) && w.resumed == 1);

  w.n = 0;
  own_want(w.own, context_create(&w, owner_a));
  assert(w.n == 2 && w.trace[0] == 'A' && w.trace[1] == 'R');

  // an owner that sends shared work out: the next waits for it, and resume waits for both
  w.n = 0;
  own_out(w.own);
  own_want(w.own, context_create(&w, owner_sends));
  own_want(w.own, context_create(&w, owner_a));
  own_back(w.own);
  assert(w.n == 1 && w.trace[0] == 'S');
  own_back(w.own);
  assert(w.n == 3 && w.trace[1] == 'A' && w.trace[2] == 'R');

  own_out(w.own);
  own_want(w.own, context_create(&w, owner_a));
  own_destroy(w.own);
  printf("[test_own] order, nested owners, owners sending work and resume passed.\n");
  return 0;
}
