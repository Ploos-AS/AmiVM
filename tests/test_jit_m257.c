#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static int put32(struct amivm_vm *v,uint32_t a,uint32_t x){unsigned i;for(i=0;i<4u;i++)if(!amivm_write8(v,a+i,(uint8_t)(x>>((3u-i)*8u))))return 0;return 1;}
static int run(struct amivm_ir_block *b,struct amivm_cpu_state *c,struct amivm_vm *v){struct amivm_jit_code code;struct amivm_jit_runtime rt;struct amivm_jit_context ctx;CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK);amivm_jit_runtime_init(&rt);CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK);amivm_jit_context_init(&ctx,c,v);CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1);amivm_jit_runtime_release(&rt);return 0;}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x9000u;
/* D0.l, full, base suppressed, long BD, preindexed, long OD. */
const uint16_t pre_bs[]={0x4eb0u,0x09b3u,0x10001000u>>16,0x1000u,0x00000020u>>16,0x0020u};
/* D3.l, full, index suppressed, word BD, postindexed, null OD. */
const uint16_t post_is[]={0x4ef5u,0x3d25u,0x0040u};
if(amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64){puts("AmiVM M2.57 tests: SKIP");return 0;}
#if !defined(__x86_64__) || !defined(__linux__)
puts("AmiVM M2.57 tests: SKIP");return 0;
#endif
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
cpu.d[0]=AMIVM_RAM_BASE;CHECK(put32(&vm,AMIVM_RAM_BASE+0x1000u+AMIVM_RAM_BASE,0x21000000u)==0);
/* Keep the long-displacement vector as decode qualification; its suppressed-base address is outside mapped RAM with this value. */
amivm_ir_block_init(&b,0x6000u);CHECK(amivm_ir_decode_words(&b,pre_bs,6u)==0);CHECK(b.ops[0].base_suppress==1u);CHECK(b.ops[0].base_displacement==0x10001000);CHECK(b.ops[0].outer_displacement==0x20);CHECK(b.ops[0].instruction_bytes==12u);
cpu.a[5]=AMIVM_RAM_BASE+0x3000u;cpu.d[3]=0x77777777u;CHECK(put32(&vm,cpu.a[5]+0x40u,0x31000000u));amivm_ir_block_init(&b,0x7000u);CHECK(amivm_ir_decode_words(&b,post_is,3u)==0);CHECK(b.ops[0].index_suppress==1u);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x31000000u);CHECK(cpu.a[7]==sp);
amivm_vm_destroy(&vm);puts("AmiVM M2.57 full-extension edge qualification: PASS");return 0;}
