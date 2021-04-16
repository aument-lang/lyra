// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include "function.h"
#include "comp.h"
#include "context.h"
#include "insn.h"

size_t
lyra_function_shared_add_variable(struct lyra_function_shared *shared,
                                  enum lyra_value_type type,
                                  struct lyra_ctx *ctx) {
    size_t idx = shared->variables_len;
    shared->variable_types = lyra_ctx_mem_realloc(
        ctx, shared->variable_types,
        sizeof(enum lyra_value_type) * (shared->variables_len + 1));
    shared->variable_types[idx] = type;
    shared->variables_len++;
    return idx;
}

struct lyra_function *lyra_function_new(size_t idx, size_t num_args,
                                        struct lyra_ctx *ctx) {
    struct lyra_function *fn =
        lyra_ctx_gc_malloc_root(ctx, sizeof(struct lyra_function));
    fn->blocks = (struct lyra_block_array){0};
    fn->shared = (struct lyra_function_shared){0};
    fn->idx = idx;
    fn->num_args = num_args;
    fn->ctx = ctx;
    return fn;
}

size_t lyra_function_add_block(struct lyra_function *fn,
                               struct lyra_block block) {
    size_t idx = fn->blocks.len;
    lyra_block_array_add(&fn->blocks, block, fn->ctx);
    return idx;
}

size_t lyra_function_add_variable(struct lyra_function *fn,
                                  enum lyra_value_type type) {
    return lyra_function_shared_add_variable(&fn->shared, type, fn->ctx);
}

void lyra_function_reset_managed_vars(struct lyra_function *fn) {
    if (fn->shared.managed_vars_len != 0) {
        lyra_ctx_mem_free(fn->ctx, fn->shared.managed_vars_multiple_use);
    }
    fn->shared.managed_vars_multiple_use = lyra_ctx_mem_calloc(
        fn->ctx, LYRA_BA_LEN(fn->shared.variables_len), 1);
    fn->shared.managed_vars_len = fn->shared.variables_len;
}

void lyra_function_comp(struct lyra_function *fn, struct lyra_comp *c) {
    lyra_comp_print_str(c, "au_value_t f");
    lyra_comp_print_isize(c, fn->idx);
    lyra_comp_print_str(c, "(au_value_t *args) {\n");
    for (size_t i = 0; i < fn->shared.variables_len; i++) {
        const enum lyra_value_type type = fn->shared.variable_types[i];
        if (type == LYRA_VALUE_UNTYPED)
            continue;
        lyra_comp_print_str(c, "  ");
        lyra_comp_print_str(c, lyra_value_type_c(type));
        lyra_comp_print_str(c, " v");
        lyra_comp_print_i32(c, i);
        lyra_comp_print_str(c, ";\n");
    }
    for (size_t i = 0; i < fn->blocks.len; i++) {
        lyra_comp_print_str(c, "L");
        lyra_comp_print_isize(c, i);
        lyra_comp_print_str(c, ":\n");

        const struct lyra_block *block = &fn->blocks.data[i];
        for (struct lyra_insn *insn = block->insn_first; insn != 0;
             insn = insn->next) {
            lyra_comp_print_str(c, "  ");
            lyra_insn_comp(insn, c);
            lyra_comp_print_str(c, "\n");
        }
        if (block->connector.type != LYRA_BLOCK_FALLTHROUGH) {
            lyra_comp_print_str(c, "  ");
            lyra_block_connector_comp(&block->connector, &fn->shared, c);
            lyra_comp_print_str(c, "\n");
        }
    }
    lyra_comp_print_str(c, "}");
}

int lyra_function_all_blocks(struct lyra_function *fn,
                             lyra_block_mutator_fn_t mutator) {
    for (size_t i = 0; i < fn->blocks.len; i++) {
        if (!mutator(&fn->blocks.data[i], &fn->shared, fn->ctx))
            return 0;
    }
    return 1;
}
