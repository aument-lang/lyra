// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"

// Garbage collection & pointer set adopted from tgc:
// https://github.com/orangeduck/tgc
//
// Licensed Under BSD
//
// Copyright (c) 2013, Daniel Holden All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//     Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation
// are those of the authors and should not be interpreted as representing
// official policies, either expressed or implied, of the FreeBSD Project.

enum { TGC_MARK = 0x01, TGC_ROOT = 0x02, TGC_LEAF = 0x04 };

static size_t hash(void *ptr) { return ((uintptr_t)ptr) >> 3; }

static void lyra_ctx_ptr_set_init(struct lyra_ctx_ptr_set *set);
static void lyra_ctx_ptr_set_del(struct lyra_ctx_ptr_set *set);

static size_t lyra_ctx_ptr_set_probe(struct lyra_ctx_ptr_set *set,
                                     size_t i, size_t h);
static struct lyra_ctx_ptr *
lyra_ctx_ptr_set_get_ptr(struct lyra_ctx_ptr_set *set, void *ptr);
static void lyra_ctx_ptr_set_add_ptr(struct lyra_ctx_ptr_set *set,
                                     void *ptr, size_t size, int flags);
static void lyra_ctx_ptr_set_remove_ptr(struct lyra_ctx_ptr_set *set,
                                        void *ptr);
static void *lyra_ctx_ptr_set_add(struct lyra_ctx_ptr_set *set, void *ptr,
                                  size_t size, int flags);
static void lyra_ctx_ptr_set_remove(struct lyra_ctx_ptr_set *set,
                                    void *ptr);

static int lyra_ctx_ptr_set_resize_more(struct lyra_ctx_ptr_set *set);
static int lyra_ctx_ptr_set_resize_less(struct lyra_ctx_ptr_set *set);

void lyra_ctx_ptr_set_init(struct lyra_ctx_ptr_set *set) {
    set->nitems = 0;
    set->nslots = 0;
    set->mitems = 0;
    set->nfrees = 0;
    set->maxptr = 0;
    set->items = NULL;
    set->frees = NULL;
    set->minptr = UINTPTR_MAX;
    set->loadfactor = 0.9;
    set->sweepfactor = 0.5;
}

void lyra_ctx_ptr_set_del(struct lyra_ctx_ptr_set *set) {
    free(set->items);
    free(set->frees);
}

size_t lyra_ctx_ptr_set_probe(struct lyra_ctx_ptr_set *set, size_t i,
                              size_t h) {
    long v = i - (h - 1);
    if (v < 0) {
        v = set->nslots + v;
    }
    return v;
}

struct lyra_ctx_ptr *lyra_ctx_ptr_set_get_ptr(struct lyra_ctx_ptr_set *set,
                                              void *ptr) {
    size_t i, j, h;
    i = hash(ptr) % set->nslots;
    j = 0;
    while (1) {
        h = set->items[i].hash;
        if (h == 0 || j > lyra_ctx_ptr_set_probe(set, i, h)) {
            return NULL;
        }
        if (set->items[i].ptr == ptr) {
            return &set->items[i];
        }
        i = (i + 1) % set->nslots;
        j++;
    }
    return NULL;
}

void lyra_ctx_ptr_set_add_ptr(struct lyra_ctx_ptr_set *set, void *ptr,
                              size_t size, int flags) {
    struct lyra_ctx_ptr item, tmp;
    size_t h, p, i, j;

    i = hash(ptr) % set->nslots;
    j = 0;

    item.ptr = ptr;
    item.flags = flags;
    item.size = size;
    item.hash = i + 1;

    while (1) {
        h = set->items[i].hash;
        if (h == 0) {
            set->items[i] = item;
            return;
        }
        if (set->items[i].ptr == item.ptr) {
            return;
        }
        p = lyra_ctx_ptr_set_probe(set, i, h);
        if (j >= p) {
            tmp = set->items[i];
            set->items[i] = item;
            item = tmp;
            j = p;
        }
        i = (i + 1) % set->nslots;
        j++;
    }
}

