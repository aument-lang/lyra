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

static struct lyra_insn *cast_to_any(struct lyra_insn *insn,
                                     enum lyra_value_type type,
                                     struct lyra_block *block,
                                     struct lyra_function_shared *shared,
                                     struct lyra_ctx *ctx) {
    const size_t any_var = insn->dest_var;
    const size_t tmp_var =
        lyra_function_shared_add_variable(shared, type, ctx);
    struct lyra_insn *new_insn = lyra_insn_new(
        insn->type, insn->left_var, insn->right_operand, tmp_var, ctx);
    lyra_block_insert_insn(block, insn->prev, new_insn);

    insn->type = lyra_value_type_to_any_op(type);
    insn->left_var = tmp_var;
    insn->dest_var = any_var;
    return new_insn;
}

static void generate_const_insn(struct lyra_insn *insn,
                                struct lyra_value value,
                                struct lyra_block *block,
                                struct lyra_function_shared *shared,
                                struct lyra_ctx *ctx) {
    if (value.type == LYRA_VALUE_UNTYPED)
        return;
    if (shared->variable_types[insn->dest_var] == LYRA_VALUE_ANY) {
        insn = cast_to_any(insn, value.type, block, shared, ctx);
    }
    switch (value.type) {
    case LYRA_VALUE_BOOL:
    case LYRA_VALUE_I32: {
        insn->type = LYRA_OP_MOV_I32;
        insn->right_operand.i32 = value.data.i32;
        break;
    }
    case LYRA_VALUE_F64: {
        insn->type = LYRA_OP_MOV_F64;
        insn->right_operand.f64 = value.data.f64;
        break;
    }
    case LYRA_VALUE_STR: {
        insn->type = LYRA_OP_MOV_STR;
        insn->right_operand.str = value.data.str;
        break;
    }
    default:
        abort();
    }
}

