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
#include "value.h"

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

static enum lyra_value_type unify_types(enum lyra_value_type dest,
                                        enum lyra_value_type src) {
    if (dest == LYRA_VALUE_UNTYPED)
        return src;
    if (dest == src)
        return dest;
    if (lyra_value_type_is_numeric(dest) &&
        lyra_value_type_is_numeric(src))
        return LYRA_VALUE_NUM;
    return LYRA_VALUE_ANY;
}

#define SET_TYPE(VAR, TYPE)                                               \
    do {                                                                  \
        const size_t _var = (VAR);                                        \
        shared->variable_types[_var] =                                    \
            unify_types(shared->variable_types[_var], TYPE);              \
    } while (0)

static size_t generate_cast_to_any(struct lyra_insn *insn, size_t var,
                                   struct lyra_block *block,
                                   struct lyra_function_shared *shared,
                                   struct lyra_ctx *ctx) {
    if (shared->variable_types[var] == LYRA_VALUE_ANY)
        return var;
    enum lyra_insn_type cast_op =
        lyra_value_type_to_any_op(shared->variable_types[var]);
    const size_t new_var =
        lyra_function_shared_add_variable(shared, LYRA_VALUE_ANY, ctx);

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

/// Infers types based on how values are used inside a block. This pass
/// provides type information for block-local variables; type information
/// of shared variables are provided by full type inference.
///
/// Upon encountering a shared variable, it will be casted into a more
/// specific type (ie. number or f64). These cast operations will be used
/// as constraints for type unification by the full type inference pass.
int lyra_pass_partial_type_inference(struct lyra_block *block,
                                     struct lyra_function_shared *shared,
                                     LYRA_UNUSED struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (lyra_insn_type_has_dest(insn->type)) {
            if (lyra_function_shared_is_var_multiple_use(shared,
                                                         insn->dest_var))
                continue;
        }
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
        // Operations that only return string
        case LYRA_OP_MOV_STR: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_STR);
            break;
        }
        // mov var to var operation
        case LYRA_OP_MOV_VAR: {
            SET_TYPE(insn->dest_var,
                     shared->variable_types[insn->left_var]);
            break;
        }
        // Generic binary operations into specialized ops
#define BIN_OP(BASE_OP)                                                   \
    {                                                                     \
        enum lyra_value_type ltype =                                      \
            shared->variable_types[insn->left_var];                       \
        enum lyra_value_type rtype =                                      \
            shared->variable_types[insn->right_operand.var];              \
        if (IS_I32(ltype) && IS_I32(rtype)) {                             \
            SET_TYPE(insn->dest_var, LYRA_VALUE_I32);                     \
            insn->type = BASE_OP##_PRIM;                                  \
        } else if ((IS_I32(ltype) && IS_F64(rtype)) ||                    \
                   (IS_F64(ltype) && IS_I32(rtype))) {                    \
            SET_TYPE(insn->dest_var, LYRA_VALUE_F64);                     \
            insn->type = BASE_OP##_PRIM;                                  \
            insn->right_operand.var = generate_cast(                      \
                insn, insn->right_operand.var, LYRA_VALUE_F64,            \
                LYRA_OP_ENSURE_F64_PRIM, block, shared, ctx);             \
        } else if (IS_F64(ltype) && IS_F64(rtype)) {                      \
            SET_TYPE(insn->dest_var, LYRA_VALUE_F64);                     \
            insn->type = BASE_OP##_PRIM;                                  \
        } else if (lyra_value_type_is_primitive_num(ltype)) {             \
            SET_TYPE(insn->dest_var, LYRA_VALUE_NUM);                     \
            insn->type = SELECT_NUM_BIN_OP(ltype, BASE_OP);               \
            size_t left = insn->left_var;                                 \
            insn->left_var = generate_cast(                               \
                insn, insn->right_operand.var, LYRA_VALUE_NUM,            \
                LYRA_OP_ENSURE_NUM_UNTYPED, block, shared, ctx);          \
            insn->right_operand.var = left;                               \
        } else if (lyra_value_type_is_primitive_num(rtype)) {             \
            SET_TYPE(insn->dest_var, LYRA_VALUE_NUM);                     \
            insn->type = SELECT_NUM_BIN_OP(rtype, BASE_OP);               \
            insn->left_var = generate_cast(                               \
                insn, insn->left_var, LYRA_VALUE_NUM,                     \
                LYRA_OP_ENSURE_NUM_UNTYPED, block, shared, ctx);          \
        } else {                                                          \
            shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;      \
        }                                                                 \
        break;                                                            \
    }
        case LYRA_OP_ADD_VAR:
        case LYRA_OP_ADD_PRIM:
        case LYRA_OP_ADD_NUM_I32:
        case LYRA_OP_ADD_NUM_F64:
        case LYRA_OP_ADD_STR: {
            {
                enum lyra_value_type ltype =
                    shared->variable_types[insn->left_var];
                enum lyra_value_type rtype =
                    shared->variable_types[insn->right_operand.var];
                if (ltype == LYRA_VALUE_STR && rtype == LYRA_VALUE_STR) {
                    shared->variable_types[insn->dest_var] =
                        LYRA_VALUE_STR;
                    insn->type = LYRA_OP_ADD_STR;
                    break;
                } else if (ltype == LYRA_VALUE_STR) {
                    shared->variable_types[insn->dest_var] =
                        LYRA_VALUE_STR;
                    insn->type = LYRA_OP_ADD_STR;
                    insn->left_var = generate_cast(
                        insn, insn->left_var, LYRA_VALUE_STR,
                        LYRA_OP_ENSURE_VALUE_STR, block, shared, ctx);
                    break;
                } else if (rtype == LYRA_VALUE_STR) {
                    shared->variable_types[insn->dest_var] =
                        LYRA_VALUE_STR;
                    insn->type = LYRA_OP_ADD_STR;
                    insn->right_operand.var = generate_cast(
                        insn, insn->right_operand.var, LYRA_VALUE_STR,
                        LYRA_OP_ENSURE_VALUE_STR, block, shared, ctx);
                    break;
                }
            }
            BIN_OP(LYRA_OP_ADD)
        }
        case LYRA_OP_SUB_VAR:
        case LYRA_OP_SUB_PRIM:
        case LYRA_OP_SUB_NUM_I32:
        case LYRA_OP_SUB_NUM_F64:
            BIN_OP(LYRA_OP_SUB)
        case LYRA_OP_MUL_VAR:
        case LYRA_OP_MUL_PRIM:
        case LYRA_OP_MUL_NUM_I32:
        case LYRA_OP_MUL_NUM_F64:
            BIN_OP(LYRA_OP_MUL)
