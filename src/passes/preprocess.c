#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"
#include "platform.h"

int lyra_pass_fill_inputs(struct lyra_block *block,
                          struct lyra_function_shared *shared,
                          LYRA_UNUSED struct lyra_ctx *ctx) {
    lyra_bit_array owned_vars =
        calloc(LYRA_BA_LEN(shared->managed_vars_len), 1);
    lyra_bit_array used_vars =
        calloc(LYRA_BA_LEN(shared->managed_vars_len), 1);
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        // mov instructions
        case LYRA_OP_MOV_I32:
        case LYRA_OP_MOV_F64: {
            LYRA_BA_SET_BIT(owned_vars, insn->left_var);
            LYRA_FALLTHROUGH;
        }
        case LYRA_OP_MOV_VAR: {
            LYRA_BA_SET_BIT(owned_vars, insn->dest_var);
            break;
        }
        // everything else
        default: {
            if (lyra_insn_type_has_left_var(insn->type))
                LYRA_BA_SET_BIT(used_vars, insn->left_var);
            if (lyra_insn_type_has_right_var(insn->type))
                LYRA_BA_SET_BIT(used_vars, insn->right_operand.var);
            break;
        }
        }
    }
    for (size_t i = 0; i < shared->managed_vars_len; i++) {
        if (LYRA_BA_GET_BIT(used_vars, i) &&
            !LYRA_BA_GET_BIT(owned_vars, i)) {
            LYRA_BA_SET_BIT(shared->managed_vars_multiple_use, i);
        }
    }
    free(owned_vars);
    free(used_vars);
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
        if (has_dest_reg) {
            if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use,
                                insn->dest_var))
                LYRA_BA_SET_BIT(shared->managed_vars_set, insn->dest_var);
            if (LYRA_BA_GET_BIT(shared->managed_vars_set,
                                insn->dest_var)) {
                enum lyra_value_type type =
                    shared->variable_types[insn->dest_var];
                const size_t new_reg =
                    lyra_function_shared_add_variable(shared, type, ctx);
                variable_mapping[insn->dest_var] = new_reg;
                insn->dest_var = new_reg;
            } else {
                LYRA_BA_SET_BIT(shared->managed_vars_set, insn->dest_var);
            }
        }
    }

    if (lyra_block_connector_type_has_var(block->connector.type)) {
        block->connector.var = variable_mapping[block->connector.var];
    }

    free(variable_mapping);
    return 1;
}
