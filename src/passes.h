#include <stdlib.h>

#include "bit_array.h"
#include "block.h"
#include "function.h"

int lyra_pass_fill_inputs(struct lyra_block *block,
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

int lyra_pass_type_inference(struct lyra_block *block,
                             struct lyra_function_shared *shared,
                             struct lyra_ctx *ctx);

int lyra_pass_cast_to_specific_type(
    struct lyra_block *block, struct lyra_function_shared *shared,
    struct lyra_ctx *ctx);
