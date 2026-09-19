#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
static int run(enum amivm_ir_opcode op,uint8_t mode,uint32_t pc,uint32_t base,int32_t idx,int8_t disp,uint32_t expected)
{
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;
 struct amivm_vm vm;struct amivm_jit_context ctx;uint32_t sp=AMIVM_RAM_BASE+0xc00u;
 memset(&b,0,sizeof(b));memset(&cpu,0,sizeof(cpu));memset(&vm,0,sizeof(vm));
 if(amivm_vm_init(&vm,1u)!=0)return 1;amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[7]=sp;cpu.a[2]=base;cpu.d[3]=(uint32_t)idx;
 b.guest_start_pc=pc;b.guest_end_pc=pc+4u;b.op_count=1u;b.terminates=1;
 b.ops[0].opcode=op;b.ops[0].ea_mode=mode;b.ops[0].reg=2u;b.ops[0].index_reg=3u;
 b.ops[0].index_long=1u;b.ops[0].index_scale=1u;b.ops[0].imm=disp;
 b.ops[0].guest_pc=pc;b.ops[0].instruction_bytes=4u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;
 if(amivm_jit_execute_context(&j,&cpu,&ctx)!=0)return 1;
 if(cpu.pc!=expected||cpu.a[7]!=(op==AMIVM_IR_JSR?sp-4u:sp))return 1;
 amivm_vm_destroy(&vm);
#else
 (void)op;(void)mode;(void)pc;(void)base;(void)idx;(void)disp;(void)expected;
#endif
 return 0;
}
int main(void)
{
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_D8_AN_XN,0xe000u,0x10000u,8,0x10,0x10020u))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_D8_AN_XN,0xe100u,0x20000u,-4,-8,0x1fff0u))return 1;
 if(run(AMIVM_IR_JSR,AMIVM_IR_EA_PC_D8_XN,0xe200u,0,8,0x10,0xe222u))return 1;
 if(run(AMIVM_IR_JMP,AMIVM_IR_EA_PC_D8_XN,0xe300u,0,-4,-8,0xe2f2u))return 1;
 puts("AmiVM M2.75 AArch64 brief indexed control flow: PASS");return 0;
}
