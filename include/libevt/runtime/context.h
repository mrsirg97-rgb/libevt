#ifndef CONTEXT_H
#define CONTEXT_H

typedef struct Context
{
  void *scope;
  void (*future_fn)(void *scope);
} Context;

Context *context_create(void *scope, void (*future_fn)(void *scope));
void context_destroy(Context *ctx);

void context_resolve(Context *ctx);

#endif

/*
Context
  essentially a scoped object for async operations.
  scope -> the operation to perform once the context is completed
  op -> the callback fn to call once the context is no longer deferred

context_resolve
  calls future_fn(scope) - scope may be NULL (a closure with nothing to carry)
*/
