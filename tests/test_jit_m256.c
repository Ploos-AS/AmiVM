#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static int put32(struct amivm_vm *v,uint32_t a,uint32_t x){unsigned i;for(i=0;i<4u;i++)if(!amivm_write8(v,a+i,(uint8_t)(x>>((3u-i)*8u))))return 0;return 1;}
static uint32_t get32(struct amivm_vm *v,uint32_t a){uint8_t b[4];unsigned i;for(i=0;i<4u;i++)if(!amivm_read8(v,a+i,&b[i]))return 0xffffffffu;return((uint32_t)b[0]<<24u)|((uint32_t)b[1]<<16u)|((uint32_t)b[2]<<8u)|b[3];}
static int run(struct amivm_ir_block *b,struct amivm_cpu_state *c,struct amivm_vm *v){struct amivm_jit_code code;struct amivm_jit_runtime rt;struct amivm_jit_context ctx;CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK);CHECK(code.requires_context==1);amivm_jit_runtime_init(&rt);CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK);amivm_jit_context_init(&ctx,c,v);CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1);amivm_jit_runtime_release(&rt);return 0;}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x7000u,table;
const uint16_t jsr_pre[]={0x4eb3u,0x2922u,0x0020u,0x0010u};
const uint16_t jmp_post[]={0x4ef4u,0x1926u,0x0030u,0xfff0u};
if(amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64){puts("AmiVM M2.56 tests: SKIP");return 0;}
#if !defined(__x86_64__) || !defined(__linux__)
puts("AmiVM M2.56 tests: SKIP");return 0;
#endif
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
cpu.a[3]=AMIVM_RAM_BASE+0x1000u;cpu.d[2]=4u;table=cpu.a[3]+0x24u;CHECK(put32(&vm,table,0x20000000u));amivm_ir_block_init(&b,0x4000u);CHECK(amivm_ir_decode_words(&b,jsr_pre,4u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x20000010u);CHECK(get32(&vm,sp-4u)==0x4008u);
cpu.a[7]=sp;cpu.isp=sp;cpu.a[4]=AMIVM_RAM_BASE+0x2000u;cpu.d[1]=8u;table=cpu.a[4]+0x30u;CHECK(put32(&vm,table,0x30000000u));amivm_ir_block_init(&b,0x5000u);CHECK(amivm_ir_decode_words(&b,jmp_post,4u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x2ffffff8u);CHECK(cpu.a[7]==sp);
amivm_vm_destroy(&vm);puts("AmiVM M2.56 native memory-indirect indexed control-flow tests: PASS");return 0;}
