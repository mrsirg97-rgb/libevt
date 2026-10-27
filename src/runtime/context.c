#include <stdlib.h>

#include "libevt/runtime/context.h"

Context *context_create(void *scope, void (*future_fn)(void *scope))
{
  Context *ctx = malloc(sizeof(Context));
  if (!ctx)
    return NULL;
  ctx->scope = scope;
  ctx->future_fn = future_fn;
  return ctx;
}

void context_destroy(Context *ctx)
{
  if (!ctx)
    return;
  free(ctx);
}

void context_resolve(Context *ctx)
{
  if (!ctx || !ctx->future_fn)
    return;
  ctx->future_fn(ctx->scope);
}
