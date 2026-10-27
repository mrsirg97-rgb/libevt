# libevt

A small event engine in C, for runtimes that schedule work on shared hardware.

## build

```bash
make          # lib/libevt.a
make tests    # build and run the tests
make tsan     # the tests under ThreadSanitizer
```

```c
#include "libevt/runtime/context.h"
#include "libevt/scheduler/engine.h"
```

```bash
gcc -Iinclude main.c -Llib -levt -lpthread
```

needs a C compiler and pthreads (Windows: its own threads, no pthreads). requires nothing else.

## what it contains

- **an engine**: work runs on one consumer, highest priority first, the order it was added within a priority. any
  thread may add; only the consumer runs. nothing polls: the consumer sleeps until work comes.
- **a scheduler**: the engine on a thread of its own, for callers that do not run a loop themselves.
- **an owner** (`own`): work that needs a resource alone waits until the shared work out on it is back, runs, and the
  shared work goes out again.
- **a cap** (`cap`): `n` slots of something at once; past them, work waits in an order you give instead of being
  refused.

## the api

| | |
| --- | --- |
| `context_create(scope, fn)` | the work: `fn(scope)` when it runs |
| `engine_create(idle_fn, arg, idle_ms)` | an engine; `idle_fn` runs when nothing came for `idle_ms` |
| `engine_add(engine, ctx, priority)` | any thread: queue the work, returns its id (0: refused) |
| `engine_run(engine)` | the consumer: run until stopped |
| `engine_pump(engine, ms)` | the consumer: run one, waiting up to `ms` - also from inside an event, to wait for the events it needs |
| `engine_stop(engine)` | any thread: the run ends after the event it is in |
| `scheduler_create` / `scheduler_start_loop` / `scheduler_schedule_evt` / `scheduler_stop_loop` | the engine on its own thread |
| `own_out` / `own_back` | a piece of shared work went out / came back |
| `own_want(own, ctx)` | the work needs the resource alone: runs once none is out |
| `own_open(own)` | may new shared work go out (no owner waiting)? |
| `cap_submit(cap, ctx)` | run now if a slot is free, else wait |
| `cap_release(cap)` | a slot frees: the first waiting one by your `before_fn` runs in it |
| `cap_drop(cap, match_fn, arg)` | the waiting ones `match_fn` says yes to leave |

an added context belongs to the engine (or `own`, or `cap`): destroyed once it ran, or at destroy if it never did.
the headers have each function's notes.

## example

an inference server's loop. a request, the next prompt read and a token ready are events; the requests run first,
so every request that came is admitted before the next read is chosen.

```c
#include <stdio.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/engine.h"

enum { TOKEN = 1, READ = 2, LINE = 3 };

static void say(void *scope) { printf("%s\n", (const char *)scope); }
static void stop(void *scope) { engine_stop((Engine *)scope); }

int main(void)
{
  Engine *engine = engine_create(NULL, NULL, 0);

  engine_add(engine, context_create("a token is ready", say), TOKEN);
  engine_add(engine, context_create("read the next prompt", say), READ);
  engine_add(engine, context_create("a request came", say), LINE);
  engine_add(engine, context_create("another request came", say), LINE);
  engine_add(engine, context_create(engine, stop), 0);

  engine_run(engine);
  engine_destroy(engine);
  return 0;
}
```

```
a request came
another request came
read the next prompt
a token is ready
```

and a read that needs every GPU while tokens are in flight:

```c
#include <stdio.h>

#include "libevt/runtime/context.h"
#include "libevt/scheduler/own.h"

static void read_prompt(void *scope) { (void)scope; printf("the read runs: every GPU its own\n"); }
static void resume(void *arg) { (void)arg; printf("the answers go out again\n"); }

int main(void)
{
  Own *gpus = own_create(resume, NULL);

  own_out(gpus);
  own_out(gpus);
  printf("two tokens in flight\n");

  own_want(gpus, context_create(NULL, read_prompt));
  printf("a read wants the GPUs; open for new tokens: %s\n", own_open(gpus) ? "yes" : "no");

  own_back(gpus);
  printf("one token back\n");
  own_back(gpus);

  own_destroy(gpus);
  return 0;
}
```

```
two tokens in flight
a read wants the GPUs; open for new tokens: no
one token back
the read runs: every GPU its own
the answers go out again
```

what they show:

- the order is `(priority, id)`: the highest priority first, then the order added. two events never tie, so a run is
  the same order every time.
- one consumer owns whatever the events touch. `own` and `cap` are called only from inside events, so nothing in
  them locks.
- an owner never interrupts work in flight and is never starved by it: new work parks while it waits.

## why

most of what makes a model server feel fast is not the model - it is **what runs next**. one prompt read takes every
GPU, so a short question behind a long prompt waits for all of it unless something decides otherwise. libevt is that
something, kept separate from the model: a total order, one consumer, ownership and caps, and the policies built on
them.

it came out of [Maya](https://github.com/mw00/project-maya)'s DeepSeek serve loop, which runs on a C++ port of it.
there, answering several agents at once and reading their prompts a piece each in turn, on rig's real traffic, the
first token came 4.8x sooner than one conversation at a time, token for token the same answers.

the same `(priority, id)` engine runs [rig](https://github.com/mrsirg97-rgb/rig) in Go as
[evt](https://github.com/mrsirg97-rgb/evt).
