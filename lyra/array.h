// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "context.h"
#include "exception.h"
#include "platform.h"

#define LYRA_ARRAY_COPY_A(INNER, NAME, IN_CAP, ALLOCATOR)                 \
    struct NAME {                                                         \
        INNER *data;                                                      \
        size_t len;                                                       \
        size_t cap;                                                       \
    };                                                                    \
    static LYRA_UNUSED void NAME##_add(struct NAME *array, INNER el,      \
                                       struct lyra_ctx *ctx) {            \
        if (array->cap == 0) {                                            \
            array->data = (INNER *)ALLOCATOR##_malloc(                    \
                ctx, sizeof(INNER) * IN_CAP);                             \
            array->cap = IN_CAP;                                          \
        } else if (array->len == array->cap) {                            \
            array->data = (INNER *)ALLOCATOR##_realloc(                   \
                ctx, array->data, array->cap * 2 * sizeof(INNER));        \
            array->cap *= 2;                                              \
        }                                                                 \
        array->data[array->len++] = el;                                   \
    }                                                                     \
    static LYRA_UNUSED LYRA_ALWAYS_INLINE INNER NAME##_at(                \
        const struct NAME *array, size_t idx) {                           \
        if (LYRA_UNLIKELY(idx >= array->len))                             \
            lyra_fatal_index((void *)array, idx, array->len);             \
        return array->data[idx];                                          \
    }                                                                     \
    static LYRA_UNUSED LYRA_ALWAYS_INLINE void NAME##_set(                \
        const struct NAME *array, size_t idx, INNER thing) {              \
        if (LYRA_UNLIKELY(idx >= array->len))                             \
            lyra_fatal_index((void *)array, idx, array->len);             \
        array->data[idx] = thing;                                         \
    }

#define LYRA_ARRAY_COPY(INNER, NAME, IN_CAP) LYRA_ARRAY_COPY_A(INNER, NAME, IN_CAP, lyra_ctx_mem)

#define LYRA_ARRAY_STRUCT_A(INNER, NAME, IN_CAP, ALLOCATOR)                            \
    LYRA_ARRAY_COPY_A(INNER, NAME, IN_CAP, ALLOCATOR)                                  \
    static LYRA_UNUSED LYRA_ALWAYS_INLINE const INNER *NAME##_at_ptr(     \
        const struct NAME *array, size_t idx) {                           \
        if (LYRA_UNLIKELY(idx >= array->len))                             \
            lyra_fatal_index((void *)array, idx, array->len);             \
        return &array->data[idx];                                         \
    }                                                                     \
    static LYRA_UNUSED LYRA_ALWAYS_INLINE INNER *NAME##_at_mut(           \
        struct NAME *array, size_t idx) {                                 \
        if (LYRA_UNLIKELY(idx >= array->len))                             \
            lyra_fatal_index((void *)array, idx, array->len);             \
        return &array->data[idx];                                         \
    }

#define LYRA_ARRAY_STRUCT(INNER, NAME, IN_CAP) LYRA_ARRAY_STRUCT_A(INNER, NAME, IN_CAP, lyra_ctx_mem)
