#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "bit_array.h"
#include "insn.h"

#include "bc_data/types.txt"

LYRA_ARRAY_COPY(size_t, lyra_vars_array, 1)

struct lyra_block {
    struct lyra_insn *insn_first;
    struct lyra_insn *insn_last;
};

LYRA_ARRAY_STRUCT(struct lyra_block, lyra_block_array, 1)

void lyra_block_init(struct lyra_block *block) {
    *block = (struct lyra_block){0};
}

void lyra_block_print(struct lyra_block *block) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next)
        lyra_insn_print(insn);
}

void lyra_block_add_insn(struct lyra_block *block,
                         struct lyra_insn *insn) {
    if (block->insn_first == 0) {
        block->insn_first = insn;
        block->insn_last = insn;
        insn->prev = 0;
        insn->next = 0;
    } else {
        insn->prev = block->insn_last;
        insn->next = 0;
        block->insn_last->next = insn;
        block->insn_last = insn;
    }
}

void lyra_block_remove_insn(struct lyra_block *block,
                            struct lyra_insn *insn) {
    if (block->insn_first == insn) {
        block->insn_first = block->insn_first->next;
        if (block->insn_first != 0)
            block->insn_first->prev = 0;
    }
    if (block->insn_last == insn) {
        block->insn_last = block->insn_last->prev;
        if (block->insn_last != 0)
            block->insn_last->next = 0;
    }
}

enum lyra_value_type {
    LYRA_VALUE_ANY = 0,
    LYRA_VALUE_I32,
    LYRA_VALUE_F64,
};

struct lyra_value {
    union {
        int32_t i32;
        double f64;
    } data;
    enum lyra_value_type type;
};

struct lyra_function_shared {
    enum lyra_value_type *variable_types;
    size_t variables_len;
    /// Bit array of already set variables
    lyra_bit_array managed_vars_set;
    /// Bit array of variables that are used in multiple blocks
    lyra_bit_array managed_vars_multiple_use;
    size_t managed_vars_len;
};

size_t
lyra_function_shared_add_variable(struct lyra_function_shared *shared,
                                  enum lyra_value_type type) {
    size_t idx = shared->variables_len;
    shared->variable_types =
        realloc(shared->variable_types, sizeof(enum lyra_value_type) *
                                            (shared->variables_len + 1));
    shared->variable_types[idx] = type;
    shared->variables_len++;
    return idx;
}

struct lyra_function {
    struct lyra_block_array blocks;
    struct lyra_function_shared shared;
};

struct lyra_function *lyra_function_new() {
    struct lyra_function *fn = malloc(sizeof(struct lyra_function));
    fn->blocks = (struct lyra_block_array){0};
    fn->shared = (struct lyra_function_shared){0};
    return fn;
}

size_t lyra_function_add_block(struct lyra_function *fn,
                               struct lyra_block block) {
    size_t idx = fn->blocks.len;
    lyra_block_array_add(&fn->blocks, block);
    return idx;
}

size_t lyra_function_add_variable(struct lyra_function *fn,
                                  enum lyra_value_type type) {
    return lyra_function_shared_add_variable(&fn->shared, type);
}

void lyra_function_finalize(struct lyra_function *fn) {
    fn->shared.managed_vars_set =
        calloc(LYRA_BA_LEN(fn->shared.variables_len), 1);
    fn->shared.managed_vars_multiple_use =
        calloc(LYRA_BA_LEN(fn->shared.variables_len), 1);
    fn->shared.managed_vars_len = fn->shared.variables_len;
}

typedef int (*lyra_block_mutator_fn_t)(
    struct lyra_block *, struct lyra_function_shared *shared);

int lyra_function_all_blocks(struct lyra_function *fn,
                             lyra_block_mutator_fn_t mutator) {
    for (size_t i = 0; i < fn->blocks.len; i++) {
        if (!mutator(&fn->blocks.data[i], &fn->shared))
            return 0;
    }
    return 1;
}

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
            insn->left_var = variable_mapping[insn->left_var];
        if (lyra_insn_type_has_right_var(insn->type))
            insn->right_operand.var =
                variable_mapping[insn->right_operand.var];
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
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next) {
        int has_dest_reg = lyra_insn_type_has_dest(insn->type);
        if (has_dest_reg && !LYRA_BA_GET_BIT(used_vars, insn->dest_var)) {
            printf("remove %p\n", insn);
            lyra_block_remove_insn(block, insn);
        }
    }
    free(used_vars);
    return 1;
}

int main() {
    struct lyra_function *fn = lyra_function_new();
    lyra_function_add_variable(fn, LYRA_VALUE_ANY);

    struct lyra_block block;
    lyra_block_init(&block);
    {
        struct lyra_insn *insn = lyra_insn_new1(LYRA_OP_MOV_I32, 1, 0);
        lyra_block_add_insn(&block, insn);
    }
    {
        struct lyra_insn *insn =
            lyra_insn_new(LYRA_OP_ADD_I32, 0, LYRA_INSN_I32(2), 0);
        lyra_block_add_insn(&block, insn);
    }
    {
        struct lyra_insn *insn =
            lyra_insn_new(LYRA_OP_ADD_I32, 0, LYRA_INSN_I32(3), 0);
        lyra_block_add_insn(&block, insn);
    }
    lyra_block_print(&block);
    lyra_function_add_block(fn, block);
    lyra_function_finalize(fn);

    lyra_function_all_blocks(fn, lyra_pass_fill_inputs);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_into_semi_ssa);
    lyra_block_print(&fn->blocks.data[0]);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_const_prop);
    lyra_block_print(&fn->blocks.data[0]);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_purge_dead_code);
    lyra_block_print(&fn->blocks.data[0]);
}