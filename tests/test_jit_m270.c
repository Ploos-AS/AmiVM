#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define N 0x08u
#define Z 0x04u
#define V 0x02u
static int run(uint8_t cc,uint16_t sr,int taken)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x9000u;b.guest_end_pc=0x9002u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_BRANCH_CC;b.ops[0].guest_pc=0x9000u;
 b.ops[0].condition=cc;b.ops[0].imm=0x20;cpu.sr=sr;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.pc!=(taken?0x9022u:0x9002u))return 1;
#else
 (void)cc;(void)sr;(void)taken;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_CC_GE,0,1)||run(AMIVM_IR_CC_GE,N,0)||run(AMIVM_IR_CC_GE,N|V,1))return 1;
 if(run(AMIVM_IR_CC_LT,N,1)||run(AMIVM_IR_CC_LT,N|V,0))return 1;
 if(run(AMIVM_IR_CC_GT,0,1)||run(AMIVM_IR_CC_GT,Z,0)||run(AMIVM_IR_CC_GT,N,0))return 1;
 if(run(AMIVM_IR_CC_LE,Z,1)||run(AMIVM_IR_CC_LE,N,1)||run(AMIVM_IR_CC_LE,0,0))return 1;
 puts("AmiVM M2.70 AArch64 signed Bcc lowering: PASS");
 return 0;
}
