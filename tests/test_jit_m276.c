#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
static int one(enum amivm_ir_opcode opc,uint8_t mode,uint32_t pc,uint32_t base,uint32_t expect){
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_cpu_state cpu;struct amivm_vm vm;struct amivm_jit_context x;uint32_t sp=AMIVM_RAM_BASE+0xd00u;
 memset(&b,0,sizeof b);memset(&cpu,0,sizeof cpu);memset(&vm,0,sizeof vm);if(amivm_vm_init(&vm,1u))return 1;amivm_jit_context_init(&x,&cpu,&vm);
 cpu.a[7]=sp;cpu.a[2]=base;cpu.d[3]=4u;b.guest_start_pc=pc;b.guest_end_pc=pc+6u;b.op_count=1;b.terminates=1;
 b.ops[0].opcode=opc;b.ops[0].ea_mode=mode;b.ops[0].reg=2;b.ops[0].index_reg=3;b.ops[0].index_long=1;b.ops[0].index_scale=2;
 b.ops[0].full_format=1;b.ops[0].base_displacement=0x20;b.ops[0].instruction_bytes=6;b.ops[0].guest_pc=pc;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||!j.requires_context)return 1;if(amivm_jit_execute_context(&j,&cpu,&x))return 1;
 if(cpu.pc!=expect||cpu.a[7]!=(opc==AMIVM_IR_JSR?sp-4:sp))return 1;amivm_vm_destroy(&vm);
#else
 (void)opc;(void)mode;(void)pc;(void)base;(void)expect;
#endif
 return 0;}
int main(void){if(one(AMIVM_IR_JSR,AMIVM_IR_EA_D8_AN_XN,0xf000,0x10000,0x10028))return 1;if(one(AMIVM_IR_JMP,AMIVM_IR_EA_PC_D8_XN,0xf100,0,0xf12a))return 1;puts("AmiVM M2.76 AArch64 full indexed control flow: PASS");return 0;}
