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
                uint8_t reg, uint8_t src_reg, uint8_t condition,
                int32_t imm, uint32_t pc)
{
    struct amivm_ir_op *op;
    if (block->op_count >= AMIVM_IR_MAX_OPS) return -2;
    op = &block->ops[block->op_count++];
    op->opcode = opcode;
    op->reg = reg;
    op->src_reg = src_reg;
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

static void set_logic32_flags(struct amivm_cpu_state *cpu, uint32_t value)
{
    set_nz32(cpu, value);
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
                            uint32_t rhs, uint32_t result, int update_x)
{
    uint16_t flags = 0u;
    uint16_t clear = (uint16_t)(SR_N | SR_Z | SR_V | SR_C);
    if (result == 0u) flags |= SR_Z;
    if ((result & 0x80000000u) != 0u) flags |= SR_N;
    if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0u) flags |= SR_V;
    if (rhs > lhs) flags |= SR_C;
    if (update_x) {
        clear |= SR_X;
        if (rhs > lhs) flags |= SR_X;
    }
    cpu->sr = (uint16_t)((cpu->sr & ~clear) | flags);
}

static int condition_true(uint8_t condition, uint16_t sr)
{
    const int c = (sr & SR_C) != 0u;
    const int v = (sr & SR_V) != 0u;
    const int z = (sr & SR_Z) != 0u;
    const int n = (sr & SR_N) != 0u;

    switch (condition) {
    case AMIVM_IR_CC_T: return 1;
    case AMIVM_IR_CC_HI: return !c && !z;
    case AMIVM_IR_CC_LS: return c || z;
    case AMIVM_IR_CC_CC: return !c;
    case AMIVM_IR_CC_CS: return c;
    case AMIVM_IR_CC_NE: return !z;
    case AMIVM_IR_CC_EQ: return z;
    case AMIVM_IR_CC_VC: return !v;
    case AMIVM_IR_CC_VS: return v;
    case AMIVM_IR_CC_PL: return !n;
    case AMIVM_IR_CC_MI: return n;
    case AMIVM_IR_CC_GE: return n == v;
    case AMIVM_IR_CC_LT: return n != v;
    case AMIVM_IR_CC_GT: return !z && (n == v);
    case AMIVM_IR_CC_LE: return z || (n != v);
    default: return 0;
    }
}

