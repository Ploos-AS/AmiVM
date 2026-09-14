#include "ir.h"

#include <string.h>

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

void amivm_ir_block_init(struct amivm_ir_block *block, uint32_t guest_pc)
{
    if (block == NULL) return;
    memset(block, 0, sizeof(*block));
    block->guest_start_pc = guest_pc;
    block->guest_end_pc = guest_pc;
}

static int emit(struct amivm_ir_block *block, enum amivm_ir_opcode opcode,
                uint8_t reg, uint8_t condition, int32_t imm, uint32_t pc)
{
    struct amivm_ir_op *op;
    if (block->op_count >= AMIVM_IR_MAX_OPS) return -2;
    op = &block->ops[block->op_count++];
    op->opcode = opcode;
    op->reg = reg;
    op->condition = condition;
    op->imm = imm;
    op->guest_pc = pc;
    return 0;
}

static void set_nz32(struct amivm_cpu_state *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x80000000u) != 0u) cpu->sr |= SR_N;
}

static void set_add32_flags(struct amivm_cpu_state *cpu, uint32_t lhs,
                            uint32_t rhs, uint32_t result)
{
    uint16_t flags = 0u;
    uint64_t wide = (uint64_t)lhs + (uint64_t)rhs;
    if (result == 0u) flags |= SR_Z;
    if ((result & 0x80000000u) != 0u) flags |= SR_N;
    if (((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0u) flags |= SR_V;
    if ((wide >> 32u) != 0u) flags |= (SR_C | SR_X);
    cpu->sr = (uint16_t)((cpu->sr & ~(SR_X | SR_N | SR_Z | SR_V | SR_C)) | flags);
}

static void set_sub32_flags(struct amivm_cpu_state *cpu, uint32_t lhs,
                            uint32_t rhs, uint32_t result)
{
    uint16_t flags = 0u;
    if (result == 0u) flags |= SR_Z;
    if ((result & 0x80000000u) != 0u) flags |= SR_N;
    if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0u) flags |= SR_V;
    if (rhs > lhs) flags |= (SR_C | SR_X);
    cpu->sr = (uint16_t)((cpu->sr & ~(SR_X | SR_N | SR_Z | SR_V | SR_C)) | flags);
}

static int condition_true(uint8_t condition, uint16_t sr)
{
    switch (condition) {
    case AMIVM_IR_CC_T: return 1;
    case AMIVM_IR_CC_NE: return (sr & SR_Z) == 0u;
    case AMIVM_IR_CC_EQ: return (sr & SR_Z) != 0u;
    case AMIVM_IR_CC_PL: return (sr & SR_N) == 0u;
    case AMIVM_IR_CC_MI: return (sr & SR_N) != 0u;
    default: return 0;
    }
}

int amivm_ir_decode_words(struct amivm_ir_block *block,
                          const uint16_t *words, size_t word_count)
{
    size_t i;
    uint32_t pc;

    if (block == NULL || words == NULL || word_count == 0u) return -1;
    pc = block->guest_start_pc;

    for (i = 0u; i < word_count; ++i, pc += 2u) {
        uint16_t op = words[i];

        if (op == 0x4e71u) {
            if (emit(block, AMIVM_IR_NOP, 0u, 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf100u) == 0x7000u) {
            uint8_t reg = (uint8_t)((op >> 9u) & 7u);
            int8_t imm8 = (int8_t)(op & 0xffu);
            if (emit(block, AMIVM_IR_MOVEQ, reg, 0u, (int32_t)imm8, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xfff8u) == 0x4280u) {
            if (emit(block, AMIVM_IR_CLR_L, (uint8_t)(op & 7u), 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xfff8u) == 0x4a80u) {
            if (emit(block, AMIVM_IR_TST_L, (uint8_t)(op & 7u), 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf1f8u) == 0x5080u || (op & 0xf1f8u) == 0x5180u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t quick = (uint8_t)((op >> 9u) & 7u);
            if (quick == 0u) quick = 8u;
            if (emit(block, (op & 0x0100u) != 0u ? AMIVM_IR_SUBQ_L : AMIVM_IR_ADDQ_L,
                     reg, 0u, (int32_t)quick, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf000u) == 0x6000u) {
            uint8_t condition = (uint8_t)((op >> 8u) & 0x0fu);
            int8_t disp8 = (int8_t)(op & 0xffu);
            if (disp8 == 0 || condition == 1u) goto unsupported;
            if (condition == 0u) {
                if (emit(block, AMIVM_IR_BRANCH, 0u, AMIVM_IR_CC_T,
                         (int32_t)disp8, pc) != 0) return -2;
            } else if (condition == AMIVM_IR_CC_NE || condition == AMIVM_IR_CC_EQ ||
                       condition == AMIVM_IR_CC_PL || condition == AMIVM_IR_CC_MI) {
                if (emit(block, AMIVM_IR_BRANCH_CC, 0u, condition,
                         (int32_t)disp8, pc) != 0) return -2;
            } else {
                goto unsupported;
            }
            block->terminates = 1;
            pc += 2u;
            block->guest_end_pc = pc;
            return 0;
        }

unsupported:
        if (emit(block, AMIVM_IR_EXIT, 0u, 0u, (int32_t)op, pc) != 0) return -2;
        block->terminates = 1;
        pc += 2u;
        block->guest_end_pc = pc;
        return 1;
    }

    block->guest_end_pc = pc;
    return 0;
}

int amivm_ir_execute(const struct amivm_ir_block *block,
                     struct amivm_cpu_state *cpu)
{
    size_t i;

    if (block == NULL || cpu == NULL) return -1;
    cpu->pc = block->guest_start_pc;

    for (i = 0u; i < block->op_count; ++i) {
        const struct amivm_ir_op *op = &block->ops[i];
        uint32_t lhs, rhs, result;
        switch (op->opcode) {
        case AMIVM_IR_NOP:
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_MOVEQ:
            if (op->reg >= 8u) return -2;
            cpu->d[op->reg] = (uint32_t)op->imm;
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_CLR_L:
            if (op->reg >= 8u) return -2;
            cpu->d[op->reg] = 0u;
            set_nz32(cpu, 0u);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_TST_L:
            if (op->reg >= 8u) return -2;
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_ADDQ_L:
            if (op->reg >= 8u) return -2;
            lhs = cpu->d[op->reg];
            rhs = (uint32_t)op->imm;
            result = lhs + rhs;
            cpu->d[op->reg] = result;
            set_add32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_SUBQ_L:
            if (op->reg >= 8u) return -2;
            lhs = cpu->d[op->reg];
            rhs = (uint32_t)op->imm;
            result = lhs - rhs;
            cpu->d[op->reg] = result;
            set_sub32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_BRANCH:
            cpu->pc = op->guest_pc + 2u + (uint32_t)op->imm;
            return 1;
        case AMIVM_IR_BRANCH_CC:
            cpu->pc = condition_true(op->condition, cpu->sr)
                          ? op->guest_pc + 2u + (uint32_t)op->imm
                          : op->guest_pc + 2u;
            return 1;
        case AMIVM_IR_EXIT:
            cpu->pc = op->guest_pc;
            return 2;
        default:
            return -3;
        }
    }

    return 0;
}
