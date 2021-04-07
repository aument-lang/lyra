// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <string.h>

#include "block.h"
#include "comp.h"
#include "function.h"

void lyra_block_connector_comp(const struct lyra_block_connector *conn,
                               const struct lyra_function_shared *shared,
                               struct lyra_comp *c) {
    switch (conn->type) {
    case LYRA_BLOCK_FALLTHROUGH: {
        break;
    }
    case LYRA_BLOCK_RET: {
        lyra_comp_print_str(c, "return ");
        const char *convert_fn =
            lyra_value_type_to_any_fn(shared->variable_types[conn->var]);
        if (convert_fn != 0) {
            lyra_comp_print_str(c, convert_fn);
            lyra_comp_print_str(c, "(v");
            lyra_comp_print_isize(c, conn->var);
            lyra_comp_print_str(c, ")");
        } else {
            lyra_comp_print_str(c, "v");
            lyra_comp_print_isize(c, conn->var);
        }
        lyra_comp_print_str(c, ";");
        break;
    }
    case LYRA_BLOCK_RET_NULL: {
        lyra_comp_print_str(c, "return au_value_none();");
        break;
    }
    case LYRA_BLOCK_JMP: {
        lyra_comp_print_str(c, "goto L");
        lyra_comp_print_isize(c, conn->label);
        lyra_comp_print_str(c, ";");
        break;
    }
    case LYRA_BLOCK_JIF: {
        assert(shared->variable_types[conn->var] == LYRA_VALUE_BOOL);
        lyra_comp_print_str(c, "if (v");
        lyra_comp_print_isize(c, conn->var);
        lyra_comp_print_str(c, ") goto L");
        lyra_comp_print_isize(c, conn->label);
        lyra_comp_print_str(c, ";");
        break;
    }
    case LYRA_BLOCK_JNIF: {
        assert(shared->variable_types[conn->var] == LYRA_VALUE_BOOL);
        lyra_comp_print_str(c, "if(!v");
        lyra_comp_print_isize(c, conn->var);
        lyra_comp_print_str(c, ") goto L");
        lyra_comp_print_isize(c, conn->label);
        lyra_comp_print_str(c, ";");
        break;
    }
    }
}

void lyra_block_init(struct lyra_block *block) {
    memset(block, 0, sizeof(struct lyra_block));
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

void lyra_block_insert_insn(struct lyra_block *block,
                            struct lyra_insn *after_insn,
                            struct lyra_insn *insn) {
    struct lyra_insn *next_insn = after_insn->next;

    if (after_insn == 0) {
        block->insn_first = insn;
    } else {
        after_insn->next = insn;
    }

    if (next_insn == 0) {
        block->insn_last = insn;
    } else {
        next_insn->prev = insn;
    }

    insn->prev = after_insn;
    insn->next = next_insn;
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
