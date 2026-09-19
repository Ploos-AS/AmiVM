#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
static int run_case(uint32_t value, uint16_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    memset(&block,0,sizeof(block)); memset(&cpu,0,sizeof(cpu));
    block.guest_start_pc=0x3000u; block.guest_end_pc=0x3002u; block.op_count=1u;
    block.ops[0].opcode=AMIVM_IR_TST_L; block.ops[0].reg=4u;
    cpu.d[4]=value; cpu.sr=SR_X|SR_N|SR_Z|SR_V|SR_C;
    if(amivm_jit_compile(&block,&code)!=AMIVM_JIT_OK) return 1;
    if(amivm_jit_execute(&code,&cpu)!=1) return 1;
    if(cpu.d[4]!=value) return 1;
    if((cpu.sr&(SR_X|SR_N|SR_Z|SR_V|SR_C))!=expected) return 1;
#else
    (void)value; (void)expected;
#endif
    return 0;
}
int main(void)
{
    if(run_case(0u,SR_X|SR_Z)) return 1;
    if(run_case(0x80000000u,SR_X|SR_N)) return 1;
    if(run_case(42u,SR_X)) return 1;
    puts("AmiVM M2.64 AArch64 TST.L lowering: PASS");
    return 0;
}
