#include "cpu.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
static void p16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void seed(struct amivm_cpu_state*c){memset(c->pmmu_atc,0,sizeof c->pmmu_atc);c->pmmu_atc[0].valid=true;c->pmmu_atc[0].function_code=1;c->pmmu_atc[1].valid=true;c->pmmu_atc[1].function_code=5;c->pmmu_atc[2].valid=true;c->pmmu_atc[2].function_code=9;}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_cpu_profile p;const struct amivm_cpu_backend*be=amivm_cpu_reference_backend();
 amivm_config_init(&cfg);cfg.ram_size=1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));memset(&cpu,0,sizeof cpu);amivm_cpu_set_profile(&cpu,&p);cpu.sr=0x2000;cpu.pc=AMIVM_RAM_BASE;
 seed(&cpu);p16(vm.ram,0xf000);p16(vm.ram+2,0x31f9);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid&&!cpu.pmmu_atc[2].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.d[3]=5;p16(vm.ram+2,0x31eb);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pmmu_atc[0].valid&&!cpu.pmmu_atc[1].valid&&cpu.pmmu_atc[2].valid);
 amivm_vm_destroy(&vm);puts("AmiVM M2.109 MC68851 PFLUSH FC,MASK decode: PASS");return 0;
}