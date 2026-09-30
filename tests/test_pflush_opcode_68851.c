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
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.sfc=1;p16(vm.ram+2,0x31e0);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid&&cpu.pmmu_atc[2].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.dfc=5;p16(vm.ram+2,0x31e1);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pmmu_atc[0].valid&&!cpu.pmmu_atc[1].valid&&cpu.pmmu_atc[2].valid);
 seed(&cpu);cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;amivm_pmmu51_atc_flush_fc_page(&cpu,1,15,0x1234);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=0x1234;cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;p16(vm.ram,0xf012);p16(vm.ram+2,0x39f1);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=0x0800;cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;p16(vm.ram,0xf02a);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x0834);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pc==AMIVM_RAM_BASE+6u);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;p16(vm.ram,0xf000);p16(vm.ram+2,0x33f1);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid&&cpu.pmmu_atc[2].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=0x0800;cpu.d[3]=0x0400;cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;p16(vm.ram,0xf032);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x3c10);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pc==AMIVM_RAM_BASE+6u);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=0x0800;cpu.d[3]=0x0200;cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;p16(vm.ram,0xf032);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x3b20);p16(vm.ram+6,0x0400);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pc==AMIVM_RAM_BASE+8u);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.d[3]=0x1000;cpu.pmmu_atc[0].logical_page=0x1000;cpu.pmmu_atc[1].logical_page=0x2000;cpu.pmmu_atc[1].function_code=1;p16(vm.ram,0xf032);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x3dc0);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(cpu.pc==AMIVM_RAM_BASE+6u);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=AMIVM_RAM_BASE+0x100;cpu.d[3]=0x20;cpu.pmmu_atc[0].logical_page=0x3000;cpu.pmmu_atc[1].logical_page=0x4000;cpu.pmmu_atc[1].function_code=1;vm.ram[0x120]=0x00;vm.ram[0x121]=0x00;vm.ram[0x122]=0x30;vm.ram[0x123]=0x00;p16(vm.ram,0xf032);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x3911);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 seed(&cpu);cpu.pc=AMIVM_RAM_BASE;cpu.a[2]=AMIVM_RAM_BASE+0x180;cpu.d[3]=0x1000;cpu.pmmu_atc[0].logical_page=0x5000;cpu.pmmu_atc[1].logical_page=0x6000;cpu.pmmu_atc[1].function_code=1;vm.ram[0x180]=0x00;vm.ram[0x181]=0x00;vm.ram[0x182]=0x40;vm.ram[0x183]=0x00;p16(vm.ram,0xf032);p16(vm.ram+2,0x39f1);p16(vm.ram+4,0x3915);p16(vm.ram+6,0x0000);CHECK(amivm_cpu_step(&cpu,&vm,be)==1);CHECK(!cpu.pmmu_atc[0].valid&&cpu.pmmu_atc[1].valid);
 amivm_vm_destroy(&vm);puts("AmiVM M2.115 MC68851 PFLUSH memory-indirect EA: PASS");return 0;
}