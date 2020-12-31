/*
riscv32i.c - RISC-V Interger instruction emulator
Copyright (C) 2020  Mr0maks <mr.maks0443@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <assert.h>

#include "riscv.h"
#include "riscv32.h"
#include "riscv32i.h"

// translate register number into abi name
const char *riscv32i_translate_register(uint32_t reg)
{
    assert(reg < REGISTERS_MAX);
    switch (reg) {
    case REGISTER_ZERO: return "zero";
    case REGISTER_X1: return "ra";
    case REGISTER_X2: return "sp";
    case REGISTER_X3: return "gp";
    case REGISTER_X4: return "tp";
    case REGISTER_X5: return "t0";
    case REGISTER_X6: return "t1";
    case REGISTER_X7: return "t2";
    case REGISTER_X8: return "s0/fp";
    case REGISTER_X9: return "s1";
    case REGISTER_X10: return "a0";
    case REGISTER_X11: return "a1";
    case REGISTER_X12: return "a2";
    case REGISTER_X13: return "a3";
    case REGISTER_X14: return "a4";
    case REGISTER_X15: return "a5";
    case REGISTER_X16: return "a6";
    case REGISTER_X17: return "a7";
    case REGISTER_X18: return "s2";
    case REGISTER_X19: return "s3";
    case REGISTER_X20: return "s4";
    case REGISTER_X21: return "s5";
    case REGISTER_X22: return "s6";
    case REGISTER_X23: return "s7";
    case REGISTER_X24: return "s8";
    case REGISTER_X25: return "s9";
    case REGISTER_X26: return "s10";
    case REGISTER_X27: return "s11";
    case REGISTER_X28: return "t3";
    case REGISTER_X29: return "t4";
    case REGISTER_X30: return "t5";
    case REGISTER_X31: return "t6";
    case REGISTER_PC: return "pc";
    default: return "unknown";
    }
}

static void riscv32i_lui(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t imm = (instruction & 0xFFFFF000);

    riscv32i_write_register_u(vm, rds, imm);

    printf("lui %u,%u\n", rds, imm);
    printf("RV32I: LUI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_auipc(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t imm = (instruction & 0xFFFFF000);
    uint32_t pc = riscv32i_read_register_u(vm, REGISTER_PC);

    riscv32i_write_register_u(vm, rds, pc + imm);

    printf("auipc %u,%u\n", rds, imm);
    printf("RV32I: AUIPC instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_jal(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: JAL instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_srli_srai(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t shamt = (instruction >> 20) & 0x1F;

    if( instruction & (1u << 30) )
    {
        int32_t reg1 = riscv32i_read_register_s(vm, rs1);
        riscv32i_write_register_s(vm, rds, (reg1 >> shamt));
        printf("srai %u,%u,%i\n", rds, rs1, shamt);
    } else {
        uint32_t reg1 = riscv32i_read_register_s(vm, rs1);
        riscv32i_write_register_s(vm, rds, (reg1 >> shamt));
        printf("srli %u,%u,%i\n", rds, rs1, shamt);
    }

    printf("RV32I: SRLI/SRAI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_add_sub(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);

    if(func7 == 0x20)
    {
        int32_t reg1 = riscv32i_read_register_s(vm, rs1), reg2 = riscv32i_read_register_s(vm, rs2);

        int32_t result = (reg1 - reg2);
        riscv32i_write_register_s(vm, rds, result);
        //printf("sub %u,%u,%u\n", rds, rs1, rs2);
    }
    else
    {
        int32_t reg1 = riscv32i_read_register_s(vm, rs1), reg2 = riscv32i_read_register_s(vm, rs2);

        int32_t result = (reg1 + reg2);
        riscv32i_write_register_s(vm, rds, result);
        //printf("add %u,%u,%u\n", rds, rs1, rs2);
    }
    //printf("RV32I: ADD/SUB instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_ecall_ebreak(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: ECALL/EBREAK instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_srl_sra(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);

    if(func7 == 0x20)
    {
        //NOTE: check for ub here?
        int32_t reg1 = riscv32i_read_register_s(vm, rs1), reg2 = riscv32i_read_register_s(vm, rs2);
        int32_t result = (reg1 >> reg2);
        riscv32i_write_register_s(vm, rds, result);
        //printf("sra %u,%u,%u\n", rds, rs1, rs2);
    } else {
        //NOTE: check for ub here?
        uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);
        uint32_t result = (reg1 >> reg2);
        riscv32i_write_register_u(vm, rds, result);
        //printf("srl %u,%u,%u\n", rds, rs1, rs2);
    }

    printf("RV32I: SRL/SRA instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_jalr(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);

    printf("RV32I: JALR instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_beq(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BEQ instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_bne(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BNE instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_blt(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BLT instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_bge(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BGE instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_bltu(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BLTU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_bgeu(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: BGEU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_lb(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: LB instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_lh(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: LH instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_lw(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: LW instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_lbu(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: LBU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_lhu(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: LHU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sb(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: SB instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sh(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: SH instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sw(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: SW instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_addi(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    int32_t imm = (instruction >> 20);

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    int32_t result = riscv32i_read_register_s(vm, rs1) + imm;
    riscv32i_write_register_s(vm, rds, result);
    printf("addi %u,%u,%i\n", rds, rs1, imm);
    printf("RV32I: ADDI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_slti(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    int32_t imm = (instruction >> 20);
    int32_t reg1 = riscv32i_read_register_s(vm, rs1);

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    if( reg1 > imm )
        riscv32i_write_register_s(vm, rds, 1);
    else
        riscv32i_write_register_s(vm, rds, 0);

    printf("RV32I: SLTI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sltiu(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t imm = (instruction >> 20);
    uint32_t reg1 = riscv32i_read_register_u(vm, rs1);

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    if( reg1 > imm )
        riscv32i_write_register_s(vm, rds, 1);
    else
        riscv32i_write_register_s(vm, rds, 0);

    printf("RV32I: SLTIU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_xori(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    int32_t imm = 0;

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    printf("RV32I: XORI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_ori(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    int32_t imm = 0;

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    printf("RV32I: ORI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_andi(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    int32_t imm = 0;

    // extend 12 bit signed imm into 32 bit
    if( imm & (1 << 11) )
        imm |= 0xFFFFF000;

    printf("RV32I: ANDI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_slli(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t shamt = (instruction >> 20) & 0x1F;
    uint32_t reg1 = riscv32i_read_register_u(vm, rs1);

    uint32_t result = (shamt << reg1);

    riscv32i_write_register_u(vm, rds, result);
    printf("slli %u,%u,%u\n", rds, rs1, shamt);
    printf("RV32I: SLLI instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sll(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);
    //NOTE: check for ub here?
    uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);

    uint32_t result = (reg1 << reg2);

    riscv32i_write_register_u(vm, rds, result);
    //printf("sll %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: SLL instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_slt(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);
    int32_t reg1 = riscv32i_read_register_s(vm, rs1), reg2 = riscv32i_read_register_s(vm, rs2);

    int32_t result = 0;
    if( reg1 < reg2 )
        result = 1;

    riscv32i_write_register_s(vm, rds, result);
    //printf("slt %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: SLT instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_sltu(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);
    uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);

    uint32_t result = 0;
    if( reg1 < reg2 )
        result = 1;

    riscv32i_write_register_u(vm, rds, result);
    //printf("sltu %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: SLTU instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_xor(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);
    uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);

    uint32_t result = (reg1 ^ reg2);

    riscv32i_write_register_u(vm, rds, result);
    //printf("xor %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: XOR instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_or(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);

    uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);

    uint32_t result = (reg1 | reg2);

    riscv32i_write_register_u(vm, rds, result);
    //printf("or %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: OR instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_and(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t rds = ((instruction >> 7) & 0x1f);
    uint32_t func3 = ((instruction >> 12) & 0x3);
    uint32_t rs1 = ((instruction >> 15) & 0x1f);
    uint32_t rs2 = ((instruction >> 20) & 0x1f);
    uint32_t func7 = (instruction >> 25);

    uint32_t reg1 = riscv32i_read_register_u(vm, rs1), reg2 = riscv32i_read_register_u(vm, rs2);

    uint32_t result = (reg1 & reg2);

    riscv32i_write_register_u(vm, rds, result);
    //printf("and %u,%u,%u\n", rds, rs1, rs2);
    //printf("RV32I: AND instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_fence(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: FENCE instruction 0x%x in VM %p\n", instruction, vm);
}

static void riscv32i_(risc32_vm_state_t *vm, uint32_t instruction)
{
    printf("RV32I: AAA instruction 0x%x in VM %p\n", instruction, vm);
}

void riscv32i_init()
{
    smudge_opcode_func3(RV32I_LUI, riscv32i_lui);
    smudge_opcode_func3(RV32I_AUIPC, riscv32i_auipc);
    smudge_opcode_func3(RV32I_JAL, riscv32i_jal);

    riscv32_opcodes[RV32I_SRLI_SRAI] = riscv32i_srli_srai;
    riscv32_opcodes[RV32I_ADD_SUB] = riscv32i_add_sub;
    riscv32_opcodes[RV32I_ECALL_EBREAK] = riscv32i_ecall_ebreak;
    riscv32_opcodes[RV32I_SRL_SRA] = riscv32i_srl_sra;

    riscv32_opcodes[RV32I_JALR] = riscv32i_jalr;
    riscv32_opcodes[RV32I_BEQ] = riscv32i_beq;
    riscv32_opcodes[RV32I_BNE] = riscv32i_bne;
    riscv32_opcodes[RV32I_BLT] = riscv32i_blt;
    riscv32_opcodes[RV32I_BGE] = riscv32i_bge;
    riscv32_opcodes[RV32I_BLTU] = riscv32i_bltu;
    riscv32_opcodes[RV32I_BGEU] = riscv32i_bgeu;
    riscv32_opcodes[RV32I_LB] = riscv32i_lb;
    riscv32_opcodes[RV32I_LH] = riscv32i_lh;
    riscv32_opcodes[RV32I_LW] = riscv32i_lw;
    riscv32_opcodes[RV32I_LBU] = riscv32i_lbu;
    riscv32_opcodes[RV32I_LHU] = riscv32i_lhu;
    riscv32_opcodes[RV32I_SB] = riscv32i_sb;
    riscv32_opcodes[RV32I_SH] = riscv32i_sh;
    riscv32_opcodes[RV32I_SW] = riscv32i_sw;
    riscv32_opcodes[RV32I_ADDI] = riscv32i_addi;
    riscv32_opcodes[RV32I_SLTI] = riscv32i_slti;
    riscv32_opcodes[RV32I_SLTIU] = riscv32i_sltiu;
    riscv32_opcodes[RV32I_XORI] = riscv32i_xori;
    riscv32_opcodes[RV32I_ORI] = riscv32i_ori;
    riscv32_opcodes[RV32I_ANDI] = riscv32i_andi;
    riscv32_opcodes[RV32I_SLLI] = riscv32i_slli;
    riscv32_opcodes[RV32I_SLL] = riscv32i_sll;
    riscv32_opcodes[RV32I_SLT] = riscv32i_slt;
    riscv32_opcodes[RV32I_SLTU] = riscv32i_sltu;
    riscv32_opcodes[RV32I_XOR] = riscv32i_xor;
    riscv32_opcodes[RV32I_OR] = riscv32i_or;
    riscv32_opcodes[RV32I_AND] = riscv32i_and;
    riscv32_opcodes[RV32I_FENCE] = riscv32i_fence;
}

// We already check instruction for correct code
void riscv32i_emulate(risc32_vm_state_t *vm, uint32_t instruction)
{
    uint32_t funcid = RISCV32_GET_FUNCID(instruction);
    riscv32_opcodes[funcid](vm, instruction);
}
