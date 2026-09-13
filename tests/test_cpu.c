#include "cpu.h"
#include "vm.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static void put32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t initial_pc = AMIVM_ROM_BASE + 0x100u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], initial_pc);
    vm.rom[0x100] = 0x4eu;
    vm.rom[0x101] = 0x71u;
    vm.rom_used = 0x102u;

    CHECK(backend != NULL);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(cpu.pc == initial_pc);
    CHECK((cpu.sr & 0x2000u) != 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 2u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.1 CPU tests: PASS");
    return 0;
}
