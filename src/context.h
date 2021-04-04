#pragma once

struct lyra_ctx {
    void **alloc_entries;
    size_t alloc_entries_len;
};

void lyra_ctx_init(struct lyra_ctx *ctx);
void lyra_ctx_del(struct lyra_ctx *ctx);

void *lyra_ctx_mem_malloc(struct lyra_ctx *ctx, size_t size);
void *lyra_ctx_mem_calloc(struct lyra_ctx *ctx, size_t nmemb, size_t size);
void *lyra_ctx_mem_realloc(struct lyra_ctx *ctx, void *ptr, size_t size);