void lyra_ctx_ptr_set_remove_ptr(struct lyra_ctx_ptr_set *set, void *ptr) {
    size_t i, j, h, nj, nh;

    if (set->nitems == 0) {
        return;
    }

    for (i = 0; i < set->nfrees; i++) {
        if (set->frees[i].ptr == ptr) {
            set->frees[i].ptr = NULL;
        }
    }

    i = hash(ptr) % set->nslots;
    j = 0;

    while (1) {
        h = set->items[i].hash;
        if (h == 0 || j > lyra_ctx_ptr_set_probe(set, i, h)) {
            return;
        }
        if (set->items[i].ptr == ptr) {
            memset(&set->items[i], 0, sizeof(struct lyra_ctx_ptr));
            j = i;
            while (1) {
                nj = (j + 1) % set->nslots;
                nh = set->items[nj].hash;
                if (nh != 0 && lyra_ctx_ptr_set_probe(set, nj, nh) > 0) {
                    memcpy(&set->items[j], &set->items[nj],
                           sizeof(struct lyra_ctx_ptr));
                    memset(&set->items[nj], 0,
                           sizeof(struct lyra_ctx_ptr));
                    j = nj;
                } else {
                    break;
                }
            }
            set->nitems--;
            return;
        }
        i = (i + 1) % set->nslots;
        j++;
    }
}

void *lyra_ctx_ptr_set_add(struct lyra_ctx_ptr_set *set, void *ptr,
                           size_t size, int flags) {

    set->nitems++;
    set->maxptr = ((uintptr_t)ptr) + size > set->maxptr
                      ? ((uintptr_t)ptr) + size
                      : set->maxptr;
    set->minptr =
        ((uintptr_t)ptr) < set->minptr ? ((uintptr_t)ptr) : set->minptr;

    if (lyra_ctx_ptr_set_resize_more(set)) {
        lyra_ctx_ptr_set_add_ptr(set, ptr, size, flags);
        return ptr;
    } else {
        set->nitems--;
        free(ptr);
        return NULL;
    }
}

#define TGC_PRIMES_COUNT 24

static const size_t lyra_ctx_ptr_set_primes[TGC_PRIMES_COUNT] = {
    0,     1,      5,      11,     23,      53,      101,     197,
    389,   683,    1259,   2417,   4733,    9371,    18617,   37097,
    74093, 148073, 296099, 592019, 1100009, 2200013, 4400021, 8800019};

static size_t lyra_ctx_ptr_set_ideal_size(struct lyra_ctx_ptr_set *set,
                                          size_t size) {
    size_t i, last;
    size = (size_t)((double)(size + 1) / set->loadfactor);
    for (i = 0; i < TGC_PRIMES_COUNT; i++) {
        if (lyra_ctx_ptr_set_primes[i] >= size) {
            return lyra_ctx_ptr_set_primes[i];
        }
    }
    last = lyra_ctx_ptr_set_primes[TGC_PRIMES_COUNT - 1];
    for (i = 0;; i++) {
        if (last * i >= size) {
            return last * i;
        }
    }
    return 0;
}

static int lyra_ctx_ptr_set_rehash(struct lyra_ctx_ptr_set *set,
                                   size_t new_size) {

    size_t i;
    struct lyra_ctx_ptr *old_items = set->items;
    size_t old_size = set->nslots;

    set->nslots = new_size;
    set->items = calloc(set->nslots, sizeof(struct lyra_ctx_ptr));

    if (set->items == NULL) {
        set->nslots = old_size;
        set->items = old_items;
        return 0;
    }

    for (i = 0; i < old_size; i++) {
        if (old_items[i].hash != 0) {
            lyra_ctx_ptr_set_add_ptr(set, old_items[i].ptr,
                                     old_items[i].size,
                                     old_items[i].flags);
        }
    }

    free(old_items);

    return 1;
}

void lyra_ctx_ptr_set_remove(struct lyra_ctx_ptr_set *set, void *ptr) {
    lyra_ctx_ptr_set_remove_ptr(set, ptr);
    lyra_ctx_ptr_set_resize_less(set);
    set->mitems = set->nitems + set->nitems / 2 + 1;
}

int lyra_ctx_ptr_set_resize_more(struct lyra_ctx_ptr_set *set) {
    size_t new_size = lyra_ctx_ptr_set_ideal_size(set, set->nitems);
    size_t old_size = set->nslots;
    return (new_size > old_size) ? lyra_ctx_ptr_set_rehash(set, new_size)
                                 : 1;
}

