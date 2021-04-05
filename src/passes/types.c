#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_type_inference(struct lyra_block *block,
                             struct lyra_function_shared *shared,
                             LYRA_UNUSED struct lyra_ctx *ctx) {
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
            LYRA_FALLTHROUGH;
        case LYRA_OP_MUL_I32_IMM:
            LYRA_FALLTHROUGH;
        case LYRA_OP_ADD_I32_IMM:
            LYRA_FALLTHROUGH;
        case LYRA_OP_SUB_I32_IMM: {
            switch (shared->variable_types[insn->dest_var]) {
            case LYRA_VALUE_I32:
                break;
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
            break;
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
        case LYRA_OP_LT_NUM:
        case LYRA_OP_LT_I32_IMM:
        case LYRA_OP_LT_F64_IMM:
        case LYRA_OP_LT_NUM_I32_IMM:
        case LYRA_OP_LT_NUM_F64_IMM:
        case LYRA_OP_GT_VAR:
        case LYRA_OP_GT_NUM:
        case LYRA_OP_GT_I32_IMM:
        case LYRA_OP_GT_F64_IMM:
        case LYRA_OP_GT_NUM_I32_IMM:
        case LYRA_OP_GT_NUM_F64_IMM:
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_LEQ_NUM:
        case LYRA_OP_LEQ_I32_IMM:
        case LYRA_OP_LEQ_F64_IMM:
        case LYRA_OP_LEQ_NUM_I32_IMM:
        case LYRA_OP_LEQ_NUM_F64_IMM:
        case LYRA_OP_GEQ_VAR:
        case LYRA_OP_GEQ_NUM:
        case LYRA_OP_GEQ_I32_IMM:
        case LYRA_OP_GEQ_F64_IMM:
        case LYRA_OP_GEQ_NUM_I32_IMM:
        case LYRA_OP_GEQ_NUM_F64_IMM: {
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

int lyra_pass_cast_to_specific_type(struct lyra_block *block,
                                    struct lyra_function_shared *shared,
                                    struct lyra_ctx *ctx) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        switch (insn->type) {
        // Operations with immediate operand
        case LYRA_OP_ADD_I32_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_I32)
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_I32,
                                  LYRA_OP_ENSURE_I32, block, shared, ctx);
            break;
        }
        case LYRA_OP_ADD_F64_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_F64)
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_F64,
                                  LYRA_OP_ENSURE_F64, block, shared, ctx);
            break;
        }
        case LYRA_OP_ADD_NUM_I32_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_NUM) {
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_NUM,
                                  LYRA_OP_ENSURE_NUM, block, shared, ctx);
            } else if (shared->variable_types[insn->left_var] ==
                           LYRA_VALUE_I32 &&
                       shared->variable_types[insn->dest_var] ==
                           LYRA_VALUE_I32) {
                insn->type = LYRA_OP_ADD_I32_IMM;
            }
            break;
        }
        case LYRA_OP_ADD_NUM_F64_IMM: {
            if (shared->variable_types[insn->left_var] != LYRA_VALUE_NUM) {
                insn->left_var =
                    generate_cast(insn, insn->left_var, LYRA_VALUE_NUM,
                                  LYRA_OP_ENSURE_NUM, block, shared, ctx);
            } else if (shared->variable_types[insn->left_var] ==
                           LYRA_VALUE_F64 &&
                       shared->variable_types[insn->dest_var] ==
                           LYRA_VALUE_F64) {
                insn->type = LYRA_OP_ADD_F64_IMM;
            }
            break;
        }
        // Generic operations into specialized ops
        case LYRA_OP_EQ_VAR:
        case LYRA_OP_NEQ_VAR:
        case LYRA_OP_LT_VAR:
        case LYRA_OP_GT_VAR:
        case LYRA_OP_LEQ_VAR:
        case LYRA_OP_GEQ_VAR: {
            if (shared->variable_types[insn->left_var] == LYRA_VALUE_NUM ||
                shared->variable_types[insn->right_operand.var] ==
                    LYRA_VALUE_NUM) {
                if (shared->variable_types[insn->left_var] !=
                    LYRA_VALUE_NUM) {
                    insn->left_var = generate_cast(
                        insn, insn->left_var, LYRA_VALUE_NUM,
                        LYRA_OP_ENSURE_NUM, block, shared, ctx);
                }
                if (shared->variable_types[insn->right_operand.var] !=
                    LYRA_VALUE_NUM) {
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
        default:
            break;
        }
    }
    return 1;
}
