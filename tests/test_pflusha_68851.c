#include "cpu.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
static void p16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_cpu_profile p;
 const struct amivm_cpu_backend *be=amivm_cpu_reference_backend();
 amivm_config_init(&cfg);cfg.ram_size=1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);
 CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));
 memset(&cpu,0,sizeof cpu);amivm_cpu_set_profile(&cpu,&p);cpu.sr=0x2000u;cpu.pc=AMIVM_RAM_BASE;
 cpu.pmmu_atc[0].valid=true;cpu.pmmu_atc[0].logical_page=0x00401000u;cpu.pmmu_atc_next=1u;
 p16(vm.ram,0xf000u);p16(vm.ram+2,0x2400u);
 CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pc==AMIVM_RAM_BASE+4u);
 for(unsigned i=0;i<AMIVM_PMMU51_ATC_ENTRIES;i++) CHECK(!cpu.pmmu_atc[i].valid);
 CHECK(cpu.pmmu_atc_next==0u);
 amivm_vm_destroy(&vm);puts("AmiVM M2.107 MC68851 PFLUSHA: PASS");return 0;
}