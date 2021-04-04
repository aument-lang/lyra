#include <stdlib.h>
#include <string.h>

#include "context.h"

void lyra_ctx_init(struct lyra_ctx *ctx) {
    ctx->alloc_entries = 0;
    ctx->alloc_entries_len = 0;
}

void lyra_ctx_del(struct lyra_ctx *ctx) {
    for (size_t i = 0; i < ctx->alloc_entries_len; i++) {
        free(ctx->alloc_entries[i]);
    }
    free(ctx->alloc_entries);
}

// TODO: store allocated entries in a hashmap
// TODO: do mark-and-sweep garbage collection?

static void add_alloc_entry(struct lyra_ctx *ctx, void *ptr) {
    if (ctx->alloc_entries_len == 0) {
        ctx->alloc_entries = malloc(sizeof(void *) * 1);
        ctx->alloc_entries[0] = ptr;
        ctx->alloc_entries_len = 1;
    } else {
        ctx->alloc_entries =
            realloc(ctx->alloc_entries,
                    sizeof(void *) * (ctx->alloc_entries_len + 1));
        ctx->alloc_entries[ctx->alloc_entries_len] = ptr;
        ctx->alloc_entries_len++;
    }
}

static void remove_alloc_entry(struct lyra_ctx *ctx, void *ptr) {
    for (size_t i = 0; i < ctx->alloc_entries_len; i++) {
        if (ctx->alloc_entries[i] == ptr) {
            memmove(&ctx->alloc_entries[i], &ctx->alloc_entries[i + 1],
                    sizeof(void *) * (ctx->alloc_entries_len - i - 1));
            ctx->alloc_entries_len--;
            return;
        }
    }
}

void *lyra_ctx_mem_malloc(struct lyra_ctx *ctx, size_t size) {
    void *ptr = malloc(size);
    add_alloc_entry(ctx, ptr);
    return ptr;
}

void *lyra_ctx_mem_calloc(struct lyra_ctx *ctx, size_t nmemb,
                          size_t size) {
    void *ptr = calloc(nmemb, size);
    add_alloc_entry(ctx, ptr);
    return ptr;
}

void *lyra_ctx_mem_realloc(struct lyra_ctx *ctx, void *ptr, size_t size) {
    void *old_ptr = ptr;
    void *new_ptr = realloc(ptr, size);
    if (new_ptr != old_ptr) {
        remove_alloc_entry(ctx, old_ptr);
        add_alloc_entry(ctx, new_ptr);
    }
    return new_ptr;
}
