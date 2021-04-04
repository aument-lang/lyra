#include <stdlib.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_fill_inputs(struct lyra_block *block,
                          struct lyra_function_shared *shared) {
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
            // fallthrough
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
                            struct lyra_function_shared *shared) {
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
                    lyra_function_shared_add_variable(shared, type);
                variable_mapping[insn->dest_var] = new_reg;
                insn->dest_var = new_reg;
            } else {
                LYRA_BA_SET_BIT(shared->managed_vars_set, insn->dest_var);
            }
        }
    }

    free(variable_mapping);
    return 1;
}

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared) {
    struct lyra_value *constants =
        malloc(sizeof(struct lyra_value) * shared->variables_len);
    for (size_t i = 0; i < shared->variables_len; i++)
        constants[i] = (struct lyra_value){0};

    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        // mov immediate instructions
        case LYRA_OP_MOV_I32: {
            if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use,
                                insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = (int32_t)insn->left_var,
                .type = LYRA_VALUE_I32,
            };
            break;
        }
        // Arithmetic
        case LYRA_OP_ADD_I32: {
            assert(constants[insn->left_var].type == LYRA_VALUE_I32);
            struct lyra_value result = (struct lyra_value){
                .data.i32 = constants[insn->left_var].data.i32 +
                            (int32_t)insn->right_operand.i32,
                .type = LYRA_VALUE_I32,
            };
            constants[insn->dest_var] = result;
            insn->type = LYRA_OP_MOV_I32;
            insn->left_var = 0;
            insn->right_operand = LYRA_INSN_I32(result.data.i32);
            continue;
        }
        default:
            break;
        }
    }

    free(constants);
    return 1;
}

int lyra_pass_purge_dead_code(struct lyra_block *block,
                              struct lyra_function_shared *shared) {
    lyra_bit_array used_vars =
        calloc(LYRA_BA_LEN(shared->variables_len), 1);
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (lyra_insn_type_has_left_var(insn->type))
            LYRA_BA_SET_BIT(used_vars, insn->left_var);
        if (lyra_insn_type_has_right_var(insn->type))
            LYRA_BA_SET_BIT(used_vars, insn->right_operand.var);
    }
    for (size_t i = 0; i < shared->managed_vars_len; i++) {
        if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use, i))
            LYRA_BA_SET_BIT(used_vars, i);
    }
    struct lyra_insn *insn = block->insn_first;
    while (insn != 0) {
        int has_dest_reg = lyra_insn_type_has_dest(insn->type);
        if (has_dest_reg && !LYRA_BA_GET_BIT(used_vars, insn->dest_var)) {
            printf("remove %p\n", insn);
            struct lyra_insn *insn_next = insn->next;
            lyra_block_remove_insn(block, insn);
            insn = insn_next;
        } else {
            insn = insn->next;
        }
    }
    free(used_vars);
    return 1;
}