static int into_const_mov(struct lyra_insn *insn, struct lyra_value value,
                          LYRA_UNUSED struct lyra_block *block,
                          LYRA_UNUSED struct lyra_function_shared *shared,
                          LYRA_UNUSED struct lyra_ctx *ctx) {
    if (shared->variable_types[insn->dest_var] == LYRA_VALUE_ANY) {
        insn = cast_to_any(insn, value.type, block, shared, ctx);
    }
    switch (value.type) {
    case LYRA_VALUE_I32: {
        insn->type = LYRA_OP_MOV_I32;
        insn->right_operand = LYRA_INSN_I32(value.data.i32);
        return 1;
    }
    case LYRA_VALUE_F64: {
        insn->type = LYRA_OP_MOV_F64;
        insn->right_operand = LYRA_INSN_F64(value.data.f64);
        return 1;
    }
    default:
        return 0;
    }
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
        if (!lyra_insn_type_has_dest(insn->type))
            continue;
        switch (insn->type) {
        // mov immediate instructions
        case LYRA_OP_MOV_I32: {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_I32,
            };
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        case LYRA_OP_MOV_F64: {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.f64 = insn->right_operand.f64,
                .type = LYRA_VALUE_F64,
            };
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        case LYRA_OP_MOV_BOOL: {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.i32 = insn->right_operand.i32,
                .type = LYRA_VALUE_BOOL,
            };
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        case LYRA_OP_MOV_STR: {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            constants[insn->dest_var] = (struct lyra_value){
                .data.str = insn->right_operand.str,
                .type = LYRA_VALUE_STR,
            };
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        // mov var->var instructions
        case LYRA_OP_MOV_VAR: {
            if (into_const_mov(insn, constants[insn->left_var], block,
                               shared, ctx)) {
                constants[insn->dest_var] = constants[insn->left_var];
                continue;
            }
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            constants[insn->dest_var] = constants[insn->left_var];
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        // Casting
        case LYRA_OP_ENSURE_F64_PRIM: {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
            if (constants[insn->left_var].type == LYRA_VALUE_I32) {
                constants[insn->dest_var] = (struct lyra_value){
                    .data.f64 = (double)constants[insn->left_var].data.i32,
                    .type = LYRA_VALUE_F64,
                };
                generate_const_insn(insn, constants[insn->dest_var], block,
                                    shared, ctx);
            }
            break;
        }
        // Arithmetic
        case LYRA_OP_ADD_VAR:
        case LYRA_OP_ADD_STR:
        case LYRA_OP_ADD_PRIM:
        case LYRA_OP_ADD_NUM_I32:
        case LYRA_OP_ADD_NUM_F64: {
            const struct lyra_value lhs = constants[insn->left_var];
            const struct lyra_value rhs =
                constants[insn->right_operand.var];
            switch (lhs.type) {
            case LYRA_VALUE_I32: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.i32 = lhs.data.i32 + rhs.data.i32,
                        .type = LYRA_VALUE_I32,
                    };
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = (double)lhs.data.i32 + rhs.data.f64,
                        .type = LYRA_VALUE_F64,
                    };
                }
                break;
            }
            case LYRA_VALUE_F64: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = lhs.data.f64 + (double)rhs.data.i32,
                        .type = LYRA_VALUE_F64,
                    };
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = lhs.data.f64 + rhs.data.f64,
                        .type = LYRA_VALUE_F64,
                    };
                }
                break;
            }
            case LYRA_VALUE_STR: {
                if (rhs.type == LYRA_VALUE_STR) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.str = lyra_string_add(ctx, lhs.data.str,
                                                    rhs.data.str),
                        .type = LYRA_VALUE_STR,
                    };
                }
                break;
            }
            default:
                break;
            }
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
#define DEF_BIN_OP(OP)                                                    \
    {                                                                     \
        const struct lyra_value lhs = constants[insn->left_var];          \
        const struct lyra_value rhs = constants[insn->right_operand.var]; \
        switch (lhs.type) {                                               \
        case LYRA_VALUE_I32: {                                            \
            if (rhs.type == LYRA_VALUE_I32) {                             \
                constants[insn->dest_var] = (struct lyra_value){          \
                    .data.i32 = lhs.data.i32 OP rhs.data.i32,             \
                    .type = LYRA_VALUE_I32,                               \
                };                                                        \
            } else if (rhs.type == LYRA_VALUE_F64) {                      \
                constants[insn->dest_var] = (struct lyra_value){          \
                    .data.f64 = (double)lhs.data.i32 OP rhs.data.f64,     \
                    .type = LYRA_VALUE_F64,                               \
                };                                                        \
            }                                                             \
            break;                                                        \
        }                                                                 \
        case LYRA_VALUE_F64: {                                            \
            if (rhs.type == LYRA_VALUE_I32) {                             \
                constants[insn->dest_var] = (struct lyra_value){          \
                    .data.f64 = lhs.data.f64 OP(double) rhs.data.i32,     \
                    .type = LYRA_VALUE_F64,                               \
                };                                                        \
            } else if (rhs.type == LYRA_VALUE_F64) {                      \
                constants[insn->dest_var] = (struct lyra_value){          \
                    .data.i32 = lhs.data.f64 + rhs.data.f64,              \
                    .type = LYRA_VALUE_F64,                               \
                };                                                        \
            }                                                             \
            break;                                                        \
        }                                                                 \
        default:                                                          \
            break;                                                        \
        }                                                                 \
        generate_const_insn(insn, constants[insn->dest_var], block,       \
                            shared, ctx);                                 \
        break;                                                            \
    }
        case LYRA_OP_SUB_VAR:
        case LYRA_OP_SUB_PRIM:
        case LYRA_OP_SUB_NUM_I32:
        case LYRA_OP_SUB_NUM_F64:
            DEF_BIN_OP(-)
        case LYRA_OP_MUL_VAR:
        case LYRA_OP_MUL_PRIM:
        case LYRA_OP_MUL_NUM_I32:
        case LYRA_OP_MUL_NUM_F64:
            DEF_BIN_OP(*)
