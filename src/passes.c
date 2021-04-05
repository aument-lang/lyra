#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_fill_inputs(struct lyra_block *block,
                          struct lyra_function_shared *shared,
                          struct lyra_ctx *ctx) {
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

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared,
                         struct lyra_ctx *ctx) {
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
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_I32,
            };
            break;
        }
        case LYRA_OP_MOV_BOOL: {
            if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use,
                                insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_BOOL,
            };
            break;
        }
        // Arithmetic
        case LYRA_OP_ADD_I32_IMM: {
            if (constants[insn->left_var].type == LYRA_VALUE_UNTYPED)
                continue;
            assert(constants[insn->left_var].type == LYRA_VALUE_I32);
            struct lyra_value result = (struct lyra_value){
                .data.i32 = constants[insn->left_var].data.i32 +
                            (int32_t)insn->right_operand.i32,
                .type = LYRA_VALUE_I32,
            };
            constants[insn->dest_var] = result;
            shared->variable_types[insn->dest_var] = LYRA_VALUE_I32;
            insn->type = LYRA_OP_MOV_I32;
            insn->left_var = 0;
            insn->right_operand = LYRA_INSN_I32(result.data.i32);
            continue;
        }
        // Comparison
#define COMP_OP(LYRA_OP_BASE, C_OP)                                       \
    {                                                                     \
        const enum lyra_value_type left_type =                            \
            constants[insn->left_var].type;                               \
        const enum lyra_value_type right_type =                           \
            constants[insn->right_operand.var].type;                      \
        if (left_type == LYRA_VALUE_I32 &&                                \
            right_type == LYRA_VALUE_I32) {                               \
            struct lyra_value result = (struct lyra_value){               \
                .data.i32 =                                               \
                    constants[insn->left_var]                             \
                        .data.i32 C_OP constants[insn->right_operand.var] \
                        .data.i32,                                        \
                .type = LYRA_VALUE_BOOL,                                  \
            };                                                            \
            constants[insn->dest_var] = result;                           \
            shared->variable_types[insn->dest_var] = LYRA_VALUE_BOOL;     \
            insn->type = LYRA_OP_MOV_BOOL;                                \
            insn->left_var = 0;                                           \
            insn->right_operand = LYRA_INSN_I32(result.data.i32);         \
        } else if (left_type == LYRA_VALUE_F64 &&                         \
                   right_type == LYRA_VALUE_F64) {                        \
            struct lyra_value result = (struct lyra_value){               \
                .data.i32 =                                               \
                    constants[insn->left_var]                             \
                        .data.f64 C_OP constants[insn->right_operand.var] \
                        .data.f64,                                        \
                .type = LYRA_VALUE_BOOL,                                  \
            };                                                            \
            constants[insn->dest_var] = result;                           \
            shared->variable_types[insn->dest_var] = LYRA_VALUE_BOOL;     \
            insn->type = LYRA_OP_MOV_BOOL;                                \
            insn->left_var = 0;                                           \
            insn->right_operand = LYRA_INSN_I32(result.data.i32);         \
        } /* Left is undetermined, right side is constant */              \
        else if (right_type == LYRA_VALUE_I32) {                          \
            insn->type = LYRA_OP_BASE##_NUM_I32_IMM;                          \
            insn->right_operand = LYRA_INSN_I32(                          \
                constants[insn->right_operand.var].data.i32);             \
        } else if (right_type == LYRA_VALUE_F64) {                        \
            insn->type = LYRA_OP_BASE##_NUM_F64_IMM;                          \
            insn->right_operand = LYRA_INSN_F64(                          \
                constants[insn->right_operand.var].data.f64);             \
        }                                                                 \
        continue;                                                         \
    }
        case LYRA_OP_EQ_VAR:
            COMP_OP(LYRA_OP_EQ, ==)
        case LYRA_OP_NEQ_VAR:
            COMP_OP(LYRA_OP_NEQ, !=)
        case LYRA_OP_LT_VAR:
            COMP_OP(LYRA_OP_LT, <)
        case LYRA_OP_GT_VAR:
            COMP_OP(LYRA_OP_GT, >)
        case LYRA_OP_LEQ_VAR:
            COMP_OP(LYRA_OP_LEQ, <=)
        case LYRA_OP_GEQ_VAR:
            COMP_OP(LYRA_OP_GEQ, >=)
