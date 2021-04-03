#include <stdlib.h>
#include "block.h"
#include "bit_array.h"
#include "function.h"
#include "block.h"

int lyra_pass_fill_inputs(struct lyra_block *block,
                          struct lyra_function_shared *shared);

int lyra_pass_into_semi_ssa(struct lyra_block *block,
                            struct lyra_function_shared *shared);

int lyra_pass_const_prop(struct lyra_block *block,
                         struct lyra_function_shared *shared);

int lyra_pass_purge_dead_code(struct lyra_block *block,
                              struct lyra_function_shared *shared);