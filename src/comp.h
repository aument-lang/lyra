#pragma once
#include <stdint.h>

#include "banned.h"

#include "char_array.h"
#include "context.h"

struct lyra_comp {
    struct lyra_char_array source;
    struct lyra_ctx *ctx;
};

void lyra_comp_init(struct lyra_comp *c, struct lyra_ctx *ctx);
void lyra_comp_print_str(struct lyra_comp *c, const char *str);
void lyra_comp_print_i32(struct lyra_comp *c, int32_t i);
void lyra_comp_print_isize(struct lyra_comp *c, size_t i);
void lyra_comp_print_f64(struct lyra_comp *c, double d);
