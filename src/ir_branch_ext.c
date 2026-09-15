#include "ir.h"

#include <stddef.h>
#include <stdint.h>

#include "vm.h"

/* src/ir.c exports its established implementations under these names. */
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

static int write_stack_long(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                            uint32_t logical, uint32_t value)
{
    uint32_t physical[4];
    unsigned i;

    if (vm == NULL || (logical & 1u) != 0u) return -1;
    for (i = 0u; i < 4u; ++i) {
        if (amivm_mmu_translate(cpu, vm, logical + i, true,
                                supervisor_mode(cpu), &physical[i]) != AMIVM_MMU_OK)
            return -1;
    }
    for (i = 0u; i < 4u; ++i) {
        unsigned shift = (3u - i) * 8u;
        if (!amivm_write8(vm, physical[i], (uint8_t)(value >> shift))) return -1;
    }
    return 0;
}

int amivm_ir_decode_words(struct amivm_ir_block *block,
                          const uint16_t *words, size_t word_count)
{
    struct amivm_ir_op *op;
    uint16_t branch_word;
    uint8_t condition;
    size_t word_index;
    int32_t displacement;
    int rc;

    rc = amivm_ir_decode_words_base(block, words, word_count);
    if (rc != 1 || block == NULL || words == NULL || block->op_count == 0u)
        return rc;

    op = &block->ops[block->op_count - 1u];
    if (op->opcode != AMIVM_IR_EXIT || op->guest_pc < block->guest_start_pc)
        return rc;

    word_index = (size_t)((op->guest_pc - block->guest_start_pc) / 2u);
    if (word_index >= word_count) return rc;
    branch_word = words[word_index];
    if ((branch_word & 0xf000u) != 0x6000u) return rc;

    condition = (uint8_t)((branch_word >> 8u) & 0x0fu);

    if ((branch_word & 0x00ffu) != 0u) {
        if (condition != 1u) return rc;
        op->opcode = AMIVM_IR_BSR;
        op->imm = (int32_t)(int8_t)(branch_word & 0xffu);
        op->instruction_bytes = 2u;
        block->terminates = 1;
        return 0;
    }

    if (word_index + 1u >= word_count) return rc;
    displacement = (int32_t)(int16_t)words[word_index + 1u];

    if (condition == 1u) {
        op->opcode = AMIVM_IR_BSR;
        op->imm = displacement;
        op->instruction_bytes = 4u;
        block->terminates = 1;
        block->guest_end_pc = op->guest_pc + 4u;
        return 0;
    }

    /* Existing branch lowering uses guest_pc + 2 for both target base and
       conditional fallthrough, so retain the M2.35 extension-word shim. */
    op->opcode = condition == 0u ? AMIVM_IR_BRANCH : AMIVM_IR_BRANCH_CC;
    op->condition = condition == 0u ? AMIVM_IR_CC_T : condition;
    op->guest_pc += 2u;
    op->imm = displacement - 2;
    op->instruction_bytes = 4u;
    block->terminates = 1;
    block->guest_end_pc = op->guest_pc + 2u;
    return 0;
}

int amivm_ir_execute(const struct amivm_ir_block *block,
                     struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    struct amivm_ir_block prefix;
    const struct amivm_ir_op *call;
    uint32_t return_pc;
    uint32_t new_sp;
    int rc;

    if (block == NULL || cpu == NULL || block->op_count == 0u ||
        block->ops[block->op_count - 1u].opcode != AMIVM_IR_BSR)
        return amivm_ir_execute_base(block, cpu, vm);

    call = &block->ops[block->op_count - 1u];
    prefix = *block;
    prefix.ops[prefix.op_count - 1u].opcode = AMIVM_IR_EXIT;
    prefix.ops[prefix.op_count - 1u].instruction_bytes = 2u;

    rc = amivm_ir_execute_base(&prefix, cpu, vm);
    if (rc < 0) return rc;

    return_pc = call->guest_pc + call->instruction_bytes;
    new_sp = cpu->a[7] - 4u;
    if (write_stack_long(cpu, vm, new_sp, return_pc) != 0) return -4;
    cpu->a[7] = new_sp;
    sync_a7_bank(cpu);
    cpu->pc = call->guest_pc + 2u + (uint32_t)call->imm;
    return 1;
}
