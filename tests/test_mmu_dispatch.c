#include "cpu.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

static int check_profile(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                         const char *name)
{
    const uint32_t root=AMIVM_RAM_BASE+0x2000u;
    const uint32_t l2=AMIVM_RAM_BASE+0x3000u;
    const uint32_t page=AMIVM_RAM_BASE+0x8000u;
    const uint32_t logical=0x00401234u;
    uint32_t physical=0u;

    memset(cpu,0,sizeof *cpu);
    amivm_cpu_set_profile(cpu,amivm_cpu_profile_by_name(name));
    cpu->tc=0x80000000u;
    cpu->urp=root;
    put32(&vm->ram[0x2000u+4u],l2|1u);
    put32(&vm->ram[0x3000u+4u],page|1u);
    CHECK(amivm_mmu_translate(cpu,vm,logical,false,false,&physical)==AMIVM_MMU_OK);
    CHECK(physical==page+0x234u);
    return 0;
}

int main(void)
{
    struct amivm_config cfg;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    uint32_t physical=0u;

    amivm_config_init(&cfg);
    cfg.ram_size=1024u*1024u;
    CHECK(amivm_vm_init(&vm,&cfg)==0);

    memset(&cpu,0,sizeof cpu);
    amivm_cpu_set_profile(&cpu,amivm_cpu_profile_by_name("68020"));
    cpu.tc=0x80000000u;
    CHECK(amivm_mmu_translate(&cpu,&vm,0x12345000u,false,false,&physical)==AMIVM_MMU_OK);
    CHECK(physical==0x12345000u);

    CHECK(check_profile(&cpu,&vm,"68030")==0);
    CHECK(check_profile(&cpu,&vm,"68040")==0);
    CHECK(check_profile(&cpu,&vm,"68060")==0);
    CHECK(check_profile(&cpu,&vm,"hyper040")==0);
    CHECK(check_profile(&cpu,&vm,"hyper060")==0);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.84 MMU dispatch qualification: PASS");
    return 0;
}
