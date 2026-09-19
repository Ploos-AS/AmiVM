#include "jit.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define X 0x10u
#define N 0x08u
#define Z 0x04u
#define V 0x02u
#define C 0x01u
static int run(enum amivm_ir_opcode op,uint32_t d,uint32_t s,uint32_t out,uint16_t flags)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b; struct amivm_jit_code j; struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x5000u;b.guest_end_pc=0x5002u;b.op_count=1u;
 b.ops[0].opcode=op;b.ops[0].reg=2u;b.ops[0].src_reg=3u;
 cpu.d[2]=d;cpu.d[3]=s;cpu.sr=0xa500u|X|N|Z|V|C;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.d[2]!=out||cpu.d[3]!=s)return 1;
 if((cpu.sr&(X|N|Z|V|C))!=flags)return 1;
 if((cpu.sr&0xffe0u)!=(0xa500u&0xffe0u))return 1;
#else
 (void)op;(void)d;(void)s;(void)out;(void)flags;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_ADD_L,0xffffffffu,1u,0u,X|Z|C))return 1;
 if(run(AMIVM_IR_ADD_L,0x7fffffffu,1u,0x80000000u,N|V))return 1;
 if(run(AMIVM_IR_SUB_L,0u,1u,0xffffffffu,X|N|C))return 1;
 if(run(AMIVM_IR_SUB_L,5u,3u,2u,0u))return 1;
 if(run(AMIVM_IR_CMP_L,5u,5u,5u,X|Z))return 1;
 if(run(AMIVM_IR_CMP_L,0u,1u,0u,X|N|C))return 1;
 puts("AmiVM M2.66 AArch64 register arithmetic: PASS");
 return 0;
}
