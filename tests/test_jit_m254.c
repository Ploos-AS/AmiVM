#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static uint32_t be32(struct amivm_vm *v,uint32_t a){uint8_t b[4];unsigned i;for(i=0;i<4u;i++)if(!amivm_read8(v,a+i,&b[i]))return 0xffffffffu;return((uint32_t)b[0]<<24u)|((uint32_t)b[1]<<16u)|((uint32_t)b[2]<<8u)|b[3];}
static int run(struct amivm_ir_block *b,struct amivm_cpu_state *c,struct amivm_vm *v){struct amivm_jit_code code;struct amivm_jit_runtime rt;struct amivm_jit_context ctx;CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK);CHECK(code.requires_context==1);amivm_jit_runtime_init(&rt);CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK);amivm_jit_context_init(&ctx,c,v);CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1);amivm_jit_runtime_release(&rt);return 0;}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x3000u;
const uint16_t jsr_an[]={0x4eb3u,0x2d20u,0x0020u};
const uint16_t jmp_pc[]={0x4efbu,0x9320u,0xfff0u};
const uint16_t jmp_bs[]={0x4ef4u,0x1990u};
const uint16_t jsr_is[]={0x4eb6u,0x0160u,0x0044u};
if(amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64){puts("AmiVM M2.54 tests: SKIP");return 0;}
#if !defined(__x86_64__) || !defined(__linux__)
puts("AmiVM M2.54 tests: SKIP");return 0;
#endif
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
cpu.a[3]=0x20000000u;cpu.d[2]=5u;amivm_ir_block_init(&b,0x4000u);CHECK(amivm_ir_decode_words(&b,jsr_an,3u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x20000034u);CHECK(be32(&vm,sp-4u)==0x4006u);
cpu.a[7]=sp;cpu.isp=sp;cpu.a[1]=3u;amivm_ir_block_init(&b,0x5000u);CHECK(amivm_ir_decode_words(&b,jmp_pc,3u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x4ff8u);CHECK(cpu.a[7]==sp);
cpu.d[1]=0x12345678u;amivm_ir_block_init(&b,0x6000u);CHECK(amivm_ir_decode_words(&b,jmp_bs,2u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x12345678u);
cpu.a[7]=sp;cpu.isp=sp;cpu.a[6]=0x30000000u;amivm_ir_block_init(&b,0x7000u);CHECK(amivm_ir_decode_words(&b,jsr_is,3u)==0);CHECK(run(&b,&cpu,&vm)==0);CHECK(cpu.pc==0x30000044u);CHECK(be32(&vm,sp-4u)==0x7006u);
amivm_vm_destroy(&vm);puts("AmiVM M2.54 native full indexed JSR/JMP tests: PASS");return 0;}
