#ifndef AMIVM_IR_H
#define AMIVM_IR_H

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"

#define AMIVM_IR_MAX_OPS 64u

enum amivm_ir_opcode {
    AMIVM_IR_NOP = 0,
    AMIVM_IR_MOVEQ,
    AMIVM_IR_BRANCH,
    AMIVM_IR_EXIT,
};

struct amivm_ir_op {
    enum amivm_ir_opcode opcode;
    uint8_t reg;
    int32_t imm;
    uint32_t guest_pc;
};

struct amivm_ir_block {
    uint32_t guest_start_pc;
    uint32_t guest_end_pc;
    size_t op_count;
    int terminates;
    struct amivm_ir_op ops[AMIVM_IR_MAX_OPS];
};

void amivm_ir_block_init(struct amivm_ir_block *block, uint32_t guest_pc);
int amivm_ir_decode_words(struct amivm_ir_block *block,
                          const uint16_t *words, size_t word_count);
int amivm_ir_execute(const struct amivm_ir_block *block,
                     struct amivm_cpu_state *cpu);

#endif