int lyra_ctx_ptr_set_resize_less(struct lyra_ctx_ptr_set *set) {
    size_t new_size = lyra_ctx_ptr_set_ideal_size(set, set->nitems);
    size_t old_size = set->nslots;
    return (new_size < old_size) ? lyra_ctx_ptr_set_rehash(set, new_size)
                                 : 1;
}

void lyra_ctx_init(struct lyra_ctx *ctx) {
    memset(ctx, 0, sizeof(struct lyra_ctx));
    lyra_ctx_ptr_set_init(&ctx->manual);
    lyra_ctx_ptr_set_init(&ctx->gc);
}

static void gc_sweep(struct lyra_ctx_ptr_set *gc);

void lyra_ctx_del(struct lyra_ctx *ctx) {
    for (size_t i = 0; i < ctx->manual.nslots; i++) {
        struct lyra_ctx_ptr *data = &ctx->manual.items[i];
        if (data->hash == 0)
            continue;
        free(data->ptr);
    }
    lyra_ctx_ptr_set_del(&ctx->manual);

    for (size_t i = 0; i < ctx->gc.nslots; i++) {
        struct lyra_ctx_ptr *data = &ctx->gc.items[i];
        if (data->hash == 0)
            continue;
        free(data->ptr);
    }
    lyra_ctx_ptr_set_del(&ctx->gc);
}

void *lyra_ctx_mem_malloc(struct lyra_ctx *ctx, size_t size) {
    void *ptr = malloc(size);
    if (ptr == 0)
        return 0;
    return lyra_ctx_ptr_set_add(&ctx->manual, ptr, 0, 0);
}

void *lyra_ctx_mem_calloc(struct lyra_ctx *ctx, size_t nmemb,
                          size_t size) {
    void *ptr = calloc(nmemb, size);
    if (ptr == 0)
        return 0;
    return lyra_ctx_ptr_set_add(&ctx->manual, ptr, 0, 0);
}

void *lyra_ctx_mem_realloc(struct lyra_ctx *ctx, void *ptr, size_t size) {
    void *old_ptr = ptr;
    void *new_ptr = realloc(ptr, size);
    if (new_ptr != old_ptr) {
        lyra_ctx_ptr_set_remove(&ctx->manual, old_ptr);
        return lyra_ctx_ptr_set_add(&ctx->manual, new_ptr, 0, 0);
    }
    return new_ptr;
}

void lyra_ctx_mem_free(struct lyra_ctx *ctx, void *ptr) {
    lyra_ctx_ptr_set_remove(&ctx->manual, ptr);
    free(ptr);
}

static void gc_mark_ptr(struct lyra_ctx_ptr_set *gc, void *ptr) {

    size_t i, j, h, k;

    if ((uintptr_t)ptr < gc->minptr || (uintptr_t)ptr > gc->maxptr) {
        return;
    }

    i = hash(ptr) % gc->nslots;
    j = 0;

    while (1) {
        h = gc->items[i].hash;
        if (h == 0 || j > lyra_ctx_ptr_set_probe(gc, i, h)) {
            return;
        }
        if (ptr == gc->items[i].ptr) {
            if (gc->items[i].flags & TGC_MARK) {
                return;
            }
            gc->items[i].flags |= TGC_MARK;
            if (gc->items[i].flags & TGC_LEAF) {
                return;
            }
            for (k = 0; k < gc->items[i].size / sizeof(void *); k++) {
                gc_mark_ptr(gc, ((void **)gc->items[i].ptr)[k]);
            }
            return;
        }
        i = (i + 1) % gc->nslots;
        j++;
    }
}

static void gc_mark(struct lyra_ctx_ptr_set *gc) {
    size_t i, k;

    if (gc->nitems == 0) {
        return;
    }

    for (i = 0; i < gc->nslots; i++) {
        if (gc->items[i].hash == 0) {
            continue;
        }
        if (gc->items[i].flags & TGC_MARK) {
            continue;
        }
        if (gc->items[i].flags & TGC_ROOT) {
            gc->items[i].flags |= TGC_MARK;
            if (gc->items[i].flags & TGC_LEAF) {
                continue;
            }
            for (k = 0; k < gc->items[i].size / sizeof(void *); k++) {
                gc_mark_ptr(gc, ((void **)gc->items[i].ptr)[k]);
            }
            continue;
        }
    }
}

