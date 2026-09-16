#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while(0)
static uint32_t be32(struct amivm_vm *v,uint32_t a){uint8_t b[4];unsigned i;for(i=0;i<4;i++)if(!amivm_read8(v,a+i,&b[i]))return 0xffffffffu;return((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];}
static int run(struct amivm_ir_block *b,struct amivm_cpu_state *c,struct amivm_vm *v){struct amivm_jit_code code;struct amivm_jit_runtime rt;struct amivm_jit_context ctx;CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK);CHECK(code.requires_context==1);amivm_jit_runtime_init(&rt);CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK);amivm_jit_context_init(&ctx,c,v);CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1);amivm_jit_runtime_release(&rt);return 0;}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x3000u;const uint16_t jw[]={0x4eb8u,0xff00u},jl[]={0x4eb9u,0x1234u,0x5678u},mw[]={0x4ef8u,0x1234u},ml[]={0x4ef9u,0x89abu,0xcdefu};
if(amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64){puts("AmiVM M2.48 tests: SKIP");return 0;}
#if !defined(__x86_64__) || !defined(__linux__)
puts("AmiVM M2.48 tests: SKIP");return 0;
#endif
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
amivm_ir_block_init(&b,0x4000u);CHECK(amivm_ir_decode_words(&b,jw,2u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0xffffff00u);CHECK(be32(&vm,sp-4u)==0x4004u);
cpu.a[7]=sp;cpu.isp=sp;amivm_ir_block_init(&b,0x5000u);CHECK(amivm_ir_decode_words(&b,jl,3u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x12345678u);CHECK(be32(&vm,sp-4u)==0x5006u);
amivm_ir_block_init(&b,0x6000u);CHECK(amivm_ir_decode_words(&b,mw,2u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x1234u);
amivm_ir_block_init(&b,0x7000u);CHECK(amivm_ir_decode_words(&b,ml,3u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x89abcdefu);
amivm_vm_destroy(&vm);puts("AmiVM M2.48 native absolute JSR/JMP tests: PASS");return 0;}