#undef COMP_OP
        default:
            break;
        }
    }

    switch (block->connector.type) {
    case LYRA_BLOCK_JIF: {
        if (constants[block->connector.var].type == LYRA_VALUE_BOOL) {
            if (constants[block->connector.var].data.i32 == 1) {
                block->connector.type = LYRA_BLOCK_JMP;
            } else {
                block->connector.type = LYRA_BLOCK_FALLTHROUGH;
            }
        }
        break;
    }
    case LYRA_BLOCK_JNIF: {
        if (constants[block->connector.var].type == LYRA_VALUE_BOOL) {
            if (constants[block->connector.var].data.i32 == 0) {
                block->connector.type = LYRA_BLOCK_JMP;
            } else {
                block->connector.type = LYRA_BLOCK_FALLTHROUGH;
            }
        }
        break;
    }
    default:
        break;
    }

    free(constants);
    return 1;
}

int lyra_pass_purge_dead_code(struct lyra_block *block,
                              struct lyra_function_shared *shared,
                              struct lyra_ctx *ctx) {
    lyra_bit_array used_vars = malloc(LYRA_BA_LEN(shared->variables_len));
    lyra_bit_array dead_vars =
        calloc(LYRA_BA_LEN(shared->variables_len), 1);
    int changed = 1;
    while (changed) {
        changed = 0;
        memset(used_vars, 0, LYRA_BA_LEN(shared->variables_len));
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

int lyra_pass_type_inference(struct lyra_block *block,
                             struct lyra_function_shared *shared,
                             struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
#define SET_TYPE(VAR, TYPE)                                               \
    do {                                                                  \
        if (shared->variable_types[VAR] == LYRA_VALUE_UNTYPED)            \
            shared->variable_types[VAR] = TYPE;                           \
        else if (shared->variable_types[VAR] != TYPE)                     \
            shared->variable_types[VAR] = LYRA_VALUE_ANY;                 \
    } while (0)
        // Operations that result in int/double
        case LYRA_OP_MOV_I32:
        case LYRA_OP_MUL_I32_IMM:
        case LYRA_OP_ADD_I32_IMM:
        case LYRA_OP_SUB_I32_IMM: {
            switch (shared->variable_types[insn->dest_var]) {
            case LYRA_VALUE_I32:
            case LYRA_VALUE_NUM:
                break;
            case LYRA_VALUE_UNTYPED: {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_I32;
                break;
            }
            case LYRA_VALUE_F64: {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_NUM;
                break;
            }
            default: {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;
                break;
            }
            }
        }
        // Operations that only return an int
        case LYRA_OP_ENSURE_I32:
        case LYRA_OP_MOD_I32_IMM:
        case LYRA_OP_BOR_I32_IMM:
        case LYRA_OP_BXOR_I32_IMM:
        case LYRA_OP_BAND_I32_IMM:
        case LYRA_OP_BSHL_I32_IMM:
        case LYRA_OP_BSHR_I32_IMM: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_I32);
            break;
        }
        // Operations that only return float
        case LYRA_OP_MOV_F64:
        case LYRA_OP_ENSURE_F64:
        case LYRA_OP_MUL_F64_IMM:
        case LYRA_OP_DIV_I32_IMM:
        case LYRA_OP_DIV_F64_IMM:
        case LYRA_OP_ADD_F64_IMM:
        case LYRA_OP_SUB_F64_IMM: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_F64);
            break;
        }
        // Operations on number type that may return int/float
        case LYRA_OP_MUL_NUM_I32_IMM:
        case LYRA_OP_ADD_NUM_I32_IMM:
        case LYRA_OP_SUB_NUM_I32_IMM:
        case LYRA_OP_ADD_NUM_F64_IMM:
        case LYRA_OP_SUB_NUM_F64_IMM: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_NUM);
            break;
        }
        // Conditionals always return bool
        case LYRA_OP_LT_VAR:
        case LYRA_OP_LT_I32_IMM:
        case LYRA_OP_LT_F64_IMM:
        case LYRA_OP_GT_VAR:
        case LYRA_OP_GT_I32_IMM:
        case LYRA_OP_GT_F64_IMM:
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_LEQ_I32_IMM:
        case LYRA_OP_LEQ_F64_IMM:
        case LYRA_OP_GEQ_VAR:
        case LYRA_OP_GEQ_I32_IMM:
        case LYRA_OP_GEQ_F64_IMM: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_BOOL);
            break;
        }
        default: {
            if (lyra_insn_type_has_dest(insn->type)) {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;
            }
        }
