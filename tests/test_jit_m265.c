#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define X 0x10u
#define N 0x08u
#define Z 0x04u
#define V 0x02u
#define C 0x01u
static int run(enum amivm_ir_opcode op,uint32_t in,int32_t q,uint32_t out,uint16_t flags)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b; struct amivm_jit_code j; struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b)); memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x4000u;b.guest_end_pc=0x4002u;b.op_count=1u;
 b.ops[0].opcode=op;b.ops[0].reg=1u;b.ops[0].imm=q;cpu.d[1]=in;cpu.sr=0xa500u|X|N|Z|V|C;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.d[1]!=out)return 1;
 if((cpu.sr&(X|N|Z|V|C))!=flags)return 1;
 if((cpu.sr&0xffe0u)!=(0xa500u&0xffe0u))return 1;
#else
 (void)op;(void)in;(void)q;(void)out;(void)flags;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_ADDQ_L,1u,3,4u,0u))return 1;
 if(run(AMIVM_IR_ADDQ_L,0xffffffffu,1,0u,X|Z|C))return 1;
 if(run(AMIVM_IR_ADDQ_L,0x7fffffffu,1,0x80000000u,N|V))return 1;
 if(run(AMIVM_IR_SUBQ_L,5u,3,2u,0u))return 1;
 if(run(AMIVM_IR_SUBQ_L,0u,1,0xffffffffu,X|N|C))return 1;
 if(run(AMIVM_IR_SUBQ_L,0x80000000u,1,0x7fffffffu,V))return 1;
 puts("AmiVM M2.65 AArch64 ADDQ.L/SUBQ.L lowering: PASS");
 return 0;
}
