#include "block.h"
#include "comp.h"

void lyra_block_connector_comp(struct lyra_block_connector *conn, struct lyra_comp *c) {
    switch(conn->type) {
    case LYRA_BLOCK_RET: {
        lyra_comp_print_str(c, "return v");
        lyra_comp_print_isize(c, conn->var);
        break;
    }
    case LYRA_BLOCK_JMP: {
        lyra_comp_print_str(c, "goto L");
        lyra_comp_print_isize(c, conn->label);
        break;
    }
    case LYRA_BLOCK_JTRUE: {
        lyra_comp_print_str(c, "if(v");
        lyra_comp_print_isize(c, conn->var);
        lyra_comp_print_str(c, ") goto L");
        lyra_comp_print_isize(c, conn->label);
        break;
    }
    case LYRA_BLOCK_JFALSE: {
        lyra_comp_print_str(c, "if(!v");
        lyra_comp_print_isize(c, conn->var);
        lyra_comp_print_str(c, ") goto L");
        lyra_comp_print_isize(c, conn->label);
        break;
    }
    }
    lyra_comp_print_str(c, ";");
}

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
    }
    if (block->insn_last == insn) {
        block->insn_last = block->insn_last->prev;
    }
    if (insn->prev != 0)
        insn->prev->next = insn->next;
    if (insn->next != 0)
        insn->next->prev = insn->prev;
    insn->prev = 0;
    insn->next = 0;
}
