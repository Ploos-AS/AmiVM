#include "ir.h"

#include <stddef.h>
#include <stdint.h>

#include "vm.h"

int amivm_ir_decode_words_base(struct amivm_ir_block *block,
                               const uint16_t *words, size_t word_count);
int amivm_ir_execute_base(const struct amivm_ir_block *block,
                          struct amivm_cpu_state *cpu, struct amivm_vm *vm);

static int supervisor_mode(const struct amivm_cpu_state *cpu)
{
    return (cpu->sr & 0x2000u) != 0u;
}

static void sync_a7_bank(struct amivm_cpu_state *cpu)
{
    if ((cpu->sr & 0x2000u) == 0u) cpu->usp = cpu->a[7];
    else if ((cpu->sr & 0x1000u) != 0u) cpu->msp = cpu->a[7];
    else cpu->isp = cpu->a[7];
}

static int translate_stack_long(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                                uint32_t logical, int write, uint32_t physical[4])
{
    unsigned i;
    if (vm == NULL || (logical & 1u) != 0u) return -1;
    for (i = 0u; i < 4u; ++i) {
        if (amivm_mmu_translate(cpu, vm, logical + i, write != 0,
                                supervisor_mode(cpu), &physical[i]) != AMIVM_MMU_OK)
            return -1;
    }
    return 0;
}

static int write_stack_long(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                            uint32_t logical, uint32_t value)
{
    uint32_t physical[4]; unsigned i;
    if (translate_stack_long(cpu, vm, logical, 1, physical) != 0) return -1;
    for (i = 0u; i < 4u; ++i) {
        unsigned shift = (3u - i) * 8u;
        if (!amivm_write8(vm, physical[i], (uint8_t)(value >> shift))) return -1;
    }
    return 0;
}

static int read_stack_long(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                           uint32_t logical, uint32_t *value)
{
    uint32_t physical[4]; uint8_t byte[4]; unsigned i;
    if (value == NULL || translate_stack_long(cpu, vm, logical, 0, physical) != 0) return -1;
    for (i = 0u; i < 4u; ++i) if (!amivm_read8(vm, physical[i], &byte[i])) return -1;
    *value = ((uint32_t)byte[0] << 24u) | ((uint32_t)byte[1] << 16u) |
             ((uint32_t)byte[2] << 8u) | (uint32_t)byte[3];
    return 0;
}

static uint32_t index_value(const struct amivm_ir_op *op,
                            const struct amivm_cpu_state *cpu)
{
    uint32_t value = op->index_is_addr ? cpu->a[op->index_reg] : cpu->d[op->index_reg];
    if (!op->index_long) value = (uint32_t)(int32_t)(int16_t)(value & 0xffffu);
    return value * (uint32_t)op->index_scale;
}

static uint32_t control_target(const struct amivm_ir_op *op,
                               const struct amivm_cpu_state *cpu)
{
    if (op->ea_mode == AMIVM_IR_EA_AN) return cpu->a[op->reg];
    if (op->ea_mode == AMIVM_IR_EA_D16_AN) return cpu->a[op->reg] + (uint32_t)op->imm;
    if (op->ea_mode == AMIVM_IR_EA_PC_D16) return op->guest_pc + 2u + (uint32_t)op->imm;
    if (op->ea_mode == AMIVM_IR_EA_D8_AN_XN)
        return cpu->a[op->reg] + (uint32_t)op->imm + index_value(op, cpu);
    if (op->ea_mode == AMIVM_IR_EA_PC_D8_XN)
        return op->guest_pc + 2u + (uint32_t)op->imm + index_value(op, cpu);
    return (uint32_t)op->imm;
}

static int decode_brief_index(struct amivm_ir_op *op, uint16_t ext)
{
    if ((ext & 0x0100u) != 0u) return 0;
    op->index_is_addr = (uint8_t)((ext >> 15u) & 1u);
    op->index_reg = (uint8_t)((ext >> 12u) & 7u);
    op->index_long = (uint8_t)((ext >> 11u) & 1u);
    op->index_scale = (uint8_t)(1u << ((ext >> 9u) & 3u));
    op->imm = (int32_t)(int8_t)(ext & 0xffu);
    return 1;
}

