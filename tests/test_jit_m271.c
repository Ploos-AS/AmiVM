#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b; struct amivm_jit_code j; struct amivm_cpu_state cpu;
 struct amivm_vm vm; struct amivm_jit_context ctx;
 memset(&b,0,sizeof(b)); memset(&cpu,0,sizeof(cpu)); memset(&vm,0,sizeof(vm));
 if(amivm_vm_init(&vm,1u)!=0)return 1;
 amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[7]=AMIVM_RAM_BASE+0x800u;
 b.guest_start_pc=0xa000u;b.guest_end_pc=0xa002u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=AMIVM_IR_BSR;b.ops[0].guest_pc=0xa000u;b.ops[0].imm=0x20;
 b.ops[0].instruction_bytes=2u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=0xa022u||cpu.a[7]!=AMIVM_RAM_BASE+0x7fcu)return 1;
 memset(&b,0,sizeof(b));b.guest_start_pc=0xa022u;b.guest_end_pc=0xa024u;
 b.op_count=1u;b.terminates=1;b.ops[0].opcode=AMIVM_IR_RTS;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=0xa002u||cpu.a[7]!=AMIVM_RAM_BASE+0x800u)return 1;
 amivm_vm_destroy(&vm);
#endif
 puts("AmiVM M2.71 AArch64 BSR/RTS lowering: PASS");
 return 0;
}
