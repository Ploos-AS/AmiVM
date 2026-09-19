#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
int main(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    memset(&block, 0, sizeof(block));
    memset(&cpu, 0, sizeof(cpu));
    block.guest_start_pc = 0x2000u;
    block.guest_end_pc = 0x2002u;
    block.op_count = 1u;
    block.ops[0].opcode = AMIVM_IR_CLR_L;
    block.ops[0].reg = 5u;
    cpu.d[5] = 0xdeadbeefu;
    cpu.sr = SR_X | SR_N | SR_V | SR_C;
    if (amivm_jit_compile(&block, &code) != AMIVM_JIT_OK) return 1;
    if (amivm_jit_execute(&code, &cpu) != 1) return 1;
    if (cpu.d[5] != 0u) return 1;
    if ((cpu.sr & (SR_X|SR_N|SR_Z|SR_V|SR_C)) != (SR_X|SR_Z)) return 1;
#endif
    puts("AmiVM M2.63 AArch64 CLR.L lowering: PASS");
    return 0;
}