#undef BIN_OP
        // Comparison operations
        case LYRA_OP_EQ_VAR:
        case LYRA_OP_NEQ_VAR:
        case LYRA_OP_LT_VAR:
        case LYRA_OP_GT_VAR:
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_GEQ_VAR:
        case LYRA_OP_EQ_NUM:
        case LYRA_OP_NEQ_NUM:
        case LYRA_OP_LT_NUM:
        case LYRA_OP_GT_NUM:
        case LYRA_OP_LEQ_NUM:
        case LYRA_OP_GEQ_NUM: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_BOOL);
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
                case LYRA_OP_EQ_NUM:
                    insn->type = LYRA_OP_EQ_NUM;
                    break;
                case LYRA_OP_NEQ_VAR:
                case LYRA_OP_NEQ_NUM:
                    insn->type = LYRA_OP_NEQ_NUM;
                    break;
                case LYRA_OP_LT_VAR:
                case LYRA_OP_LT_NUM:
                    insn->type = LYRA_OP_LT_NUM;
                    break;
                case LYRA_OP_GT_VAR:
                case LYRA_OP_GT_NUM:
                    insn->type = LYRA_OP_GT_NUM;
                    break;
                case LYRA_OP_LEQ_VAR:
                case LYRA_OP_LEQ_NUM:
                    insn->type = LYRA_OP_LEQ_NUM;
                    break;
                case LYRA_OP_GEQ_VAR:
                case LYRA_OP_GEQ_NUM:
                    insn->type = LYRA_OP_GEQ_NUM;
                    break;
                default:
                    abort();
                }
            } else if (lyra_value_type_is_primitive(ltype) &&
                       lyra_value_type_is_primitive(rtype)) {
                switch (insn->type) {
                case LYRA_OP_EQ_VAR:
                case LYRA_OP_EQ_NUM:
                    insn->type = LYRA_OP_EQ_PRIM;
                    break;
                case LYRA_OP_NEQ_VAR:
                case LYRA_OP_NEQ_NUM:
                    insn->type = LYRA_OP_NEQ_PRIM;
                    break;
                case LYRA_OP_LT_VAR:
                case LYRA_OP_LT_NUM:
                    insn->type = LYRA_OP_LT_PRIM;
                    break;
                case LYRA_OP_GT_VAR:
                case LYRA_OP_GT_NUM:
                    insn->type = LYRA_OP_GT_PRIM;
                    break;
                case LYRA_OP_LEQ_VAR:
                case LYRA_OP_LEQ_NUM:
                    insn->type = LYRA_OP_LEQ_PRIM;
                    break;
                case LYRA_OP_GEQ_VAR:
                case LYRA_OP_GEQ_NUM:
                    insn->type = LYRA_OP_GEQ_PRIM;
                    break;
                default:
                    abort();
                }
            }
            break;
        }
        // Integer-only binary operations
        case LYRA_OP_BOR_I32:
        case LYRA_OP_BXOR_I32:
        case LYRA_OP_BAND_I32:
        case LYRA_OP_BSHL_I32:
        case LYRA_OP_BSHR_I32:
        case LYRA_OP_MOD_I32: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_I32);

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
        // Division
        case LYRA_OP_DIV_VAR: {
            SET_TYPE(insn->dest_var, LYRA_VALUE_F64);

            enum lyra_value_type ltype =
                shared->variable_types[insn->left_var];
            if (ltype == LYRA_VALUE_ANY) {
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_F64,
                                  LYRA_OP_ENSURE_F64, block, shared, ctx);
                ltype = LYRA_VALUE_F64;
            } else if (ltype == LYRA_VALUE_NUM) {
                insn->left_var = generate_cast(
                    insn, insn->left_var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64_NUM, block, shared, ctx);
                ltype = LYRA_VALUE_F64;
            } else if (ltype == LYRA_VALUE_I32) {
                insn->left_var = generate_cast(
                    insn, insn->left_var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64_PRIM, block, shared, ctx);
                ltype = LYRA_VALUE_F64;
            }

            enum lyra_value_type rtype =
                shared->variable_types[insn->right_operand.var];
            if (rtype == LYRA_VALUE_ANY) {
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64, block, shared, ctx);
                rtype = LYRA_VALUE_F64;
            } else if (rtype == LYRA_VALUE_NUM) {
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64_NUM, block, shared, ctx);
                rtype = LYRA_VALUE_F64;
            } else if (rtype == LYRA_VALUE_I32) {
                insn->right_operand.var = generate_cast(
                    insn, insn->right_operand.var, LYRA_VALUE_F64,
                    LYRA_OP_ENSURE_F64_PRIM, block, shared, ctx);
                rtype = LYRA_VALUE_F64;
            }

            if (!IS_F64(ltype) || !IS_F64(rtype)) {
                abort(); // TODO
            }

            insn->type = LYRA_OP_DIV_PRIM;
            break;
        }
        // Call instructions
        case LYRA_OP_CALL: {
            struct lyra_insn_call_args *args =
                insn->right_operand.call_args;
            for (size_t i = 0; i < args->length; i++) {
                if (shared->variable_types[args->data[i]] !=
                    LYRA_VALUE_ANY) {
                    args->data[i] = generate_cast_to_any(
                        insn, args->data[i], block, shared, ctx);
                }
            }
            if (lyra_insn_call_args_has_return(args))
                shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;
            break;
        }
        // Other
        default: {
            if (lyra_insn_type_has_dest(insn->type)) {
                shared->variable_types[insn->dest_var] = LYRA_VALUE_ANY;
            }
        }
        }
    }
    return 1;
}

