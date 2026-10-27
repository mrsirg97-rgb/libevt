#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/cap.h"

typedef struct
{
  int len;
  int id;
} Req;

static int ran[16];
static int n_ran;

static void run_fn(void *scope) { ran[n_ran++] = ((Req *)scope)->id; }

static bool shortest(void *a, void *b, void *arg)
{
  (void)arg;
  return ((Req *)a)->len < ((Req *)b)->len;
}

static bool is_id(void *scope, void *arg) { return ((Req *)scope)->id == *(int *)arg; }

int main(void)
{
  printf("[test_cap] start\n");
  Req r[6] = {{16000, 1}, {4000, 2}, {1000, 3}, {250, 4}, {250, 5}, {500, 6}};

  Cap *fifo = cap_create(2, NULL, NULL);
  n_ran = 0;
  for (int i = 0; i < 5; i++)
    cap_submit(fifo, context_create(&r[i], run_fn));
  assert(n_ran == 2 && ran[0] == 1 && ran[1] == 2);
  cap_release(fifo);
  cap_release(fifo);
  cap_release(fifo);
  assert(n_ran == 5 && ran[2] == 3 && ran[3] == 4 && ran[4] == 5);
  cap_destroy(fifo);

  Cap *sjf = cap_create(1, shortest, NULL);
  n_ran = 0;
  for (int i = 0; i < 6; i++)
    cap_submit(sjf, context_create(&r[i], run_fn));
  assert(n_ran == 1 && ran[0] == 1);
  int stopped = 6;
  assert(cap_drop(sjf, is_id, &stopped) == 1);
  for (int i = 0; i < 4; i++)
    cap_release(sjf);
  assert(n_ran == 5);
  assert(ran[1] == 4 && ran[2] == 5 && ran[3] == 3 && ran[4] == 2);
  cap_release(sjf);
  assert(n_ran == 5);
  cap_destroy(sjf);
  printf("[test_cap] arrival order, a shortest-first order, ties and drop passed.\n");
  return 0;
}