static void gc_sweep(struct lyra_ctx_ptr_set *gc) {
    size_t i, j, k, nj, nh;

    if (gc->nitems == 0) {
        return;
    }

    gc->nfrees = 0;
    for (i = 0; i < gc->nslots; i++) {
        if (gc->items[i].hash == 0) {
            continue;
        }
        if (gc->items[i].flags & TGC_MARK) {
            continue;
        }
        if (gc->items[i].flags & TGC_ROOT) {
            continue;
        }
        gc->nfrees++;
    }

    gc->frees =
        realloc(gc->frees, sizeof(struct lyra_ctx_ptr) * gc->nfrees);
    if (gc->frees == NULL) {
        return;
    }

    i = 0;
    k = 0;
    while (i < gc->nslots) {
        if (gc->items[i].hash == 0) {
            i++;
            continue;
        }
        if (gc->items[i].flags & TGC_MARK) {
            i++;
            continue;
        }
        if (gc->items[i].flags & TGC_ROOT) {
            i++;
            continue;
        }

        gc->frees[k] = gc->items[i];
        k++;
        memset(&gc->items[i], 0, sizeof(struct lyra_ctx_ptr));

        j = i;
        while (1) {
            nj = (j + 1) % gc->nslots;
            nh = gc->items[nj].hash;
            if (nh != 0 && lyra_ctx_ptr_set_probe(gc, nj, nh) > 0) {
                memcpy(&gc->items[j], &gc->items[nj],
                       sizeof(struct lyra_ctx_ptr));
                memset(&gc->items[nj], 0, sizeof(struct lyra_ctx_ptr));
                j = nj;
            } else {
                break;
            }
        }
        gc->nitems--;
    }

    for (i = 0; i < gc->nslots; i++) {
        if (gc->items[i].hash == 0) {
            continue;
        }
        if (gc->items[i].flags & TGC_MARK) {
            gc->items[i].flags &= ~TGC_MARK;
        }
    }

    lyra_ctx_ptr_set_resize_less(gc);

    gc->mitems = gc->nitems + (size_t)(gc->nitems * gc->sweepfactor) + 1;

    for (i = 0; i < gc->nfrees; i++) {
        if (gc->frees[i].ptr) {
            fprintf(stderr, "free %p\n", gc->frees[i].ptr);
            free(gc->frees[i].ptr);
        }
    }

    free(gc->frees);
    gc->frees = NULL;
    gc->nfrees = 0;
}

void lyra_ctx_gc_run(struct lyra_ctx *ctx) {
    gc_mark(&ctx->gc);
    gc_sweep(&ctx->gc);
}

void *lyra_ctx_gc_malloc(struct lyra_ctx *ctx, size_t size) {
    void *ptr = malloc(size);
    if (ptr == 0)
        return 0;
    return lyra_ctx_ptr_set_add(&ctx->gc, ptr, size, 0);
}

void *lyra_ctx_gc_malloc_root(struct lyra_ctx *ctx, size_t size) {
    void *ptr = malloc(size);
    if (ptr == 0)
        return 0;
    return lyra_ctx_ptr_set_add(&ctx->gc, ptr, size, TGC_ROOT);
}

void *lyra_ctx_gc_calloc(struct lyra_ctx *ctx, size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (ptr == 0)
        return 0;
    ptr = lyra_ctx_ptr_set_add(&ctx->gc, ptr, size, 0);
    return ptr;
}

void *lyra_ctx_gc_realloc(struct lyra_ctx *ctx, void *ptr, size_t size) {
    void *old_ptr = ptr;
    void *new_ptr = realloc(ptr, size);

    struct lyra_ctx_ptr *old_ptr_data =
        lyra_ctx_ptr_set_get_ptr(&ctx->gc, old_ptr);
    if (new_ptr != old_ptr) {
        const int old_flags = old_ptr_data->flags;
        lyra_ctx_ptr_set_remove(&ctx->gc, old_ptr);
        return lyra_ctx_ptr_set_add(&ctx->gc, new_ptr, size, old_flags);
    } else {
        old_ptr_data->size = size;
    }

    return new_ptr;
}
