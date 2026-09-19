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
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));
 b.guest_start_pc=0x6000u;b.guest_end_pc=0x6002u;b.op_count=1u;
 b.ops[0].opcode=op;b.ops[0].reg=6u;b.ops[0].src_reg=7u;
 cpu.d[6]=d;cpu.d[7]=s;cpu.sr=0xa500u|X|N|Z|V|C;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&j,&cpu)!=1)return 1;
 if(cpu.d[6]!=out||cpu.d[7]!=s)return 1;
 if((cpu.sr&(X|N|Z|V|C))!=flags)return 1;
 if((cpu.sr&0xffe0u)!=(0xa500u&0xffe0u))return 1;
#else
 (void)op;(void)d;(void)s;(void)out;(void)flags;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_AND_L,0xf0f0u,0x0f0fu,0u,X|Z))return 1;
 if(run(AMIVM_IR_OR_L,0x80000000u,1u,0x80000001u,X|N))return 1;
 if(run(AMIVM_IR_EOR_L,0xaaaaaaaau,0x55555555u,0xffffffffu,X|N))return 1;
 if(run(AMIVM_IR_EOR_L,0x12345678u,0x12345678u,0u,X|Z))return 1;
 puts("AmiVM M2.67 AArch64 logical register ops: PASS");
 return 0;
}
