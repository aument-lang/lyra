ARG_TYPE_NONE = 0
ARG_TYPE_VAR = 1
ARG_TYPE_I32 = 2
ARG_TYPE_F64 = 2

class Instruction:
    instances = []
    def __init__(self, name, left_type, right_type=ARG_TYPE_NONE, has_dest=True):
        self.name = name
        self.left_type = left_type
        self.right_type = right_type
        self.has_dest = has_dest
        Instruction.instances.append(self)


Instruction("MOV_VAR", ARG_TYPE_VAR)
Instruction("MOV_I32", ARG_TYPE_I32)
Instruction("MOV_F64", ARG_TYPE_F64)

Instruction("ADD_VAR", ARG_TYPE_VAR, ARG_TYPE_VAR)
Instruction("ADD_I32", ARG_TYPE_VAR, ARG_TYPE_I32)
Instruction("ADD_F64", ARG_TYPE_VAR, ARG_TYPE_F64)

# Generator

insn_type_f = open("./src/bc_data/types.txt", "w")

insn_type_defs = "enum lyra_insn_type {\n"
for insn in Instruction.instances:
    insn_type_defs += f"LYRA_OP_{insn.name},\n"
insn_type_defs += "};\n"
insn_type_f.write(insn_type_defs)

insn_type_defs = "int lyra_insn_type_has_left_var(enum lyra_insn_type type) {\n"
for insn in Instruction.instances:
    if insn.left_type == ARG_TYPE_VAR:
        insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
insn_type_defs += "return 0;}\n"
insn_type_f.write(insn_type_defs)

insn_type_defs = "int lyra_insn_type_has_right_var(enum lyra_insn_type type) {\n"
for insn in Instruction.instances:
    if insn.right_type == ARG_TYPE_VAR:
        insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
insn_type_defs += "return 0;}\n"
insn_type_f.write(insn_type_defs)

insn_type_defs = "int lyra_insn_type_has_dest(enum lyra_insn_type type) {\n"
for insn in Instruction.instances:
    if insn.has_dest:
        insn_type_defs += f"if(type == LYRA_OP_{insn.name}) return 1;\n"
insn_type_defs += "return 0;}\n"
insn_type_f.write(insn_type_defs)
