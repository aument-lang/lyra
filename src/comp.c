#include "comp.h"
#include <inttypes.h>

void lyra_comp_init(struct lyra_comp *c) {
    c->source = (struct lyra_char_array){0};
}

void lyra_comp_print_str(struct lyra_comp *c, const char *str) {
    while(*str != 0) {
        lyra_char_array_add(&c->source, *str);
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
