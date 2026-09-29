#include "cpu.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
static void p16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void p32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint64_t g64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void p64(uint8_t*p,uint64_t v){for(int i=7;i>=0;i--){p[i]=(uint8_t)v;v>>=8;}}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_cpu_profile p;
 const struct amivm_cpu_backend*b=amivm_cpu_reference_backend();uint64_t v;
 const uint32_t pc=AMIVM_ROM_BASE+0x100u,sp=AMIVM_RAM_BASE+0x1000u,m=AMIVM_RAM_BASE+0x1800u;
 amivm_config_init(&cfg);cfg.ram_size=1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);
 p32(vm.rom,sp);p32(vm.rom+4,pc);
 p16(vm.rom+0x100,0xf010);p16(vm.rom+0x102,0x4c00); /* PMOVE (A0),CRP */
 p16(vm.rom+0x104,0xf011);p16(vm.rom+0x106,0x4e00); /* PMOVE CRP,(A1) */
 p16(vm.rom+0x108,0xf012);p16(vm.rom+0x10a,0x4800); /* PMOVE (A2),SRP */
 p16(vm.rom+0x10c,0xf013);p16(vm.rom+0x10e,0x4a00); /* PMOVE SRP,(A3) */
 vm.rom_used=0x110;
 p64(vm.ram+0x1800,UINT64_C(0x1234567810002000));
 p64(vm.ram+0x1810,UINT64_C(0xabcdef0110004000));
 CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));
 memset(&cpu,0,sizeof cpu);amivm_cpu_set_profile(&cpu,&p);CHECK(amivm_cpu_reset(&cpu,&vm,b)==0);amivm_cpu_set_profile(&cpu,&p);
 cpu.a[0]=m;cpu.a[1]=m+8;cpu.a[2]=m+0x10;cpu.a[3]=m+0x18;
 CHECK(amivm_cpu_step(&cpu,&vm,b)==1);CHECK(amivm_pmmu51_read_root(&cpu,AMIVM_PMMU51_REG_CRP,&v));CHECK(v==UINT64_C(0x1234567810002000));
 CHECK(amivm_cpu_step(&cpu,&vm,b)==1);CHECK(g64(vm.ram+0x1808)==v);
 CHECK(amivm_cpu_step(&cpu,&vm,b)==1);CHECK(amivm_pmmu51_read_root(&cpu,AMIVM_PMMU51_REG_SRP,&v));CHECK(v==UINT64_C(0xabcdef0110004000));
 CHECK(amivm_cpu_step(&cpu,&vm,b)==1);CHECK(g64(vm.ram+0x1818)==v);
 amivm_vm_destroy(&vm);puts("AmiVM M2.95 68851 root PMOVE: PASS");return 0;
}