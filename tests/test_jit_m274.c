#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
static int run(enum amivm_ir_opcode op,uint8_t mode,uint8_t reg,int32_t disp,uint32_t pc,uint32_t base,uint32_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 struct amivm_vm vm;struct amivm_jit_context ctx;uint32_t sp=AMIVM_RAM_BASE+0xb00u;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));memset(&vm,0,sizeof(vm));
 if(amivm_vm_init(&vm,1u)!=0)return 1;amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[7]=sp;if(reg<7u)cpu.a[reg]=base;
 b.guest_start_pc=pc;b.guest_end_pc=pc+4u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=op;b.ops[0].ea_mode=mode;b.ops[0].reg=reg;b.ops[0].imm=disp;
 b.ops[0].guest_pc=pc;b.ops[0].instruction_bytes=4u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=expected)return 1;
 if(cpu.a[7]!=(op==AMIVM_IR_JSR?sp-4u:sp))return 1;
 amivm_vm_destroy(&vm);
#else
 (void)op;(void)mode;(void)reg;(void)disp;(void)pc;(void)base;(void)expected;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_D16_AN,2,0x30,0xd000u,0x10000u,0x10030u))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_D16_AN,3,-0x20,0xd100u,0x20000u,0x1ffe0u))return 1;
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_PC_D16,0,0x40,0xd200u,0,0xd242u))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_PC_D16,0,-0x12,0xd300u,0,0xd2f0u))return 1;
 puts("AmiVM M2.74 AArch64 d16 control flow: PASS");return 0;
}
