#pragma once

#include "array.h"
#include "insn.h"

struct lyra_block {
    struct lyra_insn *insn_first;
    struct lyra_insn *insn_last;
};

LYRA_ARRAY_STRUCT(struct lyra_block, lyra_block_array, 1)

void lyra_block_init(struct lyra_block *block);

void lyra_block_print(struct lyra_block *block);

void lyra_block_add_insn(struct lyra_block *block,
                         struct lyra_insn *insn);

void lyra_block_remove_insn(struct lyra_block *block,
                            struct lyra_insn *insn);