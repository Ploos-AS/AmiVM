#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define C(x) do{if(!(x))return 1;}while(0)
static int put32(struct amivm_vm*v,uint32_t a,uint32_t x){unsigned i;for(i=0;i<4;i++)if(!amivm_write8(v,a+i,(uint8_t)(x>>((3-i)*8))))return 0;return 1;}
int main(void){
#if defined(__aarch64__) || defined(_M_ARM64)
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;struct amivm_jit_code j;struct amivm_jit_context x;uint32_t sp=AMIVM_RAM_BASE+0xe00u,table;
 const uint16_t jsr[]={0x4eb3u,0x2922u,0x0020u,0x0010u};
 amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;C(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof cpu);cpu.a[7]=sp;cpu.isp=sp;cpu.a[3]=AMIVM_RAM_BASE+0x1000u;cpu.d[2]=4u;
 table=cpu.a[3]+0x24u;C(put32(&vm,table,0x20000000u));amivm_ir_block_init(&b,0x4000u);C(amivm_ir_decode_words(&b,jsr,4u)==0);C(b.ops[0].full_format&&b.ops[0].indirect_mode!=AMIVM_EA_INDIRECT_NONE);
 C(amivm_jit_compile(&b,&j)==AMIVM_JIT_OK&&j.requires_context);amivm_jit_context_init(&x,&cpu,&vm);C(amivm_jit_execute_context(&j,&cpu,&x)==0);C(cpu.pc==0x20000010u&&cpu.a[7]==sp-4u);amivm_vm_destroy(&vm);
#endif
 puts("AmiVM M2.77 AArch64 memory-indirect full indexed control flow: PASS");return 0;}
