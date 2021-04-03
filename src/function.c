#include "function.h"

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