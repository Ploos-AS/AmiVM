#include "ir.h"

#include <string.h>

void amivm_ir_block_init(struct amivm_ir_block *block, uint32_t guest_pc)
{
    if (block == NULL) return;
    memset(block, 0, sizeof(*block));
    block->guest_start_pc = guest_pc;
    block->guest_end_pc = guest_pc;
}

static int emit(struct amivm_ir_block *block, enum amivm_ir_opcode opcode,
                uint8_t reg, int32_t imm, uint32_t pc)
{
    struct amivm_ir_op *op;
    if (block->op_count >= AMIVM_IR_MAX_OPS) return -2;
    op = &block->ops[block->op_count++];
    op->opcode = opcode;
    op->reg = reg;
    op->imm = imm;
    op->guest_pc = pc;
    return 0;
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
            if (emit(block, AMIVM_IR_NOP, 0u, 0, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xf100u) == 0x7000u) {
            uint8_t reg = (uint8_t)((op >> 9u) & 7u);
            int8_t imm8 = (int8_t)(op & 0xffu);
            if (emit(block, AMIVM_IR_MOVEQ, reg, (int32_t)imm8, pc) != 0) return -2;
            continue;
        }

        if ((op & 0xff00u) == 0x6000u) {
            int8_t disp8 = (int8_t)(op & 0xffu);
            if (disp8 == 0) return -3;
            if (emit(block, AMIVM_IR_BRANCH, 0u, (int32_t)disp8, pc) != 0) return -2;
            block->terminates = 1;
            pc += 2u;
            block->guest_end_pc = pc;
            return 0;
        }

        if (emit(block, AMIVM_IR_EXIT, 0u, (int32_t)op, pc) != 0) return -2;
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
        switch (op->opcode) {
        case AMIVM_IR_NOP:
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_MOVEQ:
            if (op->reg >= 8u) return -2;
            cpu->d[op->reg] = (uint32_t)op->imm;
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_BRANCH:
            cpu->pc = op->guest_pc + 2u + (uint32_t)op->imm;
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
