#include "cpu.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
static void p32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_cpu_profile p;
 struct amivm_pmmu51_long_descriptor d;uint32_t pa=0;
 const uint32_t root=AMIVM_RAM_BASE+0x2000u,l2=AMIVM_RAM_BASE+0x3000u,page=AMIVM_RAM_BASE+0x8000u,la=0x00401234u;
 uint64_t ld=(UINT64_C(0x00010002)<<32)|l2;
 CHECK(amivm_pmmu51_decode_long_descriptor(ld,&d));CHECK(d.limit==1u);CHECK(!d.lower_limit);
 CHECK(d.type==AMIVM_PMMU51_ROOT_TABLE_SHORT);CHECK(d.table_address==l2);
 amivm_config_init(&cfg);cfg.ram_size=1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);
 CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));
 memset(&cpu,0,sizeof cpu);amivm_cpu_set_profile(&cpu,&p);cpu.pmmu_tc=0x80000000u;
 cpu.pmmu_crp=(UINT64_C(0x7fff0003)<<32)|root;
 p32(vm.ram+0x2008,(uint32_t)(ld>>32));p32(vm.ram+0x200c,(uint32_t)ld);
 p32(vm.ram+0x3004,page|1u);
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);CHECK(pa==page+0x234u);
 p32(vm.ram+0x2008,0x00010102u);
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_FAULT_SUPERVISOR);
 CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_SUPERVISOR);
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,true,&pa)==AMIVM_MMU_OK);
 CHECK(pa==page+0x234u);
 p32(vm.ram+0x2008,0x00014002u);
 CHECK(amivm_pmmu51_set_access_level(&cpu,1u));
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_FAULT_READ_ACCESS);
 CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_READ_ACCESS);
 CHECK(amivm_pmmu51_set_access_level(&cpu,2u));
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_OK);
 CHECK(pa==page+0x234u);
 p32(vm.ram+0x2008,0x00000002u);
 CHECK(amivm_mmu_translate(&cpu,&vm,la,false,false,&pa)==AMIVM_MMU_FAULT_LIMIT);
 CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_LIMIT);
 amivm_vm_destroy(&vm);puts("AmiVM M2.103 68851 read access levels: PASS");return 0;
}