#undef SET_TYPE
        }
    }
    return 1;
}

int lyra_pass_immediate_binop_dynamic_lhs(
    struct lyra_block *block, struct lyra_function_shared *shared,
    struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        case LYRA_OP_ADD_I32_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_I32) {
                const size_t new_var = lyra_function_shared_add_variable(
                    shared, LYRA_VALUE_I32, ctx);
                struct lyra_insn *new_insn = lyra_insn_imm(
                    LYRA_OP_ENSURE_I32, LYRA_INSN_REG(insn->left_var),
                    new_var, ctx);
                lyra_block_insert_insn(block, insn->prev, new_insn);
                insn->left_var = new_var;
            }
            break;
        }
        case LYRA_OP_ADD_F64_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_F64) {
                const size_t new_var = lyra_function_shared_add_variable(
                    shared, LYRA_VALUE_F64, ctx);
                struct lyra_insn *new_insn = lyra_insn_imm(
                    LYRA_OP_ENSURE_F64, LYRA_INSN_REG(insn->left_var),
                    new_var, ctx);
                lyra_block_insert_insn(block, insn->prev, new_insn);
                insn->left_var = new_var;
            }
            break;
        }
        case LYRA_OP_ADD_NUM_I32_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_NUM) {
                const size_t new_var = lyra_function_shared_add_variable(
                    shared, LYRA_VALUE_NUM, ctx);
                struct lyra_insn *new_insn = lyra_insn_imm(
                    LYRA_OP_ENSURE_NUM, LYRA_INSN_REG(insn->left_var),
                    new_var, ctx);
                lyra_block_insert_insn(block, insn->prev, new_insn);
                insn->left_var = new_var;
            } else if (
                shared->variable_types[insn->left_var] == LYRA_VALUE_I32 &&
                shared->variable_types[insn->dest_var] == LYRA_VALUE_I32
            ) {
                insn->type = LYRA_OP_ADD_I32_IMM;
            }
            break;
        }
        case LYRA_OP_ADD_NUM_F64_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_NUM) {
                const size_t new_var = lyra_function_shared_add_variable(
                    shared, LYRA_VALUE_NUM, ctx);
                struct lyra_insn *new_insn = lyra_insn_imm(
                    LYRA_OP_ENSURE_NUM, LYRA_INSN_REG(insn->left_var),
                    new_var, ctx);
                lyra_block_insert_insn(block, insn->prev, new_insn);
                insn->left_var = new_var;
            } else if (
                shared->variable_types[insn->left_var] == LYRA_VALUE_F64 &&
                shared->variable_types[insn->dest_var] == LYRA_VALUE_F64
            ) {
                insn->type = LYRA_OP_ADD_F64_IMM;
            }
            break;
        }
        default:
            break;
        }
    }
    return 1;
}
