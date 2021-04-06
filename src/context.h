#pragma once

#include <stdint.h>

struct lyra_ctx_ptr {
    void *ptr;
    int flags;
    size_t size, hash;
};

struct lyra_ctx_ptr_set {
    uintptr_t minptr;
    uintptr_t maxptr;
    struct lyra_ctx_ptr *items, *frees;
    double loadfactor, sweepfactor;
    size_t nitems, nslots, mitems, nfrees;
};

struct lyra_ctx {
    struct lyra_ctx_ptr_set manual;
    struct lyra_ctx_ptr_set gc;
    int gc_paused;
};

void lyra_ctx_init(struct lyra_ctx *ctx);
void lyra_ctx_del(struct lyra_ctx *ctx);

void *lyra_ctx_mem_malloc(struct lyra_ctx *ctx, size_t size);
void *lyra_ctx_mem_calloc(struct lyra_ctx *ctx, size_t nmemb, size_t size);
void *lyra_ctx_mem_realloc(struct lyra_ctx *ctx, void *ptr, size_t size);
void lyra_ctx_mem_free(struct lyra_ctx *ctx, void *ptr);

void lyra_ctx_gc_run(struct lyra_ctx *ctx);

static inline void lyra_ctx_gc_pause(struct lyra_ctx *ctx) {
    ctx->gc_paused = 1;
}

static inline void lyra_ctx_gc_resume(struct lyra_ctx *ctx) {
    ctx->gc_paused = 0;
}

void *lyra_ctx_gc_alloc(struct lyra_ctx *ctx, size_t size);
void *lyra_ctx_gc_alloc_root(struct lyra_ctx *ctx, size_t size);
void *lyra_ctx_gc_calloc(struct lyra_ctx *ctx, size_t nmemb, size_t size);
void *lyra_ctx_gc_realloc(struct lyra_ctx *ctx, void *ptr, size_t size);
