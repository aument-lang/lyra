// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once

#include "insn.h"
#include <stdint.h>

enum lyra_value_type {
    LYRA_VALUE_UNTYPED = 0,
    LYRA_VALUE_ANY,
    LYRA_VALUE_I32,
    LYRA_VALUE_F64,
    LYRA_VALUE_BOOL,
    LYRA_VALUE_NUM,
    LYRA_VALUE_STR,
};

static inline int lyra_value_type_is_primitive(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_I32:
    case LYRA_VALUE_F64:
    case LYRA_VALUE_BOOL:
        return 1;
    default:
        return 0;
    }
}

static inline int
lyra_value_type_is_primitive_num(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_I32:
    case LYRA_VALUE_F64:
        return 1;
    default:
        return 0;
    }
}

static inline int lyra_value_type_is_numeric(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_I32:
    case LYRA_VALUE_F64:
    case LYRA_VALUE_NUM:
        return 1;
    default:
        return 0;
    }
}

static inline const char *lyra_value_type_c(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_ANY:
        return "au_value_t";
    case LYRA_VALUE_I32:
    case LYRA_VALUE_BOOL:
        return "int32_t";
    case LYRA_VALUE_F64:
        return "double";
    case LYRA_VALUE_NUM:
        return "au_num_t";
    case LYRA_VALUE_STR:
        return "au_string_ptr";
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
    case LYRA_VALUE_NUM:
        return "au_num_to_value";
    case LYRA_VALUE_STR:
        return "au_value_string";
    }
    return 0;
}

static inline enum lyra_insn_type
lyra_value_type_to_any_op(enum lyra_value_type type) {
    switch (type) {
    case LYRA_VALUE_UNTYPED: {
        return LYRA_OP_ENSURE_VALUE_UNTYPED;
    }
    case LYRA_VALUE_ANY: {
        return LYRA_OP_MOV_VAR;
    }
    case LYRA_VALUE_BOOL: {
        return LYRA_OP_ENSURE_VALUE_BOOL;
    }
    case LYRA_VALUE_I32: {
        return LYRA_OP_ENSURE_VALUE_I32;
    }
    case LYRA_VALUE_F64: {
        return LYRA_OP_ENSURE_VALUE_F64;
    }
    case LYRA_VALUE_NUM: {
        return LYRA_OP_ENSURE_VALUE_NUM;
    }
    case LYRA_VALUE_STR: {
        return LYRA_OP_ENSURE_VALUE_STR;
    }
    }
    abort();
}

struct lyra_string;
struct lyra_value {
    union {
        int32_t i32;
        double f64;
        struct lyra_string *str;
    } data;
    enum lyra_value_type type;
};