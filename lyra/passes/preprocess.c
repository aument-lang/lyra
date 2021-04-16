// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"
#include "platform.h"
#include "array.h"

LYRA_ARRAY_COPY(size_t, variable_map_array, 1)
LYRA_ARRAY_COPY(size_t *, shared_to_local_array, 1)

static inline int block_falls_through(const struct lyra_block *block, const size_t i) {
    return block->connector.type == LYRA_BLOCK_FALLTHROUGH ||
           (block->connector.type == LYRA_BLOCK_JMP &&
            block->connector.label == i + 1);
}

static inline int block_is_empty(const struct lyra_block *block) {
    return block->insn_first == 0 && block->insn_last == 0;
}

static void calculate_block_refcounts(struct lyra_function *fn, size_t *block_refcount) {
    for (size_t i = 0; i < fn->blocks.len; i++) {
        struct lyra_block *block = &fn->blocks.data[i];
        switch (block->connector.type) {
        case LYRA_BLOCK_FALLTHROUGH: {
            if ((i + 1) < fn->blocks.len) {
                block_refcount[i + 1]++;
            }
            break;
        }
        case LYRA_BLOCK_JMP: {
            block_refcount[block->connector.label]++;
            break;
        }
        case LYRA_BLOCK_JIF:
        case LYRA_BLOCK_JNIF: {
            block_refcount[block->connector.label]++;
            if ((i + 1) < fn->blocks.len) {
                block_refcount[i + 1]++;
            }
            break;
        }
        default:
            break;
        }
    }
}

void lyra_function_combine_blocks(struct lyra_function *fn) {
    if (fn->blocks.len <= 1)
        return;
    size_t *block_mapping = malloc(sizeof(size_t) * fn->blocks.len);
    for (size_t i = 0; i < fn->blocks.len; i++)
        block_mapping[i] = i;

    size_t *block_refcount = calloc(fn->blocks.len, sizeof(size_t));
    calculate_block_refcounts(fn, block_refcount);

    // Fuse all fallthrough blocks into one
    for (size_t i = 0; i < fn->blocks.len - 1; i++) {
        struct lyra_block *block = &fn->blocks.data[i];
        if (block_falls_through(block, i) &&
            ((block_refcount[i] <= 1) || block_is_empty(block))) {
            struct lyra_block *next_block = &fn->blocks.data[i + 1];

            if (block->insn_last != 0)
                block->insn_last->next = next_block->insn_first;
            if (next_block->insn_first != 0)
                next_block->insn_first->prev = block->insn_last;
            next_block->insn_first = block->insn_first;
            if (next_block->insn_last == 0)
                next_block->insn_last = block->insn_last;

            block->insn_first = 0;
            block->insn_last = 0;
            block->connector.type = LYRA_BLOCK_FALLTHROUGH;
            block_mapping[i] = i + 1;
        }
    }
    for (size_t i = 0; i < fn->blocks.len; i++) {
        struct lyra_block *block = &fn->blocks.data[i];
        if (lyra_block_connector_type_is_jmp(block->connector.type)) {
            block->connector.label = block_mapping[block->connector.label];
        }
    }

    // Reset refcounts
    memset(block_refcount, 0, fn->blocks.len * sizeof(size_t));
    calculate_block_refcounts(fn, block_refcount);

    // Remove all unreachable blocks (including empty ones)
    size_t block_placement = 0;
    for (size_t i = 0; i < fn->blocks.len; i++) {
        struct lyra_block *block = &fn->blocks.data[i];
        block_mapping[i] = block_placement;
        if (block_refcount[i] > 0) {
            fn->blocks.data[block_placement] = *block;
            block_placement++;
        }
    }

    fn->blocks.data =
        lyra_ctx_gc_realloc(fn->ctx, fn->blocks.data,
                            block_placement * sizeof(struct lyra_block));
    fn->blocks.len = block_placement;

    free(block_refcount);
    free(block_mapping);
}

int lyra_pass_check_multiple_use(struct lyra_block *block,
                                 struct lyra_function_shared *shared,
                                 LYRA_UNUSED struct lyra_ctx *ctx) {
    lyra_bit_array owned_vars =
        calloc(LYRA_BA_LEN(shared->managed_vars_len), 1);
    lyra_bit_array used_vars =
        calloc(LYRA_BA_LEN(shared->managed_vars_len), 1);
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (lyra_insn_type_has_dest(insn->type)) {
            assert(insn->dest_var < shared->managed_vars_len);
            LYRA_BA_SET_BIT(owned_vars, insn->dest_var);
        }
        if (lyra_insn_type_has_left_var(insn->type)) {
            assert(insn->left_var < shared->managed_vars_len);
            if (!LYRA_BA_GET_BIT(owned_vars, insn->left_var)) {
                LYRA_BA_SET_BIT(shared->managed_vars_multiple_use,
                                insn->left_var);
            }
            LYRA_BA_SET_BIT(used_vars, insn->left_var);
        }
        if (lyra_insn_type_has_right_var(insn->type)) {
            assert(insn->right_operand.var < shared->managed_vars_len);
            if (!LYRA_BA_GET_BIT(owned_vars, insn->right_operand.var)) {
                LYRA_BA_SET_BIT(shared->managed_vars_multiple_use,
                                insn->right_operand.var);
            }
            LYRA_BA_SET_BIT(used_vars, insn->right_operand.var);
        }
    }
    if (lyra_block_connector_type_has_var(block->connector.type)) {
        if (!LYRA_BA_GET_BIT(owned_vars, block->connector.var)) {
            LYRA_BA_SET_BIT(shared->managed_vars_multiple_use,
                            block->connector.var);
        }
    }
    free(owned_vars);
    free(used_vars);
    return 1;
}

