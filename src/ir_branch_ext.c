#include "ir.h"

#include <stddef.h>
#include <stdint.h>

/*
 * src/ir.c is compiled with amivm_ir_decode_words renamed to this symbol.
 * This thin public wrapper extends the established decoder with the 68000+
 * 16-bit displacement form of BRA/Bcc without duplicating the main decoder.
 */
int amivm_ir_decode_words_base(struct amivm_ir_block *block,
                               const uint16_t *words, size_t word_count);

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
    if (word_index >= word_count || word_index + 1u >= word_count)
        return rc;

    branch_word = words[word_index];
    if ((branch_word & 0xf000u) != 0x6000u || (branch_word & 0x00ffu) != 0u)
        return rc;

    condition = (uint8_t)((branch_word >> 8u) & 0x0fu);
    if (condition == 1u)
        return rc; /* BSR.w remains on the reference path for now. */

    displacement = (int32_t)(int16_t)words[word_index + 1u];

    /*
     * Existing branch execution/JIT uses guest_pc + 2 as its PC-relative
     * base and conditional fallthrough.  Point guest_pc at the extension
     * word and compensate the displacement by -2.  This gives:
     *
     *   taken      = opcode_pc + 2 + disp16
     *   fallthrough = opcode_pc + 4
     *
     * while retaining the established branch lowering unchanged.
     */
    op->opcode = condition == 0u ? AMIVM_IR_BRANCH : AMIVM_IR_BRANCH_CC;
    op->condition = condition == 0u ? AMIVM_IR_CC_T : condition;
    op->guest_pc += 2u;
    op->imm = displacement - 2;
    op->instruction_bytes = 4u;
    block->terminates = 1;
    block->guest_end_pc = op->guest_pc + 2u;
    return 0;
}
