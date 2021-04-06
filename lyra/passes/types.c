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

static size_t generate_cast(struct lyra_insn *insn, size_t var,
                            enum lyra_value_type into_type,
                            enum lyra_insn_type cast_op,
                            struct lyra_block *block,
                            struct lyra_function_shared *shared,
                            struct lyra_ctx *ctx) {
    const size_t new_var =
        lyra_function_shared_add_variable(shared, into_type, ctx);
    struct lyra_insn *new_insn =
        lyra_insn_new(cast_op, var, LYRA_INSN_REG(0), new_var, ctx);
    lyra_block_insert_insn(block, insn->prev, new_insn);
    return new_var;
}

#define IS_I32(TYPE) (TYPE == LYRA_VALUE_I32)
#define IS_F64(TYPE) (TYPE == LYRA_VALUE_F64)
#define IS_NUM(TYPE) (TYPE == LYRA_VALUE_NUM)

#define SELECT_NUM_BIN_OP(TYPE, OP)                                       \
    (TYPE == LYRA_VALUE_I32 ? OP##_NUM_I32 : OP##_NUM_F64)

#define SET_TYPE(VAR, TYPE)                                               \
    do {                                                                  \
        if (shared->variable_types[VAR] == LYRA_VALUE_UNTYPED)            \
            shared->variable_types[VAR] = TYPE;                           \
        else if (shared->variable_types[VAR] != TYPE)                     \
            shared->variable_types[VAR] = LYRA_VALUE_ANY;                 \
    } while (0)

int lyra_pass_type_inference(struct lyra_block *block,
                             struct lyra_function_shared *shared,
                             LYRA_UNUSED struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        // Operations that only return an int
        case LYRA_OP_MOV_I32:
        case LYRA_OP_ENSURE_I32: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_I32);
            break;
        }
        // Operations that only return float
        case LYRA_OP_MOV_F64:
        case LYRA_OP_ENSURE_F64: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_F64);
            break;
        }
        // Generic binary operations into specialized ops
        case LYRA_OP_ADD_VAR: {
            enum lyra_value_type ltype =
                shared->variable_types[insn->left_var];
            enum lyra_value_type rtype =
                shared->variable_types[insn->right_operand.var];
            if (IS_I32(ltype) && IS_I32(rtype)) {
                SET_TYPE(insn->dest_var, LYRA_VALUE_I32);
                insn->type = LYRA_OP_ADD_PRIM;
            } else if ((IS_I32(ltype) && IS_F64(rtype)) ||
                       (IS_F64(ltype) && IS_I32(rtype))) {
                SET_TYPE(insn->dest_var, LYRA_VALUE_F64);
                insn->type = LYRA_OP_ADD_PRIM;
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64_PRIM, block, shared, ctx);
            } else if (IS_F64(ltype) && IS_F64(rtype)) {
                SET_TYPE(insn->dest_var, LYRA_VALUE_F64);
                insn->type = LYRA_OP_ADD_PRIM;
            } else if (lyra_value_type_is_primitive_num(ltype)) {
                abort(); // TODO
            } else if (lyra_value_type_is_primitive_num(rtype)) {
                SET_TYPE(insn->dest_var, LYRA_VALUE_NUM);
                insn->type = SELECT_NUM_BIN_OP(rtype, LYRA_OP_ADD);
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_NUM,
                                  LYRA_OP_ENSURE_NUM, block, shared, ctx);
            } else {
                abort(); // TODO
            }
            break;
        }
        case LYRA_OP_EQ_VAR:
        case LYRA_OP_NEQ_VAR:
        case LYRA_OP_LT_VAR:
        case LYRA_OP_GT_VAR:
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_GEQ_VAR: {
            enum lyra_value_type ltype =
                shared->variable_types[insn->left_var];
            enum lyra_value_type rtype =
                shared->variable_types[insn->right_operand.var];
            if (IS_NUM(ltype) || IS_NUM(rtype)) {
                if (!IS_NUM(ltype)) {
                    insn->left_var = generate_cast(
                        insn, insn->left_var, LYRA_VALUE_NUM,
                        LYRA_OP_ENSURE_NUM, block, shared, ctx);
                }
                if (!IS_NUM(rtype)) {
                    insn->right_operand.var = generate_cast(
                        insn, insn->right_operand.var, LYRA_VALUE_NUM,
                        LYRA_OP_ENSURE_NUM, block, shared, ctx);
                }
                switch (insn->type) {
                case LYRA_OP_EQ_VAR:
                    insn->type = LYRA_OP_EQ_NUM;
                    break;
                case LYRA_OP_NEQ_VAR:
                    insn->type = LYRA_OP_NEQ_NUM;
                    break;
                case LYRA_OP_LT_VAR:
                    insn->type = LYRA_OP_LT_NUM;
                    break;
                case LYRA_OP_GT_VAR:
                    insn->type = LYRA_OP_GT_NUM;
                    break;
                case LYRA_OP_LEQ_VAR:
                    insn->type = LYRA_OP_LEQ_NUM;
                    break;
                case LYRA_OP_GEQ_VAR:
                    insn->type = LYRA_OP_GEQ_NUM;
                    break;
                default:
                    abort();
                }
            }
            break;
        }
        // Integer-only binary operations
        case LYRA_OP_BOR_I32:
            LYRA_FALLTHROUGH;
        case LYRA_OP_BXOR_I32:
            LYRA_FALLTHROUGH;
        case LYRA_OP_BAND_I32:
            LYRA_FALLTHROUGH;
        case LYRA_OP_BSHL_I32:
            LYRA_FALLTHROUGH;
        case LYRA_OP_BSHR_I32:
            LYRA_FALLTHROUGH;
        case LYRA_OP_MOD_I32: {
            enum lyra_value_type ltype =
                shared->variable_types[insn->left_var];
            if (ltype == LYRA_VALUE_ANY) {
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_I32,
                                  LYRA_OP_ENSURE_I32, block, shared, ctx);
                ltype = LYRA_VALUE_I32;
            } else if (ltype == LYRA_VALUE_NUM) {
                insn->left_var = generate_cast(
                    insn, insn->left_var, LYRA_VALUE_I32,
                    LYRA_OP_ENSURE_I32_NUM, block, shared, ctx);
                ltype = LYRA_VALUE_I32;
            }

            enum lyra_value_type rtype =
                shared->variable_types[insn->right_operand.var];
            if (rtype == LYRA_VALUE_ANY) {
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_I32,
                    LYRA_OP_ENSURE_I32, block, shared, ctx);
                rtype = LYRA_VALUE_I32;
            } else if (rtype == LYRA_VALUE_NUM) {
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_I32,
                    LYRA_OP_ENSURE_I32_NUM, block, shared, ctx);
                rtype = LYRA_VALUE_I32;
            }

            if (!IS_I32(ltype) || !IS_I32(rtype)) {
                abort(); // TODO
            }
            break;
        }
        default: {
            if (lyra_insn_type_has_dest(insn->type)) {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;
            }
        }
        }
    }
    return 1;
}
