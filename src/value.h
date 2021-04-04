#pragma once

#include <stdint.h>

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

static inline const char *lyra_value_type_c(enum lyra_value_type type) {
    switch(type) {
    case LYRA_VALUE_ANY: return "au_value_t";
    case LYRA_VALUE_I32: return "int32_t";
    case LYRA_VALUE_F64: return "double";
    }
}
