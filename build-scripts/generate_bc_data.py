ARG_TYPE_NONE = 0
ARG_TYPE_VAR = 1
ARG_TYPE_I32 = 2
ARG_TYPE_F64 = 2
ARG_TYPE_BOOL = 3

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
        elif "c_bin_i32" in self.options:
            compiler.assign_dest_var()
            compiler.left_var()
            compiler.raw(self.options["c_bin_i32"])
            compiler.right_i32()
            return compiler.end()
        elif "c_bin_f64" in self.options:
            compiler.assign_dest_var()
            compiler.left_var()
            compiler.raw(self.options["c_bin_f64"])
            compiler.right_f64()
            return compiler.end()
        elif "c_bin_bool" in self.options:
            compiler.assign_dest_var()
            compiler.left_var()
            compiler.raw(self.options["c_bin_bool"])
            compiler.right_var()
            return compiler.end()
        elif "c_bin_func_imm" in self.options:
            compiler.assign_dest_var()
            compiler.raw(self.options["c_bin_func"] + "(")
            compiler.left_var()
            compiler.raw(",")
            self.options["c_bin_func_imm"](compiler)
            compiler.raw(")")
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
        else:
            return ""

# Move operations

def gen_mov_var(compiler):
    compiler.assign_dest_var()
    compiler.left_var()
Instruction("MOV_VAR", ARG_TYPE_VAR, c_codegen=gen_mov_var)

def gen_mov_i32(compiler):
    compiler.assign_dest_var()
    compiler.right_i32()
Instruction("MOV_I32", ARG_TYPE_NONE, ARG_TYPE_I32, c_codegen=gen_mov_i32)
Instruction("MOV_BOOL", ARG_TYPE_NONE, ARG_TYPE_BOOL, c_codegen=gen_mov_i32)

def gen_mov_f64(compiler):
    compiler.assign_dest_var()
    compiler.right_f64()
Instruction("MOV_F64", ARG_TYPE_NONE, ARG_TYPE_F64, c_codegen=gen_mov_f64)

# Value assertion

def gen_ensure_i32(compiler):
    compiler.assign_dest_var()
    compiler.raw("au_value_get_int(")
    compiler.left_var()
    compiler.raw(")")
Instruction("ENSURE_I32", ARG_TYPE_VAR, c_codegen=gen_ensure_i32)

def gen_ensure_f64(compiler):
    compiler.assign_dest_var()
    compiler.raw("au_value_get_float(")
    compiler.left_var()
    compiler.raw(")")
Instruction("ENSURE_F64", ARG_TYPE_VAR, c_codegen=gen_ensure_f64)

def gen_ensure_f64_prim(compiler):
    compiler.assign_dest_var()
    compiler.raw("(double)(")
    compiler.left_var()
    compiler.raw(")")
Instruction("ENSURE_F64_PRIM", ARG_TYPE_VAR, c_codegen=gen_ensure_f64_prim)

def gen_ensure_num(compiler):
    compiler.assign_dest_var()
    compiler.raw("au_num_from_value(")
    compiler.left_var()
    compiler.raw(")")
Instruction("ENSURE_NUM", ARG_TYPE_VAR, ARG_TYPE_VAR, c_codegen=gen_ensure_num)

# Binary operations

for (op, c_bin_func, c_bin_op, num_func) in [
    ("MUL", "au_value_mul", "*", "au_num_mul"),
    ("DIV", "au_value_div", "/", None),
    ("ADD", "au_value_add", "+", "au_num_add"),
    ("SUB", "au_value_sub", "-", "au_num_sub"),
]:
    Instruction(op + "_VAR", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=c_bin_func)
    Instruction(op + "_PRIM", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_op=c_bin_op)
    if num_func:
        Instruction(op + "_NUM_I32", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=num_func + "_i32")
        Instruction(op + "_NUM_F64", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=num_func + "_f64")

for (op, c_bin_op) in [
    ("BOR", "|"),
    ("BXOR", "^"),
    ("BAND", "&"),
    ("BSHL", "<<"),
    ("BSHR", ">>"),
    ("MOD", "%"),
]:
    Instruction(op + "_I32", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_op=c_bin_op)

# Comparison operations

for (op, c_bin_func, c_bin_op, num_func) in [
    ("EQ" , "au_value_eq", "==", "au_num_eq"),
    ("NEQ", "au_value_neq", "!=", "au_num_neq"),
    ("LT" , "au_value_lt", "<", "au_num_lt"),
    ("GT" , "au_value_gt", ">", "au_num_gt"),
    ("LEQ", "au_value_leq", "<=", "au_num_leq"),
    ("GEQ", "au_value_geq", ">=", "au_num_geq"),
]:
    Instruction(op + "_VAR", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=c_bin_func)
    Instruction(op + "_PRIM", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_op=c_bin_op)
    Instruction(op + "_NUM", ARG_TYPE_VAR, ARG_TYPE_VAR, c_bin_func=num_func)

# Call instructions

def gen_load_arg(compiler):
    compiler.assign_dest_var()
    compiler.raw("args[")
    compiler.right_i32()
    compiler.raw("]")
Instruction("LOAD_ARG", ARG_TYPE_NONE, ARG_TYPE_I32, c_codegen=gen_load_arg)

# Types data

with open("./src/bc_data/types.txt", "w") as insn_type_f:

    insn_type_defs = \
        "#pragma once\n" \
        '#include "platform.h"\n' \
        "enum lyra_insn_type {\n"
    for insn in Instruction.instances:
        insn_type_defs += f"LYRA_OP_{insn.name},\n"
    insn_type_defs += "};\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = \
    "static LYRA_ALWAYS_INLINE LYRA_UNUSED " \
    "int lyra_insn_type_has_left_var(enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.left_type == ARG_TYPE_VAR:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = \
    "static LYRA_ALWAYS_INLINE LYRA_UNUSED " \
    "int lyra_insn_type_has_right_var(LYRA_UNUSED enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.right_type == ARG_TYPE_VAR:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = \
    "static LYRA_ALWAYS_INLINE LYRA_UNUSED " \
    "int lyra_insn_type_has_dest(LYRA_UNUSED enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if insn.has_dest:
            insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
    insn_type_defs += "return 0;}\n"
    insn_type_f.write(insn_type_defs)

    insn_type_defs = \
    "static LYRA_ALWAYS_INLINE LYRA_UNUSED " \
    "int lyra_insn_type_has_side_effect(LYRA_UNUSED enum lyra_insn_type type) {\n"
    for insn in Instruction.instances:
        if "has_side_effect" in insn.options:
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
