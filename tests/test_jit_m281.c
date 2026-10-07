#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
int main(void){
#if defined(__x86_64__) && defined(__linux__)
 struct amivm_ir_block b={0}; struct amivm_jit_code j; struct amivm_cpu_state cpu={0}; struct amivm_vm vm={0}; struct amivm_jit_context ctx; struct amivm_config config;
 amivm_config_init(&config); config.ram_size=2u*1024u*1024u; if(amivm_vm_init(&vm,&config)!=0)return 1; amivm_jit_context_init(&ctx,&cpu,&vm);
 cpu.a[4]=0xb000u; cpu.d[2]=0x20u; cpu.pc=0xa000u; cpu.sr=0x2000u;
 amivm_write8(&vm,0xb030u,0); amivm_write8(&vm,0xb031u,0); amivm_write8(&vm,0xb032u,0xc0); amivm_write8(&vm,0xb033u,0);
 b.guest_start_pc=0xa000u;b.guest_end_pc=0xa006u;b.op_count=1;b.terminates=1;b.ops[0].opcode=AMIVM_IR_JMP;b.ops[0].ea_mode=AMIVM_IR_EA_D8_AN_XN;b.ops[0].reg=4u;b.ops[0].index_reg=2u;b.ops[0].index_long=1u;b.ops[0].full_format=1u;b.ops[0].indirect_mode=AMIVM_EA_INDIRECT_PREINDEXED;b.ops[0].base_displacement=0x10;b.ops[0].outer_displacement=0x20;b.ops[0].guest_pc=0xa000u;b.ops[0].instruction_bytes=6u;
 if(amivm_jit_compile(&b,&j)!=AMIVM_JIT_OK||amivm_jit_execute_context(&j,&cpu,&ctx)!=1||cpu.pc!=0xc020u)return 1; amivm_vm_destroy(&vm);
#endif
 puts("AmiVM M2.81 JMP full indexed preindexed: PASS"); return 0; }