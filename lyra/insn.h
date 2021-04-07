// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bc_data/types.txt"
#include "context.h"

struct lyra_insn_call_args {
    size_t fn_idx;
    size_t length;
    size_t data[];
};

static inline struct lyra_insn_call_args *
lyra_insn_call_args_new(size_t fn_idx, size_t length,
                        struct lyra_ctx *ctx) {
    struct lyra_insn_call_args *args = lyra_ctx_gc_malloc(
        ctx, sizeof(struct lyra_insn_call_args) + length);
    args->fn_idx = fn_idx;
    args->length = length;
    for (size_t i = 0; i < length; i++)
        args->data[i] = 0;
    return args;
}

union lyra_insn_operand {
    int32_t i32;
    double f64;
    size_t var;
    struct lyra_insn_call_args *call_args;
};

#define LYRA_INSN_I32(X) ((union lyra_insn_operand){.i32 = (X)})
#define LYRA_INSN_BOOL(X) ((union lyra_insn_operand){.i32 = (X)})
#define LYRA_INSN_F64(X) ((union lyra_insn_operand){.f64 = (X)})
#define LYRA_INSN_REG(X) ((union lyra_insn_operand){.var = (X)})
#define LYRA_INSN_CALL_ARGS(X)                                            \
    ((union lyra_insn_operand){.call_args = (X)})

struct lyra_insn {
    struct lyra_insn *prev;
    struct lyra_insn *next;
    size_t dest_var;
    size_t left_var;
    union lyra_insn_operand right_operand;
    enum lyra_insn_type type;
};

struct lyra_insn *lyra_insn_new(enum lyra_insn_type type, size_t left_var,
                                union lyra_insn_operand right_operand,
                                size_t dest_var, struct lyra_ctx *ctx);

static inline struct lyra_insn *
lyra_insn_imm(enum lyra_insn_type type,
              union lyra_insn_operand right_operand, size_t dest_var,
              struct lyra_ctx *ctx) {
    return lyra_insn_new(type, 0, right_operand, dest_var, ctx);
}

struct lyra_comp;
void lyra_insn_comp(struct lyra_insn *insn, struct lyra_comp *c);

void lyra_insn_print(struct lyra_insn *insn);
