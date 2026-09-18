#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static int put32(struct amivm_vm *v,uint32_t a,uint32_t x){unsigned i;for(i=0;i<4u;i++)if(!amivm_write8(v,a+i,(uint8_t)(x>>((3u-i)*8u))))return 0;return 1;}
static int run(struct amivm_ir_block *b,struct amivm_cpu_state *c,struct amivm_vm *v){struct amivm_jit_code code;struct amivm_jit_runtime rt;struct amivm_jit_context ctx;CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK);amivm_jit_runtime_init(&rt);CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK);amivm_jit_context_init(&ctx,c,v);CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1);amivm_jit_runtime_release(&rt);return 0;}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0xa000u,table;
/* PC base, D2.l, full, word BD, preindexed, word OD. */
const uint16_t pc_pre[]={0x4ebbu,0x2922u,0x0020u,0x0010u};
/* PC base, D1.l, full, word BD, postindexed, word OD. */
const uint16_t pc_post[]={0x4efbu,0x1926u,0x0030u,0xfff0u};
if(amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64){puts("AmiVM M2.58 tests: SKIP");return 0;}
#if !defined(__x86_64__) || !defined(__linux__)
puts("AmiVM M2.58 tests: SKIP");return 0;
#endif
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
cpu.d[2]=4u;table=0x8002u+0x20u+4u;/* PC base is extension-word address. */CHECK(put32(&vm,table,0x22000000u)==0);amivm_ir_block_init(&b,0x8000u);CHECK(amivm_ir_decode_words(&b,pc_pre,4u)==0);CHECK(b.ops[0].ea_mode==AMIVM_IR_EA_PC_D8_XN);CHECK(b.ops[0].indirect_mode==AMIVM_EA_INDIRECT_PREINDEXED);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x22000010u);CHECK(cpu.a[7]==sp-4u);
cpu.a[7]=sp;cpu.isp=sp;cpu.d[1]=8u;table=0x9002u+0x30u;CHECK(put32(&vm,table,0x32000000u)==0);amivm_ir_block_init(&b,0x9000u);CHECK(amivm_ir_decode_words(&b,pc_post,4u)==0);CHECK(b.ops[0].ea_mode==AMIVM_IR_EA_PC_D8_XN);CHECK(b.ops[0].indirect_mode==AMIVM_EA_INDIRECT_POSTINDEXED);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x31fffff8u);CHECK(cpu.a[7]==sp);
amivm_vm_destroy(&vm);puts("AmiVM M2.58 PC-relative memory-indirect JIT tests: PASS");return 0;}
