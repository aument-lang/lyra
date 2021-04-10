// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include "call_args.h"
#include "comp.h"

struct lyra_insn_call_args *
lyra_insn_call_args_new_idx(size_t idx, size_t length,
                            struct lyra_ctx *ctx) {
    struct lyra_insn_call_args *args = lyra_ctx_gc_malloc(
        ctx, sizeof(struct lyra_insn_call_args) + length * sizeof(size_t));
    args->name_type = LYRA_INSN_CALL_ARGS_IDX;
    args->name.idx = idx;
    args->flags = 0;
    args->length = length;
    for (size_t i = 0; i < length; i++)
        args->data[i] = 0;
    return args;
}

struct lyra_insn_call_args *
lyra_insn_call_args_new_name(const char *name, size_t length,
                             struct lyra_ctx *ctx) {
    struct lyra_insn_call_args *args = lyra_ctx_gc_malloc(
        ctx, sizeof(struct lyra_insn_call_args) + length * sizeof(size_t));
    args->name_type = LYRA_INSN_CALL_ARGS_STRING;
    args->name.string = name;
    args->flags = 0;
    args->length = length;
    for (size_t i = 0; i < length; i++)
        args->data[i] = 0;
    return args;
}

void lyra_insn_call_args_comp_name(struct lyra_insn_call_args *args,
                                   struct lyra_comp *comp) {
    switch (args->name_type) {
    case LYRA_INSN_CALL_ARGS_IDX: {
        lyra_comp_print_str(comp, "f");
        lyra_comp_print_isize(comp, args->name.idx);
        break;
    }
    case LYRA_INSN_CALL_ARGS_STRING: {
        lyra_comp_print_str(comp, args->name.string);
        break;
    }
    }
}
