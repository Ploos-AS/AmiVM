#include "cpu.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

int main(void)
{
    struct amivm_config cfg;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const struct amivm_cpu_profile *p20 = amivm_cpu_profile_by_name("68020");
    const struct amivm_cpu_profile *p40 = amivm_cpu_profile_by_name("68040");
    uint32_t physical = 0u;

    amivm_config_init(&cfg);
    cfg.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &cfg) == 0);
    memset(&cpu, 0, sizeof cpu);

    put32(&vm.rom[0], AMIVM_RAM_BASE + 0x1000u);
    put32(&vm.rom[4], AMIVM_ROM_BASE + 0x100u);
    put32(&vm.rom[AMIVM_VECTOR_ILLEGAL_INSTRUCTION * 4u], AMIVM_ROM_BASE + 0x200u);
    put16(&vm.rom[0x100u], 0xf200u);
    put16(&vm.rom[0x102u], 0x0000u);
    put16(&vm.rom[0x200u], 0x4e71u);
    vm.rom_used = 0x202u;

    amivm_cpu_set_profile(&cpu, p20);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(amivm_cpu_get_profile(&cpu) == p20);

    cpu.tc = 0x80000000u;
    cpu.urp = 0u;
    CHECK(amivm_mmu_translate(&cpu, &vm, 0x12345000u, false, false, &physical) == AMIVM_MMU_OK);
    CHECK(physical == 0x12345000u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_ILLEGAL);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_ILLEGAL_INSTRUCTION);

    memset(&cpu, 0, sizeof cpu);
    amivm_cpu_set_profile(&cpu, p40);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(amivm_cpu_get_profile(&cpu) == p40);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.82 CPU capability enforcement: PASS");
    return 0;
}