int amivm_ir_decode_words(struct amivm_ir_block *block,
                          const uint16_t *words, size_t word_count)
{
    struct amivm_ir_op *op; uint16_t branch_word; uint8_t condition;
    size_t word_index; int32_t displacement; int rc;
    rc = amivm_ir_decode_words_base(block, words, word_count);
    if (rc != 1 || block == NULL || words == NULL || block->op_count == 0u) return rc;
    op = &block->ops[block->op_count - 1u];
    if (op->opcode != AMIVM_IR_EXIT || op->guest_pc < block->guest_start_pc) return rc;
    word_index = (size_t)((op->guest_pc - block->guest_start_pc) / 2u);
    if (word_index >= word_count) return rc;
    branch_word = words[word_index];

    if (branch_word == 0x4e75u) {
        op->opcode = AMIVM_IR_RTS; op->instruction_bytes = 2u; block->terminates = 1; return 0;
    }
    if ((branch_word & 0xfff8u) == 0x4e90u || (branch_word & 0xfff8u) == 0x4ed0u) {
        op->opcode = (branch_word & 0x0040u) != 0u ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        op->reg = (uint8_t)(branch_word & 7u); op->ea_mode = AMIVM_IR_EA_AN;
        op->instruction_bytes = 2u; block->terminates = 1; return 0;
    }
    if ((branch_word & 0xfff8u) == 0x4ea8u || (branch_word & 0xfff8u) == 0x4ee8u) {
        if (word_index + 1u >= word_count) return rc;
        op->opcode = (branch_word & 0x0040u) != 0u ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        op->reg = (uint8_t)(branch_word & 7u); op->ea_mode = AMIVM_IR_EA_D16_AN;
        op->imm = (int32_t)(int16_t)words[word_index + 1u]; op->instruction_bytes = 4u;
        block->guest_end_pc = op->guest_pc + 4u; block->terminates = 1; return 0;
    }
    if (branch_word == 0x4ebau || branch_word == 0x4efau) {
        if (word_index + 1u >= word_count) return rc;
        op->opcode = branch_word == 0x4efau ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        op->ea_mode = AMIVM_IR_EA_PC_D16; op->imm = (int32_t)(int16_t)words[word_index + 1u];
        op->instruction_bytes = 4u; block->guest_end_pc = op->guest_pc + 4u;
        block->terminates = 1; return 0;
    }
    /* M2.51: brief indexed d8(An,Xn) and d8(PC,Xn). Full extension follows later. */
    if ((branch_word & 0xfff8u) == 0x4eb0u || (branch_word & 0xfff8u) == 0x4ef0u ||
        branch_word == 0x4ebbu || branch_word == 0x4efbu) {
        if (word_index + 1u >= word_count) return rc;
        if (!decode_brief_index(op, words[word_index + 1u])) return rc;
        op->opcode = (branch_word & 0x0040u) != 0u ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        if (branch_word == 0x4ebbu || branch_word == 0x4efbu) {
            op->ea_mode = AMIVM_IR_EA_PC_D8_XN; op->reg = 0u;
        } else {
            op->ea_mode = AMIVM_IR_EA_D8_AN_XN; op->reg = (uint8_t)(branch_word & 7u);
        }
        op->instruction_bytes = 4u; block->guest_end_pc = op->guest_pc + 4u;
        block->terminates = 1; return 0;
    }
    if (branch_word == 0x4eb8u || branch_word == 0x4ef8u) {
        if (word_index + 1u >= word_count) return rc;
        op->opcode = branch_word == 0x4ef8u ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        op->ea_mode = AMIVM_IR_EA_ABS_W; op->imm = (int32_t)(int16_t)words[word_index + 1u];
        op->instruction_bytes = 4u; block->guest_end_pc = op->guest_pc + 4u;
        block->terminates = 1; return 0;
    }
    if (branch_word == 0x4eb9u || branch_word == 0x4ef9u) {
        if (word_index + 2u >= word_count) return rc;
        op->opcode = branch_word == 0x4ef9u ? AMIVM_IR_JMP : AMIVM_IR_JSR;
        op->ea_mode = AMIVM_IR_EA_ABS_L;
        op->imm = (int32_t)(((uint32_t)words[word_index + 1u] << 16u) | (uint32_t)words[word_index + 2u]);
        op->instruction_bytes = 6u; block->guest_end_pc = op->guest_pc + 6u;
        block->terminates = 1; return 0;
    }
    if ((branch_word & 0xf000u) != 0x6000u) return rc;
    condition = (uint8_t)((branch_word >> 8u) & 0x0fu);
    if ((branch_word & 0x00ffu) != 0u) {
        if (condition != 1u) return rc;
        op->opcode = AMIVM_IR_BSR; op->imm = (int32_t)(int8_t)(branch_word & 0xffu);
        op->instruction_bytes = 2u; block->terminates = 1; return 0;
    }
    if (word_index + 1u >= word_count) return rc;
    displacement = (int32_t)(int16_t)words[word_index + 1u];
    if (condition == 1u) {
        op->opcode = AMIVM_IR_BSR; op->imm = displacement; op->instruction_bytes = 4u;
        block->terminates = 1; block->guest_end_pc = op->guest_pc + 4u; return 0;
    }
    op->opcode = condition == 0u ? AMIVM_IR_BRANCH : AMIVM_IR_BRANCH_CC;
    op->condition = condition == 0u ? AMIVM_IR_CC_T : condition; op->guest_pc += 2u;
    op->imm = displacement - 2; op->instruction_bytes = 4u; block->terminates = 1;
    block->guest_end_pc = op->guest_pc + 2u; return 0;
}

