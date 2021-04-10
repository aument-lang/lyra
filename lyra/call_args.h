// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bc_data/types.txt"
#include "context.h"

#define LYRA_INSN_CALL_NO_RET_FLAG (1 << 0)
#define LYRA_INSN_CALL_FLAT_ARGS_FLAG (1 << 1)

struct lyra_insn_call_args {
    enum {
        LYRA_INSN_CALL_ARGS_IDX,
        LYRA_INSN_CALL_ARGS_STRING,
    } name_type;
    union {
        size_t idx;
        const char *string;
    } name;
    uint32_t flags;
    size_t length;
    size_t data[];
};

static inline int
lyra_insn_call_args_has_return(const struct lyra_insn_call_args *args) {
    return (args->flags & LYRA_INSN_CALL_NO_RET_FLAG) == 0;
}

struct lyra_insn_call_args *
lyra_insn_call_args_new_idx(size_t idx, size_t length,
                            struct lyra_ctx *ctx);

struct lyra_insn_call_args *
lyra_insn_call_args_new_name(const char *name, size_t length,
                             struct lyra_ctx *ctx);

struct lyra_comp;
void lyra_insn_call_args_comp_name(struct lyra_insn_call_args *args,
                                   struct lyra_comp *comp);