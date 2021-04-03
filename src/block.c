#include "block.h"

void lyra_block_init(struct lyra_block *block) {
    *block = (struct lyra_block){0};
}

void lyra_block_print(struct lyra_block *block) {
    for (struct lyra_insn *insn = block->insn_first; insn != 0;
         insn = insn->next)
        lyra_insn_print(insn);
}

void lyra_block_add_insn(struct lyra_block *block,
                         struct lyra_insn *insn) {
    if (block->insn_first == 0) {
        block->insn_first = insn;
        block->insn_last = insn;
        insn->prev = 0;
        insn->next = 0;
    } else {
        insn->prev = block->insn_last;
        insn->next = 0;
        block->insn_last->next = insn;
        block->insn_last = insn;
    }
}

void lyra_block_remove_insn(struct lyra_block *block,
                            struct lyra_insn *insn) {
    if (block->insn_first == insn) {
        block->insn_first = block->insn_first->next;
        if (block->insn_first != 0)
            block->insn_first->prev = 0;
    }
    if (block->insn_last == insn) {
        block->insn_last = block->insn_last->prev;
        if (block->insn_last != 0)
            block->insn_last->next = 0;
    }
}