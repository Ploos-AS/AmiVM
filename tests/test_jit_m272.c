#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
int main(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 struct amivm_vm vm;struct amivm_jit_context ctx;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));memset(&vm,0,sizeof(vm));
 if(amivm_vm_init(&vm,1u)!=0)return 1;amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[7]=AMIVM_RAM_BASE+0x900u;cpu.a[3]=0x12345678u;
 b.guest_start_pc=0xb000u;b.guest_end_pc=0xb002u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_JSR;b.ops[0].ea_mode=AMIVM_IR_EA_AN;b.ops[0].reg=3u;
 b.ops[0].guest_pc=0xb000u;b.ops[0].instruction_bytes=2u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=0x12345678u||cpu.a[7]!=AMIVM_RAM_BASE+0x8fcu)return 1;
 memset(&b,0,sizeof(b));cpu.a[4]=0x87654321u;
 b.guest_start_pc=0xb100u;b.guest_end_pc=0xb102u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_JMP;b.ops[0].ea_mode=AMIVM_IR_EA_AN;b.ops[0].reg=4u;
 b.ops[0].guest_pc=0xb100u;b.ops[0].instruction_bytes=2u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=0x87654321u||cpu.a[7]!=AMIVM_RAM_BASE+0x8fcu)return 1;
 amivm_vm_destroy(&vm);
#endif
 puts("AmiVM M2.72 AArch64 JSR/JMP (An): PASS");return 0;
}
