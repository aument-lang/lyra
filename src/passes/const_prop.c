#include <stdlib.h>
#include <string.h>

#include "bc_data/types.txt"
#include "bit_array.h"
#include "block.h"
#include "function.h"
#include "platform.h"

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared,
                         LYRA_UNUSED struct lyra_ctx *ctx) {
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
            insn->type = LYRA_OP_BASE##_NUM_I32_IMM;                      \
            insn->right_operand = LYRA_INSN_I32(                          \
                constants[insn->right_operand.var].data.i32);             \
        } else if (right_type == LYRA_VALUE_F64) {                        \
            insn->type = LYRA_OP_BASE##_NUM_F64_IMM;                      \
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
