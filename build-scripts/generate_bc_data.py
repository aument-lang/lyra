ARG_TYPE_NONE = 0
ARG_TYPE_VAR = 1
ARG_TYPE_I32 = 2
ARG_TYPE_F64 = 2

class Compiler:
    
    def __init__(self):
        self.c_source = ""

    def assign_dest_var(self):
        self.c_source += 'lyra_comp_print_str(c,"v");' \
            'lyra_comp_print_isize(c,insn->dest_var);' \
            'lyra_comp_print_str(c,"=");\n'

    def left_var(self):
        self.c_source += 'lyra_comp_print_str(c,"v");' \
            'lyra_comp_print_isize(c,insn->left_var);\n'

    def right_var(self):
        self.c_source += 'lyra_comp_print_str(c,"v");' \
            'lyra_comp_print_isize(c,insn->right_operand.var);\n'

    def right_i32(self):
        self.c_source += 'lyra_comp_print_i32(c,insn->right_operand.i32);\n'

    def right_f64(self):
        self.c_source += 'lyra_comp_print_f64(c,insn->right_operand.f64);\n'

    def raw(self, string):
        self.c_source += f'lyra_comp_print_str(c,"{string}");\n'
    
    def end(self):
        self.raw(";")
        return self.c_source

class Instruction:
    instances = []

    def __init__(self, name, left_type, right_type=ARG_TYPE_NONE, has_dest=True, **options):
        self.name = name
        self.left_type = left_type
        self.right_type = right_type
        self.has_dest = has_dest
        self.options = options
        Instruction.instances.append(self)

    def gen_compiler(self):
        compiler = Compiler()
        if "c_bin_op" in self.options:
            compiler.assign_dest_var()
            compiler.left_var()
            compiler.raw(self.options["c_bin_op"])
            compiler.right_var()
            return compiler.end()
        elif "c_bin_func" in self.options:
            compiler.assign_dest_var()
            compiler.raw(self.options["c_bin_func"] + "(")
            compiler.left_var()
            compiler.raw(",")
            compiler.right_var()
            compiler.raw(")")
            return compiler.end()
        elif "c_codegen" in self.options:
            self.options["c_codegen"](compiler)
            return compiler.end()

def gen_mov_var(compiler):
    compiler.assign_dest_var()
    compiler.left_var()
Instruction("MOV_VAR", ARG_TYPE_VAR, c_codegen=gen_mov_var)

def gen_mov_i32(compiler):
    compiler.assign_dest_var()
    compiler.right_i32()
Instruction("MOV_I32", ARG_TYPE_VAR, ARG_TYPE_I32, c_codegen=gen_mov_i32)

def gen_mov_f64(compiler):
    compiler.assign_dest_var()
    compiler.right_f64()
Instruction("MOV_F64", ARG_TYPE_VAR, ARG_TYPE_F64, c_codegen=gen_mov_f64)

for (op, c_bin_func, c_bin_op) in [
    ("MUL", "au_value_mul", "*"),
    ("DIV", "au_value_div", "/"),
    ("ADD", "au_value_add", "+"),
    ("SUB", "au_value_sub", "-"),
    ("MOD", "au_value_mod", "%"),
    ("EQ" , "au_value_eq", "=="),
    ("NEQ", "au_value_neq", "!="),
    ("LT" , "au_value_lt", "<"),
    ("GT" , "au_value_gt", ">"),
    ("LEQ", "au_value_leq", "<="),
    ("GEQ", "au_value_geq", ">="),
]:
    Instruction(op + "_VAR", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=c_bin_func)
    Instruction(op + "_I32", ARG_TYPE_VAR, ARG_TYPE_I32, c_bin_op=c_bin_op)
    Instruction(op + "_F64", ARG_TYPE_VAR, ARG_TYPE_F64, c_bin_op=c_bin_op)

for (op, c_bin_op) in [
    ("BOR", "|"),
    ("BXOR", "^"),
    ("BAND", "&"),
    ("BSHL", "<<"),
    ("BSHR", ">>"),
]:
    Instruction(op + "_I32", ARG_TYPE_VAR, ARG_TYPE_I32, c_bin_op=c_bin_op)

# Types data

with open("./src/bc_data/types.txt", "w") as insn_type_f:

    insn_type_defs = "#pragma once\nenum lyra_insn_type {\n"
    for insn in Instruction.instances:
        insn_type_defs += f"LYRA_OP_{insn.name},\n"
    insn_type_defs += "};\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = "static inline int lyra_insn_type_has_left_var(enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.left_type == ARG_TYPE_VAR:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = "static inline int lyra_insn_type_has_right_var(enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.right_type == ARG_TYPE_VAR:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = "static inline int lyra_insn_type_has_dest(enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.has_dest:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

# Codegen data

with open("./src/bc_data/codegen.txt", "w") as insn_codegen_f:
    insn_codegen_defs = """\
#include <stdint.h>
#include "../insn.h"
#include "../comp.h"
#include "types.txt"
void lyra_insn_comp(struct lyra_insn *insn, struct lyra_comp *c){
"""

    for insn in Instruction.instances:
        insn_codegen_defs += f"if(insn->type == LYRA_OP_{insn.name}) {{\n"
        insn_codegen_defs += insn.gen_compiler()
        insn_codegen_defs += f"return;\n"
        insn_codegen_defs += f"}}\n"

    insn_codegen_defs += "}\n"
    insn_codegen_f.write(insn_codegen_defs)
