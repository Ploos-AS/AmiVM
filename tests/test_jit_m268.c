#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int run(int32_t disp,uint32_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x7000u;b.guest_end_pc=0x7002u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_BRANCH;b.ops[0].guest_pc=0x7000u;b.ops[0].imm=disp;
 cpu.pc=0x7000u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.pc!=expected)return 1;
#else
 (void)disp;(void)expected;
#endif
 return 0;
}
int main(void)
{
 if(run(0x20,0x7022u))return 1;
 if(run(-0x12,0x6ff0u))return 1;
 puts("AmiVM M2.68 AArch64 BRA lowering: PASS");
 return 0;
}
