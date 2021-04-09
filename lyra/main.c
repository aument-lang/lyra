// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "bit_array.h"
#include "block.h"
#include "comp.h"
#include "context.h"
#include "insn.h"
#include "passes.h"
#include "value.h"

int main() {
    struct lyra_ctx ctx;
    lyra_ctx_init(&ctx);

    struct lyra_function *fn = lyra_function_new(0, 1, &ctx);
    lyra_function_add_variable(fn, LYRA_VALUE_UNTYPED);
    lyra_function_add_variable(fn, LYRA_VALUE_UNTYPED);
    lyra_function_add_variable(fn, LYRA_VALUE_UNTYPED);

    {
        struct lyra_block block;
        lyra_block_init(&block);

        {
            struct lyra_insn *insn =
                lyra_insn_imm(LYRA_OP_MOV_I32, LYRA_INSN_I32(0), 0, &ctx);
            lyra_block_add_insn(&block, insn);
        }
        {
            struct lyra_insn *insn =
                lyra_insn_imm(LYRA_OP_MOV_I32, LYRA_INSN_I32(10), 1, &ctx);
            lyra_block_add_insn(&block, insn);
        }
        {
            struct lyra_insn *insn = lyra_insn_new(
                LYRA_OP_ADD_VAR, 0, LYRA_INSN_REG(1), 1, &ctx);
            lyra_block_add_insn(&block, insn);
        }
        {
            struct lyra_insn *insn = lyra_insn_new(
                LYRA_OP_LT_VAR, 0, LYRA_INSN_REG(1), 2, &ctx);
            lyra_block_add_insn(&block, insn);
        }
        {
            block.connector.type = LYRA_BLOCK_RET;
            block.connector.var = 2;
            block.connector.label = 0;
        }

        lyra_block_print(&block);
        lyra_function_add_block(fn, block);
    }

    lyra_function_reset_managed_vars(fn);

    lyra_function_all_blocks(fn, lyra_pass_check_multiple_use);
    lyra_function_all_blocks(fn, lyra_pass_into_semi_ssa);
    lyra_function_all_blocks(fn, lyra_pass_partial_type_inference);
    // lyra_function_all_blocks(fn, lyra_pass_const_prop);
    // lyra_function_all_blocks(fn, lyra_pass_purge_dead_code);
    lyra_ctx_gc_run(&ctx);

    lyra_block_print(&fn->blocks.data[0]);

    {
        struct lyra_comp c = {0};
        lyra_comp_init(&c, &ctx);
        lyra_function_comp(fn, &c);
        fwrite(c.source.data, 1, c.source.len, stdout);
    }

    lyra_ctx_del(&ctx);
}
