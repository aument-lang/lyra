#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bc_data/types.txt"

union lyra_insn_operand {
    int32_t i32;
    double f64;
    size_t var;
};

#define LYRA_INSN_I32(X) ((union lyra_insn_operand){.i32 = (X)})
#define LYRA_INSN_F64(X) ((union lyra_insn_operand){.f64 = (X)})
#define LYRA_INSN_REG(X) ((union lyra_insn_operand){.var = (X)})

struct lyra_insn {
    struct lyra_insn *prev;
    struct lyra_insn *next;
    size_t dest_var;
    size_t left_var;
    union lyra_insn_operand right_operand;
    enum lyra_insn_type type;
};

struct lyra_insn *lyra_insn_new(enum lyra_insn_type type, size_t left_var,
                                union lyra_insn_operand right_operand,
                                size_t dest_var);

static inline struct lyra_insn *
lyra_insn_imm(enum lyra_insn_type type,
              union lyra_insn_operand right_operand, size_t dest_var) {
    return lyra_insn_new(type, 0, right_operand, dest_var);
}

struct lyra_comp;
void lyra_insn_comp(struct lyra_insn *insn, struct lyra_comp *c);

void lyra_insn_print(struct lyra_insn *insn);
