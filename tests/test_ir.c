#include "ir.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu;
    const uint16_t hot_block[] = {
        0x4e71u, /* NOP */
        0x7480u, /* MOVEQ #-128,D2 */
        0x6002u  /* BRA +2 */
    };
    const uint16_t loop_block[] = { 0x60feu }; /* BRA -2 */
    const uint16_t unsupported[] = { 0x4e75u }; /* RTS -> fallback exit */

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

    puts("AmiVM M2.11 IR foundation tests: PASS");
    return 0;
}
