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
#include "platform.h"

static struct lyra_value generate_const_i32_insn(struct lyra_insn *insn,
                                                 int32_t value) {
    insn->type = LYRA_OP_MOV_I32;
    insn->right_operand.i32 = value;
    return (struct lyra_value){
        .type = LYRA_VALUE_I32,
        .data.i32 = value,
    };
}

static struct lyra_value generate_const_f64_insn(struct lyra_insn *insn,
                                                 double value) {
    insn->type = LYRA_OP_MOV_F64;
    insn->right_operand.f64 = value;
    return (struct lyra_value){
        .type = LYRA_VALUE_F64,
        .data.f64 = value,
    };
}

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared,
                         LYRA_UNUSED struct lyra_ctx *ctx) {
    struct lyra_value *constants =
        malloc(sizeof(struct lyra_value) * shared->variables_len);
    for (size_t i = 0; i < shared->variables_len; i++)
        constants[i] = (struct lyra_value){0};

    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (!lyra_insn_type_has_dest(insn))
            continue;
        if (LYRA_BA_GET_BIT(shared->managed_vars_multiple_use,
                            insn->dest_var))
            continue;
        switch (insn->type) {
        // mov immediate instructions
        case LYRA_OP_MOV_I32: {
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_I32,
            };
            break;
        }
        case LYRA_OP_MOV_BOOL: {
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_BOOL,
            };
            break;
        }
        // Arithmetic
        case LYRA_OP_ADD_PRIM: {
            const struct lyra_value lhs = constants[insn->left_var];
            const struct lyra_value rhs =
                constants[insn->right_operand.var];
            switch (lhs.type) {
            case LYRA_VALUE_I32: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = generate_const_i32_insn(
                        insn, lhs.data.i32 + rhs.data.i32);
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, (double)lhs.data.i32 + rhs.data.f64);
                }
                break;
            }
            case LYRA_VALUE_F64: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, lhs.data.f64 + (double)rhs.data.i32);
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, lhs.data.f64 + rhs.data.f64);
                }
                break;
            }
            default:
                break;
            }
            break;
        }
#define DEF_BIN_OP(NAME, OP)                                              \
    case NAME##_PRIM: {                                                   \
        const struct lyra_value lhs = constants[insn->left_var];          \
        const struct lyra_value rhs = constants[insn->right_operand.var]; \
        switch (lhs.type) {                                               \
        case LYRA_VALUE_I32: {                                            \
            if (rhs.type == LYRA_VALUE_I32) {                             \
                constants[insn->dest_var] = generate_const_i32_insn(      \
                    insn, lhs.data.i32 OP rhs.data.i32);                  \
            } else if (rhs.type == LYRA_VALUE_F64) {                      \
                constants[insn->dest_var] = generate_const_f64_insn(      \
                    insn, (double)lhs.data.i32 OP rhs.data.f64);          \
            }                                                             \
            break;                                                        \
        }                                                                 \
        case LYRA_VALUE_F64: {                                            \
            if (rhs.type == LYRA_VALUE_I32) {                             \
                constants[insn->dest_var] = generate_const_f64_insn(      \
                    insn, lhs.data.f64 OP(double) rhs.data.i32);          \
            } else if (rhs.type == LYRA_VALUE_F64) {                      \
                constants[insn->dest_var] = generate_const_f64_insn(      \
                    insn, lhs.data.f64 OP rhs.data.f64);                  \
            }                                                             \
            break;                                                        \
        }                                                                 \
        default:                                                          \
            break;                                                        \
        }                                                                 \
        break;                                                            \
    }
            DEF_BIN_OP(LYRA_OP_SUB, -)
            DEF_BIN_OP(LYRA_OP_MUL, *)
#undef DEF_BIN_OP
        case LYRA_OP_DIV_PRIM: {
            const struct lyra_value lhs = constants[insn->left_var];
            const struct lyra_value rhs =
                constants[insn->right_operand.var];
            switch (lhs.type) {
            case LYRA_VALUE_I32: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, (double)lhs.data.i32 / (double)rhs.data.i32);
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, (double)lhs.data.i32 / rhs.data.f64);
                }
                break;
            }
            case LYRA_VALUE_F64: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, lhs.data.f64 / (double)rhs.data.i32);
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = generate_const_f64_insn(
                        insn, lhs.data.f64 / rhs.data.f64);
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        // Comparison
#define COMP_OP(LYRA_OP_BASE, C_OP)                                       \
    {                                                                     \
        const enum lyra_value_type ltype =                                \
            constants[insn->left_var].type;                               \
        const enum lyra_value_type rtype =                                \
            constants[insn->right_operand.var].type;                      \
        if (ltype == LYRA_VALUE_I32 && rtype == LYRA_VALUE_I32) {         \
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
        } else if (ltype == LYRA_VALUE_F64 && rtype == LYRA_VALUE_F64) {  \
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
