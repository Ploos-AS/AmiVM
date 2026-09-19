#include "jit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;

    memset(&block, 0, sizeof(block));
    block.guest_start_pc = 0x1000u;
    block.guest_end_pc = 0x1002u;
    block.op_count = 1u;
    block.ops[0].opcode = AMIVM_IR_MOVEQ;
    block.ops[0].reg = 3u;
    block.ops[0].imm = -42;

#if defined(__aarch64__) || defined(_M_ARM64)
    {
        struct amivm_cpu_state cpu;
        memset(&cpu, 0, sizeof(cpu));
        if (amivm_jit_compile(&block, &code) != AMIVM_JIT_OK) return 1;
        if (code.arch != AMIVM_JIT_ARCH_AARCH64) return 1;
        if (amivm_jit_execute(&code, &cpu) != 1) return 1;
        if (cpu.d[3] != (uint32_t)-42) return 1;
    }
#else
    amivm_jit_code_init(&code);
    if (code.arch == AMIVM_JIT_ARCH_AARCH64) return 1;
#endif

    puts("AmiVM M2.61 AArch64 MOVEQ lowering: PASS");
    return 0;
}