/// Infers types based on type cast operations generated by the partial
/// type inference pass
static int
lyra_pass_full_type_inference(struct lyra_block *block,
                              struct lyra_function_shared *shared,
                              LYRA_UNUSED struct lyra_ctx *ctx) {
    int changed = 0;
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (!lyra_insn_type_has_dest(insn->type))
            continue;
        switch (insn->type) {
        case LYRA_OP_MOV_VAR: {
            enum lyra_value_type old_type =
                shared->variable_types[insn->dest_var];
            shared->variable_types[insn->dest_var] = unify_types(
                old_type, shared->variable_types[insn->left_var]);
            if (shared->variable_types[insn->dest_var] != old_type) {
                changed = 1;
            }
            break;
        }
        default:
            break;
        }
    }
    return changed;
}

int lyra_function_full_type_inference(struct lyra_function *fn) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (size_t i = 0; i < fn->blocks.len; i++)
            if (lyra_pass_full_type_inference(&fn->blocks.data[i],
                                              &fn->shared, fn->ctx))
                changed = 1;
    }
    return 1;
}

/// Corrects any variable assignments and casting operations generated
/// by the partial type inference pass. This requires type information from
/// the full type inference pass
int lyra_pass_correct_var_movs(struct lyra_block *block,
                               struct lyra_function_shared *shared,
                               LYRA_UNUSED struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        case LYRA_OP_MOV_VAR:
        case LYRA_OP_ENSURE_VALUE_NUM:
        case LYRA_OP_ENSURE_VALUE_I32:
        case LYRA_OP_ENSURE_VALUE_F64:
        case LYRA_OP_ENSURE_VALUE_UNTYPED:
        case LYRA_OP_ENSURE_NUM:
        case LYRA_OP_ENSURE_NUM_UNTYPED:
        case LYRA_OP_ENSURE_NUM_I32:
        case LYRA_OP_ENSURE_NUM_F64: {
            switch (shared->variable_types[insn->dest_var]) {
            case LYRA_VALUE_ANY: {
                switch (shared->variable_types[insn->left_var]) {
                case LYRA_VALUE_UNTYPED:
                    break;
                case LYRA_VALUE_ANY: {
                    insn->type = LYRA_OP_MOV_VAR;
                    break;
                }
                case LYRA_VALUE_BOOL: {
                    insn->type = LYRA_OP_ENSURE_VALUE_BOOL;
                    break;
                }
                case LYRA_VALUE_NUM: {
                    insn->type = LYRA_OP_ENSURE_VALUE_NUM;
                    break;
                }
                case LYRA_VALUE_I32: {
                    insn->type = LYRA_OP_ENSURE_VALUE_I32;
                    break;
                }
                case LYRA_VALUE_F64: {
                    insn->type = LYRA_OP_ENSURE_VALUE_F64;
                    break;
                }
                case LYRA_VALUE_STR: {
                    insn->type = LYRA_OP_ENSURE_VALUE_STR;
                    break;
                }
                }
                break;
            }
            case LYRA_VALUE_NUM: {
                switch (shared->variable_types[insn->left_var]) {
                case LYRA_VALUE_UNTYPED:
                    break;
                case LYRA_VALUE_ANY: {
                    insn->type = LYRA_OP_ENSURE_NUM;
                    break;
                }
                case LYRA_VALUE_NUM: {
                    insn->type = LYRA_OP_MOV_VAR;
                    break;
                }
                case LYRA_VALUE_I32: {
                    insn->type = LYRA_OP_ENSURE_NUM_I32;
                    break;
                }
                case LYRA_VALUE_F64: {
                    insn->type = LYRA_OP_ENSURE_NUM_F64;
                    break;
                }
                default:
                    abort();
                }
                break;
            }
            default:
                break;
            }
        }
        default:
            break;
        }
    }
    return 1;
}