static int emit_reg_alu(struct amivm_ir_block *block, uint16_t op, uint32_t pc)
{
    enum amivm_ir_opcode ir_op;
    uint8_t dst;
    uint8_t src;

    if ((op & 0xf1f8u) == 0x8080u) ir_op = AMIVM_IR_OR_L;
    else if ((op & 0xf1f8u) == 0x9080u) ir_op = AMIVM_IR_SUB_L;
    else if ((op & 0xf1f8u) == 0xb080u) ir_op = AMIVM_IR_CMP_L;
    else if ((op & 0xf1f8u) == 0xb180u) ir_op = AMIVM_IR_EOR_L;
    else if ((op & 0xf1f8u) == 0xc080u) ir_op = AMIVM_IR_AND_L;
    else if ((op & 0xf1f8u) == 0xd080u) ir_op = AMIVM_IR_ADD_L;
    else return 0;

    if (ir_op == AMIVM_IR_EOR_L) {
        src = (uint8_t)((op >> 9u) & 7u);
        dst = (uint8_t)(op & 7u);
    } else {
        dst = (uint8_t)((op >> 9u) & 7u);
        src = (uint8_t)(op & 7u);
    }
    return emit(block, ir_op, dst, src, 0u, 0, pc) == 0 ? 1 : -1;
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
        int alu_rc;

        if (op == 0x4e71u) {
            if (emit(block, AMIVM_IR_NOP, 0u, 0u, 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf100u) == 0x7000u) {
            uint8_t reg = (uint8_t)((op >> 9u) & 7u);
            int8_t imm8 = (int8_t)(op & 0xffu);
            if (emit(block, AMIVM_IR_MOVEQ, reg, 0u, 0u, (int32_t)imm8, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xfff8u) == 0x4280u) {
            if (emit(block, AMIVM_IR_CLR_L, (uint8_t)(op & 7u), 0u, 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xfff8u) == 0x4a80u) {
            if (emit(block, AMIVM_IR_TST_L, (uint8_t)(op & 7u), 0u, 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf1f8u) == 0x5080u || (op & 0xf1f8u) == 0x5180u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t quick = (uint8_t)((op >> 9u) & 7u);
            if (quick == 0u) quick = 8u;
            if (emit(block, (op & 0x0100u) != 0u ? AMIVM_IR_SUBQ_L : AMIVM_IR_ADDQ_L,
                     reg, 0u, 0u, (int32_t)quick, pc) != 0) return -2;
            continue;
        }

        alu_rc = emit_reg_alu(block, op, pc);
        if (alu_rc < 0) return -2;
        if (alu_rc > 0) continue;

        if ((op & 0xf000u) == 0x6000u) {
            uint8_t condition = (uint8_t)((op >> 8u) & 0x0fu);
            int8_t disp8 = (int8_t)(op & 0xffu);
            if (disp8 == 0 || condition == 1u) goto unsupported;
            if (condition == 0u) {
                if (emit(block, AMIVM_IR_BRANCH, 0u, 0u, AMIVM_IR_CC_T,
                         (int32_t)disp8, pc) != 0) return -2;
            } else {
                if (emit(block, AMIVM_IR_BRANCH_CC, 0u, 0u, condition,
                         (int32_t)disp8, pc) != 0) return -2;
            }
            block->terminates = 1;
            pc += 2u;
            block->guest_end_pc = pc;
            return 0;
        }

unsupported:
        if (emit(block, AMIVM_IR_EXIT, 0u, 0u, 0u, (int32_t)op, pc) != 0) return -2;
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
        if (op->reg >= 8u || op->src_reg >= 8u) {
            if (op->opcode != AMIVM_IR_NOP && op->opcode != AMIVM_IR_BRANCH &&
                op->opcode != AMIVM_IR_BRANCH_CC && op->opcode != AMIVM_IR_EXIT)
                return -2;
        }
        switch (op->opcode) {
        case AMIVM_IR_NOP:
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_MOVEQ:
            cpu->d[op->reg] = (uint32_t)op->imm;
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_CLR_L:
            cpu->d[op->reg] = 0u;
            set_nz32(cpu, 0u);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_TST_L:
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_ADDQ_L:
            lhs = cpu->d[op->reg];
            rhs = (uint32_t)op->imm;
            result = lhs + rhs;
            cpu->d[op->reg] = result;
            set_add32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_SUBQ_L:
            lhs = cpu->d[op->reg];
            rhs = (uint32_t)op->imm;
            result = lhs - rhs;
            cpu->d[op->reg] = result;
            set_sub32_flags(cpu, lhs, rhs, result, 1);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_ADD_L:
            lhs = cpu->d[op->reg];
            rhs = cpu->d[op->src_reg];
            result = lhs + rhs;
            cpu->d[op->reg] = result;
            set_add32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_SUB_L:
            lhs = cpu->d[op->reg];
            rhs = cpu->d[op->src_reg];
            result = lhs - rhs;
            cpu->d[op->reg] = result;
            set_sub32_flags(cpu, lhs, rhs, result, 1);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_CMP_L:
            lhs = cpu->d[op->reg];
            rhs = cpu->d[op->src_reg];
            result = lhs - rhs;
            set_sub32_flags(cpu, lhs, rhs, result, 0);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_AND_L:
            result = cpu->d[op->reg] & cpu->d[op->src_reg];
            cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_OR_L:
            result = cpu->d[op->reg] | cpu->d[op->src_reg];
            cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_EOR_L:
            result = cpu->d[op->reg] ^ cpu->d[op->src_reg];
            cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result);
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
