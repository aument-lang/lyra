#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "comp.h"
#include "bit_array.h"
#include "block.h"
#include "insn.h"
#include "passes.h"
#include "value.h"

int main() {
    struct lyra_function *fn = lyra_function_new("main");
    lyra_function_add_variable(fn, LYRA_VALUE_ANY);

    struct lyra_block block;
    lyra_block_init(&block);
    {
        struct lyra_insn *insn =
            lyra_insn_imm(LYRA_OP_MOV_I32, LYRA_INSN_I32(0x66), 0);
        lyra_block_add_insn(&block, insn);
    }
    {
        struct lyra_insn *insn =
            lyra_insn_new(LYRA_OP_ADD_I32_IMM, 0, LYRA_INSN_I32(0x77), 0);
        lyra_block_add_insn(&block, insn);
    }
    {
        struct lyra_insn *insn =
            lyra_insn_new(LYRA_OP_ADD_I32_IMM, 0, LYRA_INSN_I32(0x88), 0);
        lyra_block_add_insn(&block, insn);
    }
    lyra_block_print(&block);
    lyra_function_add_block(fn, block);
    lyra_function_finalize(fn);

    /*
    lyra_function_all_blocks(fn, lyra_pass_fill_inputs);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_into_semi_ssa);
    lyra_block_print(&fn->blocks.data[0]);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_const_prop);
    lyra_block_print(&fn->blocks.data[0]);

    printf("---\n");

    lyra_function_all_blocks(fn, lyra_pass_purge_dead_code);
    lyra_block_print(&fn->blocks.data[0]);

    printf("---\n"); */

    struct lyra_comp c = {0};
    lyra_comp_init(&c);
    lyra_function_comp(fn, &c);
    fwrite(c.source.data, 1, c.source.len, stdout);
}
