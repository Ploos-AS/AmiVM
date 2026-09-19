#include "jit.h"
#include <stdio.h>
#include <string.h>
int main(void){
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code c;struct amivm_jit_runtime r;struct amivm_cpu_state cpu;
 memset(&b,0,sizeof b);memset(&cpu,0,sizeof cpu);b.guest_start_pc=0x1000;b.guest_end_pc=0x1002;b.op_count=1;
 b.ops[0].opcode=AMIVM_IR_MOVEQ;b.ops[0].reg=0;b.ops[0].imm=42;b.ops[0].guest_pc=0x1000;
 if(amivm_jit_compile(&b,&c)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_execute(&c,&cpu)!=1||cpu.d[0]!=42u)return 1;
 memset(&cpu,0,sizeof cpu);amivm_jit_runtime_init(&r);if(amivm_jit_runtime_prepare(&r,&c)!=AMIVM_JIT_OK)return 1;
 if(amivm_jit_runtime_execute(&r,&cpu)!=1||cpu.d[0]!=42u)return 1;amivm_jit_runtime_release(&r);
#endif
 puts("AmiVM M2.78 AArch64 JIT cache coherency: PASS");return 0;}
