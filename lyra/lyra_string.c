// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <string.h>

#include "lyra_string.h"

struct lyra_string *lyra_string_from_const(struct lyra_ctx *ctx,
                                           const char *data, size_t len) {
    struct lyra_string *header =
        lyra_ctx_gc_malloc(ctx, sizeof(struct lyra_string) + len);
    header->len = len;
    memcpy(header->data, data, len);
    return header;
}

struct lyra_string *lyra_string_add(struct lyra_ctx *ctx,
                                    const struct lyra_string *left,
                                    const struct lyra_string *right) {
    const size_t len = left->len + right->len;
    struct lyra_string *header =
        lyra_ctx_gc_malloc(ctx, sizeof(struct lyra_string) + len);
    header->len = len;
    memcpy(header->data, left->data, left->len);
    memcpy(&header->data[left->len], right->data, right->len);
    return header;
}
