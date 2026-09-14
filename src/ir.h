#ifndef AMIVM_IR_H
#define AMIVM_IR_H

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"

#define AMIVM_IR_MAX_OPS 64u

enum amivm_ir_opcode {
    AMIVM_IR_NOP = 0,
    AMIVM_IR_MOVEQ,
    AMIVM_IR_CLR_L,
    AMIVM_IR_TST_L,
    AMIVM_IR_ADDQ_L,
    AMIVM_IR_SUBQ_L,
    AMIVM_IR_ADD_L,
    AMIVM_IR_SUB_L,
    AMIVM_IR_CMP_L,
    AMIVM_IR_AND_L,
    AMIVM_IR_OR_L,
    AMIVM_IR_EOR_L,
    AMIVM_IR_LOAD_L,
    AMIVM_IR_STORE_L,
    AMIVM_IR_BRANCH,
    AMIVM_IR_BRANCH_CC,
    AMIVM_IR_EXIT,
};

enum amivm_ir_condition {
    AMIVM_IR_CC_T = 0,
    AMIVM_IR_CC_HI = 2,
    AMIVM_IR_CC_LS = 3,
    AMIVM_IR_CC_CC = 4,
    AMIVM_IR_CC_CS = 5,
    AMIVM_IR_CC_NE = 6,
    AMIVM_IR_CC_EQ = 7,
    AMIVM_IR_CC_VC = 8,
    AMIVM_IR_CC_VS = 9,
    AMIVM_IR_CC_PL = 10,
    AMIVM_IR_CC_MI = 11,
    AMIVM_IR_CC_GE = 12,
    AMIVM_IR_CC_LT = 13,
    AMIVM_IR_CC_GT = 14,
    AMIVM_IR_CC_LE = 15,
};

enum amivm_ir_ea_mode {
    AMIVM_IR_EA_NONE = 0,
    AMIVM_IR_EA_AN = 2,
    AMIVM_IR_EA_POSTINC = 3,
    AMIVM_IR_EA_PREDEC = 4,
    AMIVM_IR_EA_D16_AN = 5,
};

struct amivm_ir_op {
    enum amivm_ir_opcode opcode;
    uint8_t reg;
    uint8_t src_reg;
    uint8_t condition;
    uint8_t ea_mode;
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
                     struct amivm_cpu_state *cpu, struct amivm_vm *vm);

#endif
