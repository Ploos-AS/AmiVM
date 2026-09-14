#include "ir.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

static int branch_case(uint8_t condition, uint16_t sr, int taken)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu;
    uint16_t words[1];

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = sr;
    words[0] = (uint16_t)(0x6000u | ((uint16_t)condition << 8u) | 0x02u);
    amivm_ir_block_init(&block, 0x9000u + (uint32_t)condition * 0x10u);
    CHECK(amivm_ir_decode_words(&block, words, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_BRANCH_CC);
    CHECK(block.ops[0].condition == condition);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK(cpu.pc == block.guest_start_pc + (taken ? 4u : 2u));
    return 0;
}

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu;
    const uint16_t hot_block[] = {0x4e71u, 0x7480u, 0x6002u};
    const uint16_t loop_block[] = {0x60feu};
    const uint16_t unsupported[] = {0x4e75u};
    const uint16_t flags_block[] = {0x7000u, 0x6602u};
    const uint16_t arithmetic_block[] = {0x7001u, 0x5280u, 0x5380u, 0x4a80u, 0x6780u};
    const uint16_t clear_branch[] = {0x4280u, 0x6702u};
    const uint16_t negative_branch[] = {0x70ffu, 0x6b02u};
    const uint16_t overflow_add[] = {0x5280u};
    const uint16_t reg_alu[] = {
        0x7005u, /* MOVEQ #5,D0 */
        0x7203u, /* MOVEQ #3,D1 */
        0xd081u, /* ADD.L D1,D0 -> 8 */
        0x9081u, /* SUB.L D1,D0 -> 5 */
        0xb081u, /* CMP.L D1,D0 -> positive */
        0xc081u, /* AND.L D1,D0 -> 1 */
        0x8081u, /* OR.L D1,D0 -> 3 */
        0xb380u  /* EOR.L D1,D0 -> 0 */
    };

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, hot_block, 3u) == 0);
    CHECK(block.op_count == 3u);
    CHECK(block.terminates == 1);
    CHECK(block.guest_end_pc == 0x1006u);
    CHECK(block.ops[0].opcode == AMIVM_IR_NOP);
    CHECK(block.ops[1].opcode == AMIVM_IR_MOVEQ);
    CHECK(block.ops[1].reg == 2u);
    CHECK(block.ops[1].imm == -128);
    CHECK(block.ops[2].opcode == AMIVM_IR_BRANCH);
    CHECK(block.ops[2].imm == 2);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK(cpu.d[2] == 0xffffff80u);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK((cpu.sr & SR_Z) == 0u);
    CHECK(cpu.pc == 0x1008u);

    amivm_ir_block_init(&block, 0x2000u);
    CHECK(amivm_ir_decode_words(&block, loop_block, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK(cpu.pc == 0x2000u);

    amivm_ir_block_init(&block, 0x3000u);
    CHECK(amivm_ir_decode_words(&block, unsupported, 1u) == 1);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_EXIT);
    CHECK(amivm_ir_execute(&block, &cpu) == 2);
    CHECK(cpu.pc == 0x3000u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, flags_block, 2u) == 0);
    CHECK(block.ops[1].opcode == AMIVM_IR_BRANCH_CC);
    CHECK(block.ops[1].condition == AMIVM_IR_CC_NE);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK(cpu.pc == 0x4004u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, arithmetic_block, 5u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK(cpu.d[0] == 1u);
    CHECK((cpu.sr & (SR_N | SR_Z | SR_V | SR_C)) == 0u);
    CHECK(cpu.pc == 0x500au);

    cpu.sr = SR_X | SR_N | SR_V | SR_C;
    cpu.d[0] = 0xffffffffu;
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, clear_branch, 2u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK(cpu.d[0] == 0u);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK((cpu.sr & (SR_N | SR_V | SR_C)) == 0u);
    CHECK((cpu.sr & SR_X) != 0u);
    CHECK(cpu.pc == 0x6006u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, negative_branch, 2u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu) == 1);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK(cpu.pc == 0x7006u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.d[0] = 0x7fffffffu;
    amivm_ir_block_init(&block, 0x8000u);
    CHECK(amivm_ir_decode_words(&block, overflow_add, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu) == 0);
    CHECK(cpu.d[0] == 0x80000000u);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK((cpu.sr & SR_V) != 0u);
    CHECK((cpu.sr & SR_C) == 0u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_X;
    amivm_ir_block_init(&block, 0xa000u);
    CHECK(amivm_ir_decode_words(&block, reg_alu, sizeof(reg_alu) / sizeof(reg_alu[0])) == 0);
    CHECK(block.ops[2].opcode == AMIVM_IR_ADD_L);
    CHECK(block.ops[2].reg == 0u && block.ops[2].src_reg == 1u);
    CHECK(block.ops[7].opcode == AMIVM_IR_EOR_L);
    CHECK(amivm_ir_execute(&block, &cpu) == 0);
    CHECK(cpu.d[0] == 0u);
    CHECK(cpu.d[1] == 3u);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK((cpu.sr & (SR_N | SR_V | SR_C)) == 0u);
    CHECK((cpu.sr & SR_X) == 0u); /* ADD/SUB updated X before logic/CMP preserved it. */

    CHECK(branch_case(AMIVM_IR_CC_HI, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LS, SR_C, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_CC, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_CS, SR_C, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_NE, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_EQ, SR_Z, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_VC, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_VS, SR_V, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_PL, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_MI, SR_N, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GE, SR_N | SR_V, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LT, SR_N, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GT, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LE, SR_Z, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_HI, SR_Z, 0) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GT, SR_Z, 0) == 0);

    puts("AmiVM M2.17 full-Bcc/register-ALU tests: PASS");
    return 0;
}