int amivm_ir_execute(const struct amivm_ir_block *block,
                     struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    struct amivm_ir_block prefix; const struct amivm_ir_op *terminal;
    uint32_t return_pc, new_sp, target_pc; int rc;
    if (block == NULL || cpu == NULL || block->op_count == 0u) return amivm_ir_execute_base(block, cpu, vm);
    terminal = &block->ops[block->op_count - 1u];
    if (terminal->opcode != AMIVM_IR_BSR && terminal->opcode != AMIVM_IR_RTS &&
        terminal->opcode != AMIVM_IR_JSR && terminal->opcode != AMIVM_IR_JMP)
        return amivm_ir_execute_base(block, cpu, vm);
    prefix = *block; prefix.ops[prefix.op_count - 1u].opcode = AMIVM_IR_EXIT;
    prefix.ops[prefix.op_count - 1u].instruction_bytes = 2u;
    rc = amivm_ir_execute_base(&prefix, cpu, vm); if (rc < 0) return rc;
    if (terminal->opcode == AMIVM_IR_JMP) { cpu->pc = control_target(terminal, cpu); return 1; }
    if (terminal->opcode == AMIVM_IR_JSR) {
        target_pc = control_target(terminal, cpu); return_pc = terminal->guest_pc + terminal->instruction_bytes;
        new_sp = cpu->a[7] - 4u; if (write_stack_long(cpu, vm, new_sp, return_pc) != 0) return -4;
        cpu->a[7] = new_sp; sync_a7_bank(cpu); cpu->pc = target_pc; return 1;
    }
    if (terminal->opcode == AMIVM_IR_BSR) {
        return_pc = terminal->guest_pc + terminal->instruction_bytes; new_sp = cpu->a[7] - 4u;
        if (write_stack_long(cpu, vm, new_sp, return_pc) != 0) return -4;
        cpu->a[7] = new_sp; sync_a7_bank(cpu);
        cpu->pc = terminal->guest_pc + 2u + (uint32_t)terminal->imm; return 1;
    }
    if (read_stack_long(cpu, vm, cpu->a[7], &return_pc) != 0) return -4;
    cpu->a[7] += 4u; sync_a7_bank(cpu); cpu->pc = return_pc; return 1;
}