int lyra_pass_check_multiple_set(struct lyra_block *block,
                                 struct lyra_function_shared *shared,
                                 LYRA_UNUSED struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (lyra_insn_type_has_dest(insn->type) &&
            lyra_function_shared_is_var_multiple_use(shared,
                                                     insn->dest_var))
            assert(insn->type == LYRA_OP_MOV_VAR);
    }
    return 1;
}

// TODO: clean up these passes

void lyra_function_into_semi_ssa(struct lyra_function *fn) {
    struct variable_map_array variable_mapping = (struct variable_map_array) {
        .data = calloc(fn->shared.managed_vars_len, sizeof(size_t)),
        .len = fn->shared.managed_vars_len,
        .cap = fn->shared.managed_vars_len,
    };

    struct shared_to_local_array shared_to_local_mapping = (struct shared_to_local_array) {
        .data = calloc(fn->shared.managed_vars_len, sizeof(size_t*)),
        .len = fn->shared.managed_vars_len,
        .cap = fn->shared.managed_vars_len,
    };
    for(size_t var_idx = 0; var_idx < LYRA_BA_LEN(fn->shared.managed_vars_len); v++) {
        if (LYRA_BA_GET(fn->shared.managed_vars_multiple_use, var_idx)) {
            shared_to_local_mapping.data[var_idx] = calloc(fn->blocks.len, sizeof(size_t));
            for (size_t block_idx = 0; block_idx < fn->blocks.len; block_idx++)
                shared_to_local_mapping.data[var_idx][block_idx] = var_idx;
        }
    }

    for(size_t block_idx = 0; block_idx < fn->blocks.len; block_idx++) {
        struct lyra_block *block = &fn->blocks.data[block_idx];

        for (size_t i = 0; i < variable_mapping.len; i++)
            variable_mapping.data[i] = i;

        for (struct lyra_insn *insn = block->insn_first; insn != 0;
            insn = insn->next) {
            int has_dest_reg = lyra_insn_type_has_dest(insn->type);

            if (lyra_insn_type_has_left_var(insn->type))
                insn->left_var =
                    variable_map_array_at(&variable_mapping, insn->left_var);

            if (lyra_insn_type_has_right_var(insn->type))
                insn->right_operand.var =
                    variable_map_array_at(&variable_mapping, insn->right_operand.var);
            else if (insn->type == LYRA_OP_CALL) {
                struct lyra_insn_call_args *args =
                    insn->right_operand.call_args;
                for (size_t i = 0; i < args->length; i++)
                    args->data[i] = variable_map_array_at(&variable_mapping, args->data[i]);
                if (lyra_insn_call_args_has_return(args))
                    has_dest_reg = 1;
            }

            if (has_dest_reg) {
                enum lyra_value_type type =
                    fn->shared.variable_types[insn->dest_var];
                const size_t new_reg = lyra_function_add_variable(fn, type);
                if (lyra_function_shared_is_var_multiple_use(&fn->shared,
                                                            insn->dest_var)) {
                    shared_to_local_mapping.data[insn->dest_var][block_idx] = new_reg;
                }
                variable_map_array_set(&variable_mapping, insn->dest_var, new_reg);
                insn->dest_var = new_reg;
            }
        }

        if (lyra_block_connector_type_has_var(block->connector.type)) {
            block->connector.var = variable_mapping.data[block->connector.var];
        }
    }

    free(variable_mapping.data);
    free(shared_to_local_mapping.data);
}

void lyra_function_defrag_vars(struct lyra_function *fn) {
    size_t *variable_mapping =
        malloc(sizeof(size_t) * fn->shared.variables_len);
    for (size_t i = 0; i < fn->shared.variables_len; i++)
        variable_mapping[i] = i;

    size_t placement = 0;
    for (size_t i = 0; i < fn->shared.variables_len; i++) {
        if (fn->shared.variable_types[i] == LYRA_VALUE_UNTYPED) {
            variable_mapping[i] = (size_t)-1;
            continue;
        }
        assert(placement < i);
        variable_mapping[i] = placement;
        fn->shared.variable_types[placement] =
            fn->shared.variable_types[i];
        placement++;
    }
    fn->shared.variables_len = placement;

    for (size_t n_block = 0; n_block < fn->blocks.len; n_block++) {
        struct lyra_block *block = &fn->blocks.data[n_block];
        for (struct lyra_insn *insn = block->insn_first; insn != 0;
             insn = insn->next) {
            int has_dest_reg = lyra_insn_type_has_dest(insn->type);

            if (lyra_insn_type_has_left_var(insn->type))
                insn->left_var = variable_mapping[insn->left_var];

            if (lyra_insn_type_has_right_var(insn->type))
                insn->right_operand.var =
                    variable_mapping[insn->right_operand.var];
            else if (insn->type == LYRA_OP_CALL) {
                struct lyra_insn_call_args *args =
                    insn->right_operand.call_args;
                for (size_t i = 0; i < args->length; i++)
                    args->data[i] = variable_mapping[args->data[i]];
                if (lyra_insn_call_args_has_return(args))
                    has_dest_reg = 1;
            }

            if (has_dest_reg) {
                insn->dest_var = variable_mapping[insn->dest_var];
            }
        }

        if (lyra_block_connector_type_has_var(block->connector.type)) {
            block->connector.var = variable_mapping[block->connector.var];
        }
    }

    free(variable_mapping);
}
