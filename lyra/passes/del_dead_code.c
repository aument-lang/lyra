// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information

#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_purge_dead_code(struct lyra_block *block,
                              struct lyra_function_shared *shared,
                              LYRA_UNUSED struct lyra_ctx *ctx) {
    lyra_bit_array used_vars = malloc(LYRA_BA_LEN(shared->variables_len));
    lyra_bit_array dead_vars =
        calloc(LYRA_BA_LEN(shared->variables_len), 1);
    int changed = 1;
    while (changed) {
        changed = 0;
        memset(used_vars, 0, LYRA_BA_LEN(shared->variables_len));

        for (struct lyra_insn *insn = block->insn_first; insn != 0;
             insn = insn->next) {
            if (lyra_insn_type_has_left_var(insn->type)) {
                // printf("%d: mark %ld\n", insn->type, insn->left_var);
                LYRA_BA_SET_BIT(used_vars, insn->left_var);
            }

            if (lyra_insn_type_has_right_var(insn->type)) {
                LYRA_BA_SET_BIT(used_vars, insn->right_operand.var);
            } else if (insn->type == LYRA_OP_CALL ||
                       insn->type == LYRA_OP_CALL_FLAT) {
                struct lyra_insn_call_args *args =
                    insn->right_operand.call_args;
                for (size_t i = 0; i < args->length; i++) {
                    LYRA_BA_SET_BIT(used_vars, args->data[i]);
                }
                LYRA_BA_SET_BIT(used_vars, insn->dest_var);
            }
        }
        for (size_t i = 0; i < shared->managed_vars_len; i++) {
            if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use, i)) {
                LYRA_BA_SET_BIT(used_vars, i);
            }
        }
        if (lyra_block_connector_type_has_var(block->connector.type)) {
            LYRA_BA_SET_BIT(used_vars, block->connector.var);
        }

        struct lyra_insn *insn = block->insn_first;
        while (insn != 0) {
            int has_dest_reg = lyra_insn_type_has_dest(insn->type);
            if (!lyra_insn_type_has_side_effect(insn->type) &&
                has_dest_reg &&
                !LYRA_BA_GET_BIT(used_vars, insn->dest_var)) {
                changed = 1;
                LYRA_BA_SET_BIT(dead_vars, insn->dest_var);
                struct lyra_insn *insn_next = insn->next;
                lyra_block_remove_insn(block, insn);
                insn = insn_next;
            } else {
                insn = insn->next;
            }
        }
    }
    for (size_t i = 0; i < shared->variables_len; i++)
        if (LYRA_BA_GET_BIT(dead_vars, i))
            shared->variable_types[i] = LYRA_VALUE_UNTYPED;
    free(used_vars);
    free(dead_vars);
    return 1;
}
