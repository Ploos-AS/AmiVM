#include "cpu.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
static void p32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_cpu_profile p;uint32_t pa=0;
 const uint32_t root=AMIVM_RAM_BASE+0x2000u,l2=AMIVM_RAM_BASE+0x3000u,page=AMIVM_RAM_BASE+0x8000u,alt=AMIVM_RAM_BASE+0x9000u,la=0x00401234u;
 amivm_config_init(&cfg);cfg.ram_size=1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);
 CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));memset(&cpu,0,sizeof cpu);amivm_cpu_set_profile(&cpu,&p);
 cpu.pmmu_tc=0x80000000u;cpu.pmmu_crp=(UINT64_C(0x7fff0003)<<32)|root;
 p32(vm.ram+0x2008,0x7fff0202u);p32(vm.ram+0x200c,l2);p32(vm.ram+0x3004,page|1u);
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 p32(vm.ram+0x3004,alt|1u);
 CHECK(amivm_pmmu51_set_access_level(&cpu,7u));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,true,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 amivm_pmmu51_atc_flush(&cpu);CHECK(amivm_mmu_translate(&cpu,&vm,la,false,true,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_crp=(UINT64_C(0x7fff0203)<<32)|root;p32(vm.ram+0x2008,0x7fff0002u);p32(vm.ram+0x200c,l2);p32(vm.ram+0x3004,page|1u);CHECK(amivm_pmmu51_set_access_level(&cpu,0u));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);CHECK(cpu.pmmu_atc[0].shared_globally);p32(vm.ram+0x3004,alt|1u);CHECK(amivm_pmmu51_set_access_level(&cpu,7u));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,true,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_crp=(UINT64_C(0x7fff0003)<<32)|root;p32(vm.ram+0x2008,0x7fff0002u);p32(vm.ram+0x200c,l2);p32(vm.ram+0x3004,page|1u);CHECK(amivm_pmmu51_set_access_level(&cpu,0u));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);cpu.pmmu_atc[0].write_protected=true;CHECK(amivm_mmu_translate(&cpu,&vm,la,true,false,&pa)==AMIVM_MMU_FAULT_WRITE_PROTECT);CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_WRITE_PROTECT);
 cpu.pmmu_atc[0].write_protected=false;cpu.pmmu_atc[0].read_access_level=3u;cpu.pmmu_atc[0].access_level=0u;CHECK(amivm_pmmu51_set_access_level(&cpu,2u));CHECK(cpu.pmmu_atc[0].access_level!=amivm_pmmu51_get_access_level(&cpu));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_FAULT_READ_ACCESS);CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_READ_ACCESS);CHECK(amivm_pmmu51_set_access_level(&cpu,3u));CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);
 cpu.pmmu_atc[0].write_access_level=4u;CHECK(amivm_mmu_translate(&cpu,&vm,la,true,false,&pa)==AMIVM_MMU_FAULT_WRITE_ACCESS);CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_WRITE_ACCESS);
 cpu.pmmu_atc[0].write_access_level=0u;cpu.pmmu_atc[0].supervisor_only=true;CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_FAULT_SUPERVISOR);CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_SUPERVISOR);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_atc[0].valid=true;cpu.pmmu_atc[0].logical_page=la&0xfffff000u;cpu.pmmu_atc[0].physical_page=page;cpu.pmmu_atc[0].function_code=5u;cpu.pmmu_atc[0].supervisor=true;cpu.pmmu_atc[0].shared_globally=true;p32(vm.ram+0x3004,alt|1u);CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_crp=(UINT64_C(0x7fff0003)<<32)|root;cpu.pmmu_srp=(UINT64_C(0x7fff0003)<<32)|root;p32(vm.ram+0x3004,page|1u);
 CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,1u,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);CHECK(cpu.pmmu_atc[0].function_code==1u);
 p32(vm.ram+0x3004,alt|1u);CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,2u,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);CHECK(cpu.pmmu_atc[1].function_code==2u);
 p32(vm.ram+0x3004,page|1u);CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,5u,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);CHECK(cpu.pmmu_atc[2].function_code==5u);
 p32(vm.ram+0x3004,alt|1u);CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,6u,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);CHECK(cpu.pmmu_atc[3].function_code==6u);
 CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,1u,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,2u,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_tc=0x80000000u;cpu.pmmu_srp=(UINT64_C(0x7fff0003)<<32)|root;p32(vm.ram+0x3004,page|1u);p32(vm.ram+(page-AMIVM_RAM_BASE)+0x234u,0x4e710000u);cpu.pc=la;cpu.sr=0x2700u;
 CHECK(amivm_cpu_step(&cpu,&vm,amivm_cpu_reference_backend())==1);CHECK(cpu.pc==la+2u);CHECK(cpu.pmmu_atc[0].valid);CHECK(cpu.pmmu_atc[0].function_code==6u);
 p32(vm.ram+0x3004,alt|1u);CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,5u,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);CHECK(cpu.pmmu_atc[1].valid);CHECK(cpu.pmmu_atc[1].function_code==5u);
 CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,6u,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 amivm_pmmu51_atc_flush(&cpu);cpu.pmmu_tc=0x80000000u;cpu.pmmu_crp=(UINT64_C(0x7fff0003)<<32)|root;cpu.sr=0x0000u;cpu.pc=la;p32(vm.ram+0x3004,page|1u);p32(vm.ram+(page-AMIVM_RAM_BASE)+0x234u,0x4e710000u);
 CHECK(amivm_cpu_step(&cpu,&vm,amivm_cpu_reference_backend())==1);CHECK(cpu.pc==la+2u);CHECK(cpu.pmmu_atc[0].valid);CHECK(cpu.pmmu_atc[0].function_code==2u);
 p32(vm.ram+0x3004,alt|1u);CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,1u,&pa)==AMIVM_MMU_OK);CHECK(pa==alt+0x234u);CHECK(cpu.pmmu_atc[1].valid);CHECK(cpu.pmmu_atc[1].function_code==1u);
 CHECK(amivm_mmu_translate_fc(&cpu,&vm,la,false,2u,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 amivm_vm_destroy(&vm);puts("AmiVM M2.131 68851 user instruction/data FC execution: PASS");return 0;
}