int lyra_pass_remove_indirection(struct lyra_block *block,
                                 struct lyra_function_shared *shared,
                                 LYRA_UNUSED struct lyra_ctx *ctx) {
    size_t *direct_registers =
        malloc(sizeof(size_t) * shared->variables_len);
    for (size_t i = 0; i < shared->variables_len; i++)
        direct_registers[i] = i;

    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        if (lyra_insn_type_has_dest(insn->type) &&
            lyra_function_shared_is_var_multiple_use(shared,
                                                     insn->dest_var))
            break;
        switch (insn->type) {
        case LYRA_OP_MOV_VAR:
        case LYRA_OP_ENSURE_VALUE_NUM:
        case LYRA_OP_ENSURE_VALUE_I32:
        case LYRA_OP_ENSURE_VALUE_F64:
        case LYRA_OP_ENSURE_VALUE_UNTYPED:
        case LYRA_OP_ENSURE_NUM_UNTYPED:
        case LYRA_OP_ENSURE_NUM_I32:
        case LYRA_OP_ENSURE_NUM_F64: {
            direct_registers[insn->dest_var] =
                direct_registers[insn->left_var];
            insn->left_var = direct_registers[insn->left_var];
            break;
        }
        default: {
            int changed = 0;

            if (lyra_insn_type_has_left_var(insn->type)) {
                insn->left_var = direct_registers[insn->left_var];
                changed = 1;
            }

            if (lyra_insn_type_has_right_var(insn->type)) {
                insn->right_operand.var =
                    direct_registers[insn->right_operand.var];
                changed = 1;
            }

            if (changed) {
                shared->variable_types[insn->dest_var] =
                    LYRA_VALUE_UNTYPED;
            }
            break;
        }
        }
    }
    free(direct_registers);
    return 1;
}