// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <inttypes.h>

#include "comp.h"

void lyra_comp_init(struct lyra_comp *c, struct lyra_ctx *ctx) {
    c->source = (struct lyra_char_array){0};
    c->ctx = ctx;
}

void lyra_comp_print_str(struct lyra_comp *c, const char *str) {
    while (*str != 0) {
        lyra_char_array_add(&c->source, *str, c->ctx);
        str++;
    }
}

void lyra_comp_print_i32(struct lyra_comp *c, int32_t i) {
    char temp[32];
    temp[snprintf(temp, sizeof(temp) - 1, "%" PRId32, i)] = 0;
    lyra_comp_print_str(c, temp);
}

void lyra_comp_print_isize(struct lyra_comp *c, size_t i) {
    char temp[32];
    temp[snprintf(temp, sizeof(temp) - 1, "%" PRIdMAX, i)] = 0;
    lyra_comp_print_str(c, temp);
}

void lyra_comp_print_f64(struct lyra_comp *c, double d) {
    char temp[32];
    temp[snprintf(temp, sizeof(temp) - 1, "%f", d)] = 0;
    lyra_comp_print_str(c, temp);
}
