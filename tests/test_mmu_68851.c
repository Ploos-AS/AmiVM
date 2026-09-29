#include "cpu.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

int main(void)
{
    struct amivm_config cfg;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_cpu_profile p;
    const uint32_t root=AMIVM_RAM_BASE+0x2000u;
    const uint32_t supervisor_root=AMIVM_RAM_BASE+0x4000u;
    const uint32_t supervisor_l2=AMIVM_RAM_BASE+0x5000u;
    const uint32_t supervisor_page=AMIVM_RAM_BASE+0x9000u;
    const uint32_t l2=AMIVM_RAM_BASE+0x3000u;
    const uint32_t page=AMIVM_RAM_BASE+0x8000u;
    const uint32_t logical=0x00401234u;
    uint32_t physical=0u;

    amivm_config_init(&cfg);
    cfg.ram_size=1024u*1024u;
    CHECK(amivm_vm_init(&vm,&cfg)==0);
    CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));

    memset(&cpu,0,sizeof cpu);
    amivm_cpu_set_profile(&cpu,&p);
    cpu.tc=0u; /* 040/060 TC must not control the external PMMU. */
    cpu.urp=0u;
    cpu.srp=0u;
    cpu.pmmu_tc=0x80000000u;
    cpu.pmmu_crp=root;
    cpu.pmmu_srp=supervisor_root;
    put32(&vm.ram[0x2000u+4u],l2|2u);
    put32(&vm.ram[0x3000u+4u],page|1u);
    put32(&vm.ram[0x4000u+4u],supervisor_l2|2u);
    put32(&vm.ram[0x5000u+4u],supervisor_page|1u);

    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,false,&physical)==AMIVM_MMU_OK);
    CHECK(physical==page+0x234u);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_OK);
    CHECK(cpu.mmusr==0u);

    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,true,&physical)==AMIVM_MMU_OK);
    CHECK(physical==supervisor_page+0x234u);

    put32(&vm.ram[0x3000u+4u],page|5u);
    CHECK(amivm_mmu_translate(&cpu,&vm,logical,true,false,&physical)==AMIVM_MMU_FAULT_WRITE_PROTECT);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_WRITE_PROTECT);
    CHECK(cpu.mmusr==0u);

    /* A table descriptor is not a terminal page descriptor. */
    put32(&vm.ram[0x3000u+4u],page|2u);
    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,false,&physical)==AMIVM_MMU_FAULT_PAGE);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_PAGE);

    put32(&vm.ram[0x3000u+4u],0u);
    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,false,&physical)==AMIVM_MMU_FAULT_PAGE);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_PAGE);
    CHECK(cpu.mmusr==0u);

    cpu.pmmu_crp=0u;
    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,false,&physical)==AMIVM_MMU_FAULT_ROOT);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_ROOT);
    CHECK(cpu.mmusr==0u);

    cpu.pmmu_tc=0u;
    CHECK(amivm_mmu_translate(&cpu,&vm,logical,false,false,&physical)==AMIVM_MMU_OK);
    CHECK(physical==logical);
    CHECK(cpu.pmmu_psr==AMIVM_PMMU51_PSR_OK);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.90 68851 PMMU status: PASS");
    return 0;
}
