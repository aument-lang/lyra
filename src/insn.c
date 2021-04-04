#include "insn.h"
#include <stdlib.h>

#include "bc_data/codegen.txt"

struct lyra_insn *lyra_insn_new(enum lyra_insn_type type, size_t left_var,
                                union lyra_insn_operand right_operand,
                                size_t dest_var) {
    struct lyra_insn *insn = malloc(sizeof(struct lyra_insn));
    insn->prev = 0;
    insn->next = 0;
    insn->left_var = left_var;
    insn->right_operand = right_operand;
    insn->dest_var = dest_var;
    insn->type = type;
    return insn;
}

void lyra_insn_print(struct lyra_insn *insn) {
    printf("insn %p [prev: %p] [next: %p]\n", insn, insn->prev,
           insn->next);
    printf("  type: %d\n", insn->type);
    if (lyra_insn_type_has_left_var(insn->type))
        printf("  left: 0x%" PRIxMAX "\n", insn->left_var);
    printf("  right: 0x%" PRIxMAX "\n", insn->right_operand.var);
    printf("  dest: 0x%" PRIxMAX "\n", insn->dest_var);
}
