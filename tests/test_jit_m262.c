#include "jit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

static int run_case(int32_t imm, uint16_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;

    memset(&block, 0, sizeof(block));
    memset(&cpu, 0, sizeof(cpu));
    block.guest_start_pc = 0x1000u;
    block.guest_end_pc = 0x1002u;
    block.op_count = 1u;
    block.ops[0].opcode = AMIVM_IR_MOVEQ;
    block.ops[0].reg = 2u;
    block.ops[0].imm = imm;
    cpu.sr = SR_X | SR_N | SR_Z | SR_V | SR_C;

    if (amivm_jit_compile(&block, &code) != AMIVM_JIT_OK) return 1;
    if (amivm_jit_execute(&code, &cpu) != 1) return 1;
    if (cpu.d[2] != (uint32_t)imm) return 1;
    if ((cpu.sr & (SR_X | SR_N | SR_Z | SR_V | SR_C)) != expected) return 1;
#else
    (void)imm;
    (void)expected;
#endif
    return 0;
}

int main(void)
{
    if (run_case(0, SR_X | SR_Z) != 0) return 1;
    if (run_case(-1, SR_X | SR_N) != 0) return 1;
    if (run_case(42, SR_X) != 0) return 1;
    puts("AmiVM M2.62 AArch64 MOVEQ condition codes: PASS");
    return 0;
}
