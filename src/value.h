#pragma once

#include <stdint.h>

enum lyra_value_type {
    LYRA_VALUE_UNTYPED = 0,
    LYRA_VALUE_ANY,
    LYRA_VALUE_I32,
    LYRA_VALUE_F64,
    LYRA_VALUE_BOOL,
};

struct lyra_value {
    union {
        int32_t i32;
        double f64;
    } data;
    enum lyra_value_type type;
};

static inline const char *lyra_value_type_c(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_ANY:
        return "au_value_t";
    case LYRA_VALUE_I32:
    case LYRA_VALUE_BOOL:
        return "int32_t";
    case LYRA_VALUE_F64:
        return "double";
    default:
        return 0;
    }
}

static inline const char *
lyra_value_type_to_any_fn(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_UNTYPED:
    case LYRA_VALUE_ANY:
        return 0;
    case LYRA_VALUE_I32:
        return "au_value_int";
    case LYRA_VALUE_BOOL:
        return "au_value_bool";
    case LYRA_VALUE_F64:
        return "au_value_double";
    }
    return 0;
}
