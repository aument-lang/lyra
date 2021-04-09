// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once
#include <stdlib.h>

#include "bit_array.h"
#include "block.h"
#include "value.h"

struct lyra_function_shared {
    enum lyra_value_type *variable_types;
    size_t variables_len;
    lyra_bit_array used_vars;
    /// Bit array of already set variables
    lyra_bit_array managed_vars_set;
    /// Bit array of variables that are used in multiple blocks.
    /// Do NOT access this variable directly
    /// (use lyra_function_shared_is_var_multiple_use)
    lyra_bit_array managed_vars_multiple_use;
    size_t managed_vars_len;
};

size_t
lyra_function_shared_add_variable(struct lyra_function_shared *shared,
                                  enum lyra_value_type type,
                                  struct lyra_ctx *ctx);

static inline int lyra_function_shared_is_var_multiple_use(
    const struct lyra_function_shared *shared, size_t var) {
    return var < shared->managed_vars_len &&
           LYRA_BA_GET_BIT(shared->managed_vars_multiple_use, var);
}

struct lyra_function {
    struct lyra_block_array blocks;
    struct lyra_function_shared shared;
    size_t name;
    size_t num_args;
    struct lyra_ctx *ctx;
};

struct lyra_function *lyra_function_new(size_t name, size_t num_args,
                                        struct lyra_ctx *ctx);

size_t lyra_function_add_block(struct lyra_function *fn,
                               struct lyra_block block);

size_t lyra_function_add_variable(struct lyra_function *fn,
                                  enum lyra_value_type type);

void lyra_function_reset_managed_vars(struct lyra_function *fn);

struct lyra_comp;
void lyra_function_comp(struct lyra_function *fn, struct lyra_comp *c);

typedef int (*lyra_block_mutator_fn_t)(struct lyra_block *,
                                       struct lyra_function_shared *shared,
                                       struct lyra_ctx *);

int lyra_function_all_blocks(struct lyra_function *fn,
                             lyra_block_mutator_fn_t mutator);