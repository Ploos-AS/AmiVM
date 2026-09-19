#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
static int run(enum amivm_ir_opcode op,uint8_t mode,int32_t target,uint32_t pc,uint8_t bytes,uint32_t expect_sp)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 struct amivm_vm vm;struct amivm_jit_context ctx;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));memset(&vm,0,sizeof(vm));
 if(amivm_vm_init(&vm,1u)!=0)return 1;amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[7]=AMIVM_RAM_BASE+0xa00u;
 b.guest_start_pc=pc;b.guest_end_pc=pc+bytes;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=op;b.ops[0].ea_mode=mode;b.ops[0].imm=target;
 b.ops[0].guest_pc=pc;b.ops[0].instruction_bytes=bytes;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=(uint32_t)target||cpu.a[7]!=expect_sp)return 1;
 amivm_vm_destroy(&vm);
#else
 (void)op;(void)mode;(void)target;(void)pc;(void)bytes;(void)expect_sp;
#endif
 return 0;
}
int main(void)
{
 uint32_t sp=AMIVM_RAM_BASE+0xa00u;
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_ABS_W,0x1234,0xc000u,4u,sp-4u))return 1;
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_ABS_L,0x12345678,0xc100u,6u,sp-4u))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_ABS_W,0x2345,0xc200u,4u,sp))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_ABS_L,0x23456789,0xc300u,6u,sp))return 1;
 puts("AmiVM M2.73 AArch64 absolute JSR/JMP: PASS");return 0;
}