#undef DEF_BIN_OP
        case LYRA_OP_DIV_VAR:
        case LYRA_OP_DIV_PRIM: {
            const struct lyra_value lhs = constants[insn->left_var];
            const struct lyra_value rhs =
                constants[insn->right_operand.var];
            switch (lhs.type) {
            case LYRA_VALUE_I32: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 =
                            (double)lhs.data.i32 / (double)rhs.data.i32,
                        .type = LYRA_VALUE_F64,
                    };
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = (double)lhs.data.i32 / rhs.data.f64,
                        .type = LYRA_VALUE_F64,
                    };
                }
                break;
            }
            case LYRA_VALUE_F64: {
                if (rhs.type == LYRA_VALUE_I32) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = lhs.data.f64 / (double)rhs.data.i32,
                        .type = LYRA_VALUE_F64,
                    };
                } else if (rhs.type == LYRA_VALUE_F64) {
                    constants[insn->dest_var] = (struct lyra_value){
                        .data.f64 = lhs.data.f64 / rhs.data.f64,
                        .type = LYRA_VALUE_F64,
                    };
                }
                break;
            }
            default:
                break;
            }
            generate_const_insn(insn, constants[insn->dest_var], block,
                                shared, ctx);
            break;
        }
        case LYRA_OP_MOD_I32: {
            const struct lyra_value lhs = constants[insn->left_var];
            const struct lyra_value rhs =
                constants[insn->right_operand.var];
            if (lhs.type == LYRA_VALUE_I32 && rhs.type == LYRA_VALUE_I32) {
                constants[insn->dest_var] = (struct lyra_value){
                    .data.f64 = lhs.data.f64 / rhs.data.f64,
                    .type = LYRA_VALUE_F64,
                };
                generate_const_insn(insn, constants[insn->dest_var], block,
                                    shared, ctx);
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
        int32_t result = 0;                                               \
        int has_result = 0;                                               \
        if (ltype == LYRA_VALUE_I32 && rtype == LYRA_VALUE_I32) {         \
            result =                                                      \
                constants[insn->left_var]                                 \
                    .data.i32 C_OP constants[insn->right_operand.var]     \
                    .data.i32;                                            \
            has_result = 1;                                               \
        } else if (ltype == LYRA_VALUE_F64 && rtype == LYRA_VALUE_F64) {  \
            result =                                                      \
                constants[insn->left_var]                                 \
                    .data.f64 C_OP constants[insn->right_operand.var]     \
                    .data.f64;                                            \
            has_result = 1;                                               \
        } else if (ltype == LYRA_VALUE_I32 && rtype == LYRA_VALUE_F64) {  \
            result = ((double)constants[insn->left_var].data.i32)         \
                         C_OP constants[insn->right_operand.var]          \
                             .data.f64;                                   \
            has_result = 1;                                               \
        } else if (ltype == LYRA_VALUE_F64 && rtype == LYRA_VALUE_I32) {  \
            result = constants[insn->left_var].data.f64 C_OP(             \
                (double)constants[insn->right_operand.var].data.i32);     \
            has_result = 1;                                               \
        }                                                                 \
        if (has_result) {                                                 \
            constants[insn->dest_var] = (struct lyra_value){              \
                .data.i32 = result,                                       \
                .type = LYRA_VALUE_BOOL,                                  \
            };                                                            \
            shared->variable_types[insn->dest_var] = LYRA_VALUE_BOOL;     \
            insn->type = LYRA_OP_MOV_BOOL;                                \
            insn->left_var = 0;                                           \
            insn->right_operand = LYRA_INSN_I32(result);                  \
        }                                                                 \
        generate_const_insn(insn, constants[insn->dest_var], block,       \
                            shared, ctx);                                 \
        continue;                                                         \
    }
        case LYRA_OP_EQ_VAR:
        case LYRA_OP_EQ_PRIM:
        case LYRA_OP_EQ_NUM:
            COMP_OP(LYRA_OP_EQ, ==)
        case LYRA_OP_NEQ_VAR:
        case LYRA_OP_NEQ_PRIM:
        case LYRA_OP_NEQ_NUM:
            COMP_OP(LYRA_OP_NEQ, !=)
        case LYRA_OP_LT_VAR:
        case LYRA_OP_LT_PRIM:
        case LYRA_OP_LT_NUM:
            COMP_OP(LYRA_OP_LT, <)
        case LYRA_OP_GT_VAR:
        case LYRA_OP_GT_PRIM:
        case LYRA_OP_GT_NUM:
            COMP_OP(LYRA_OP_GT, >)
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_LEQ_PRIM:
        case LYRA_OP_LEQ_NUM:
            COMP_OP(LYRA_OP_LEQ, <=)
        case LYRA_OP_GEQ_VAR:
        case LYRA_OP_GEQ_PRIM:
        case LYRA_OP_GEQ_NUM:
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
