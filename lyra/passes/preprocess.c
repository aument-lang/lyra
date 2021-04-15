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

static inline size_t size_t_array_at(size_t *a, size_t n, size_t idx) {
    if (idx >= n)
        lyra_fatal_index(a, idx, n);
    return a[idx];
}

int lyra_pass_into_semi_ssa(struct lyra_block *block,
                            struct lyra_function_shared *shared,
                            struct lyra_ctx *ctx) {
    size_t *variable_mapping =
        malloc(sizeof(size_t) * shared->managed_vars_len);
    for (size_t i = 0; i < shared->managed_vars_len; i++)
        variable_mapping[i] = i;

    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        int has_dest_reg = lyra_insn_type_has_dest(insn->type);

        if (lyra_insn_type_has_left_var(insn->type))
            insn->left_var =
                size_t_array_at(variable_mapping, shared->managed_vars_len,
                                insn->left_var);

        if (lyra_insn_type_has_right_var(insn->type))
            insn->right_operand.var =
                size_t_array_at(variable_mapping, shared->managed_vars_len,
                                insn->right_operand.var);
        else if (insn->type == LYRA_OP_CALL) {
            struct lyra_insn_call_args *args =
                insn->right_operand.call_args;
            for (size_t i = 0; i < args->length; i++)
                args->data[i] = size_t_array_at(variable_mapping,
                                                shared->managed_vars_len,
                                                args->data[i]);
            if (lyra_insn_call_args_has_return(args))
                has_dest_reg = 1;
        }

        if (has_dest_reg) {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            enum lyra_value_type type =
                shared->variable_types[insn->dest_var];
            const size_t new_reg =
                lyra_function_shared_add_variable(shared, type, ctx);
            variable_mapping[insn->dest_var] = new_reg;
            insn->dest_var = new_reg;
        }
    }

    if (lyra_block_connector_type_has_var(block->connector.type)) {
        block->connector.var = variable_mapping[block->connector.var];
    }

    free(variable_mapping);
    return 1;
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
        fn->shared.variable_types[placement] = fn->shared.variable_types[i];
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
                insn->right_operand.var = variable_mapping[insn->right_operand.var];
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

