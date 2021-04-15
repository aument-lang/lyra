// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <stdlib.h>

#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_check_multiple_use(struct lyra_block *block,
                                 struct lyra_function_shared *shared,
                                 struct lyra_ctx *ctx);

int lyra_pass_check_multiple_set(struct lyra_block *block,
                                 struct lyra_function_shared *shared,
                                 struct lyra_ctx *ctx);

int lyra_pass_into_semi_ssa(struct lyra_block *block,
                            struct lyra_function_shared *shared,
                            struct lyra_ctx *ctx);

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared,
                         struct lyra_ctx *ctx);

int lyra_pass_purge_dead_code(struct lyra_block *block,
                              struct lyra_function_shared *shared,
                              struct lyra_ctx *ctx);

int lyra_function_type_inference(struct lyra_function *fn);

int lyra_pass_remove_indirection(struct lyra_block *block,
                               struct lyra_function_shared *shared,
                               struct lyra_ctx *ctx);

int lyra_pass_correct_var_movs(struct lyra_block *block,
                               struct lyra_function_shared *shared,
                               struct lyra_ctx *ctx);

int lyra_pass_add_casts(struct lyra_block *block,
                        struct lyra_function_shared *shared,
                        LYRA_UNUSED struct lyra_ctx *ctx);

void lyra_function_defrag_vars(struct lyra_function *fn);

