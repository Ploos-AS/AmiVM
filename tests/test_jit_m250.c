#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)
static uint32_t be32(struct amivm_vm *v, uint32_t a) { uint8_t b[4]; unsigned i; for (i=0;i<4u;i++) if (!amivm_read8(v,a+i,&b[i])) return 0xffffffffu; return ((uint32_t)b[0]<<24u)|((uint32_t)b[1]<<16u)|((uint32_t)b[2]<<8u)|b[3]; }
static int run(struct amivm_ir_block *b, struct amivm_cpu_state *c, struct amivm_vm *v) { struct amivm_jit_code code; struct amivm_jit_runtime rt; struct amivm_jit_context ctx; CHECK(amivm_jit_compile(b,&code)==AMIVM_JIT_OK); CHECK(code.requires_context==1); amivm_jit_runtime_init(&rt); CHECK(amivm_jit_runtime_prepare(&rt,&code)==AMIVM_JIT_OK); amivm_jit_context_init(&ctx,c,v); CHECK(amivm_jit_runtime_execute_context(&rt,c,&ctx)==1); amivm_jit_runtime_release(&rt); return 0; }
int main(void) {
    struct amivm_config cfg; struct amivm_vm vm; struct amivm_cpu_state cpu; struct amivm_ir_block b;
    uint32_t sp=AMIVM_RAM_BASE+0x3000u;
    const uint16_t jsr_d16[]={0x4eabu,0xfff0u}, jmp_d16[]={0x4eedu,0x0120u};
    const uint16_t jsr_pc[]={0x4ebau,0x0030u}, jmp_pc[]={0x4efau,0xffe0u};
    if (amivm_jit_host_arch()!=AMIVM_JIT_ARCH_X86_64) { puts("AmiVM M2.50 tests: SKIP"); return 0; }
#if !defined(__x86_64__) || !defined(__linux__)
    puts("AmiVM M2.50 tests: SKIP"); return 0;
#endif
    amivm_config_init(&cfg); cfg.ram_size=2u*1024u*1024u; CHECK(amivm_vm_init(&vm,&cfg)==0);
    memset(&cpu,0,sizeof(cpu)); cpu.sr=0x2000u; cpu.a[7]=sp; cpu.isp=sp;
    cpu.a[3]=0x20001000u; amivm_ir_block_init(&b,0x4000u); CHECK(amivm_ir_decode_words(&b,jsr_d16,2u)==0); CHECK(run(&b,&cpu,&vm)==0); CHECK(cpu.pc==0x20000ff0u); CHECK(be32(&vm,sp-4u)==0x4004u);
    cpu.a[7]=sp; cpu.isp=sp; cpu.a[5]=0x30000000u; amivm_ir_block_init(&b,0x5000u); CHECK(amivm_ir_decode_words(&b,jmp_d16,2u)==0); CHECK(run(&b,&cpu,&vm)==0); CHECK(cpu.pc==0x30000120u); CHECK(cpu.a[7]==sp);
    amivm_ir_block_init(&b,0x6000u); CHECK(amivm_ir_decode_words(&b,jsr_pc,2u)==0); CHECK(run(&b,&cpu,&vm)==0); CHECK(cpu.pc==0x6032u); CHECK(be32(&vm,sp-4u)==0x6004u);
    cpu.a[7]=sp; cpu.isp=sp; amivm_ir_block_init(&b,0x7000u); CHECK(amivm_ir_decode_words(&b,jmp_pc,2u)==0); CHECK(run(&b,&cpu,&vm)==0); CHECK(cpu.pc==0x6fe2u); CHECK(cpu.a[7]==sp);
    amivm_vm_destroy(&vm); puts("AmiVM M2.50 native d16(An)/PC-relative JSR/JMP tests: PASS"); return 0;
}
