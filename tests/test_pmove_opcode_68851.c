#include "cpu.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while(0)
static void p16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void p32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}

int main(void)
{
    struct amivm_config cfg; struct amivm_vm vm; struct amivm_cpu_state cpu;
    struct amivm_cpu_profile p; const struct amivm_cpu_backend *b=amivm_cpu_reference_backend();
    const uint32_t sp=AMIVM_RAM_BASE+0x1000u, pc=AMIVM_ROM_BASE+0x100u;
    amivm_config_init(&cfg); cfg.ram_size=1024u*1024u; CHECK(amivm_vm_init(&vm,&cfg)==0);
    p32(&vm.rom[0],sp); p32(&vm.rom[4],pc);
    p32(&vm.rom[AMIVM_VECTOR_PRIVILEGE_VIOLATION*4u],AMIVM_ROM_BASE+0x200u);
    p32(&vm.rom[AMIVM_VECTOR_ILLEGAL_INSTRUCTION*4u],AMIVM_ROM_BASE+0x220u);
    p16(&vm.rom[0x100],0xf000u); p16(&vm.rom[0x102],0x4000u); /* PMOVE D0,TC */
    p16(&vm.rom[0x104],0xf001u); p16(&vm.rom[0x106],0x4200u); /* PMOVE TC,D1 */
    vm.rom_used=0x224u;

    memset(&cpu,0,sizeof cpu); amivm_cpu_set_profile(&cpu,&p);
    CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));
    CHECK(amivm_cpu_reset(&cpu,&vm,b)==0); amivm_cpu_set_profile(&cpu,&p);
    cpu.d[0]=0x80000000u;
    CHECK(amivm_cpu_step(&cpu,&vm,b)==1); CHECK(cpu.pmmu_tc==0x80000000u);
    CHECK(amivm_cpu_step(&cpu,&vm,b)==1); CHECK(cpu.d[1]==0x80000000u);

    CHECK(amivm_cpu_reset(&cpu,&vm,b)==0); amivm_cpu_set_profile(&cpu,&p);
    cpu.sr=0u; cpu.pc=pc; CHECK(amivm_cpu_step(&cpu,&vm,b)==2);
    CHECK(cpu.last_exception_vector==AMIVM_VECTOR_PRIVILEGE_VIOLATION);

    CHECK(amivm_cpu_reset(&cpu,&vm,b)==0);
    amivm_cpu_set_profile(&cpu,amivm_cpu_profile_by_name("68040")); cpu.pc=pc;
    CHECK(amivm_cpu_step(&cpu,&vm,b)==2); CHECK(cpu.last_exception_vector==AMIVM_VECTOR_ILLEGAL_INSTRUCTION);

    amivm_vm_destroy(&vm); puts("AmiVM M2.92 68851 PMOVE opcode: PASS"); return 0;
}
