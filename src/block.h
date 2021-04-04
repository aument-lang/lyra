#pragma once

#include "array.h"
#include "insn.h"

enum lyra_block_connector_type {
    LYRA_BLOCK_FALLTHROUGH = 0,
    LYRA_BLOCK_RET,
    LYRA_BLOCK_JMP,
    // Conditional jumps (with implicit fallthrough)
    LYRA_BLOCK_JIF,
    LYRA_BLOCK_JNIF,
};

static inline int
lyra_block_connector_type_has_var(enum lyra_block_connector_type type) {
    return type == LYRA_BLOCK_RET || type == LYRA_BLOCK_JIF ||
           type == LYRA_BLOCK_JNIF;
}

struct lyra_block_connector {
    enum lyra_block_connector_type type;
    size_t var;
    size_t label;
};

struct lyra_function_shared;
void lyra_block_connector_comp(const struct lyra_block_connector *conn,
                               const struct lyra_function_shared *shared,
                               struct lyra_comp *c);

struct lyra_block {
    struct lyra_insn *insn_first;
    struct lyra_insn *insn_last;
    struct lyra_block_connector connector;
};

LYRA_ARRAY_STRUCT(struct lyra_block, lyra_block_array, 1)

void lyra_block_init(struct lyra_block *block);

void lyra_block_print(struct lyra_block *block);

void lyra_block_add_insn(struct lyra_block *block, struct lyra_insn *insn);

void lyra_block_insert_insn(struct lyra_block *block,
                            struct lyra_insn *after_insn,
                            struct lyra_insn *insn);

void lyra_block_remove_insn(struct lyra_block *block,
                            struct lyra_insn *insn);
