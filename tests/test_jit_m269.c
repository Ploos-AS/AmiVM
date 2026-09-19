#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define N 0x08u
#define Z 0x04u
#define V 0x02u
#define C 0x01u
static int run(uint8_t cc,uint16_t sr,uint32_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x8000u;b.guest_end_pc=0x8002u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_BRANCH_CC;b.ops[0].guest_pc=0x8000u;
 b.ops[0].condition=cc;b.ops[0].imm=0x20;cpu.sr=sr;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.pc!=expected)return 1;
#else
 (void)cc;(void)sr;(void)expected;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_CC_EQ,Z,0x8022u))return 1;
 if(run(AMIVM_IR_CC_EQ,0,0x8002u))return 1;
 if(run(AMIVM_IR_CC_NE,0,0x8022u))return 1;
 if(run(AMIVM_IR_CC_CS,C,0x8022u))return 1;
 if(run(AMIVM_IR_CC_CC,C,0x8002u))return 1;
 if(run(AMIVM_IR_CC_MI,N,0x8022u))return 1;
 if(run(AMIVM_IR_CC_VS,V,0x8022u))return 1;
 if(run(AMIVM_IR_CC_HI,0,0x8022u))return 1;
 if(run(AMIVM_IR_CC_LS,Z,0x8022u))return 1;
 puts("AmiVM M2.69 AArch64 basic Bcc lowering: PASS");
 return 0;
}
