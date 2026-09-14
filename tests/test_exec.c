#include "exec.h"
#include "vm.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static void put16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

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
    struct amivm_exec_engine exec;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t loop_pc = AMIVM_ROM_BASE + 0x100u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x60feu); /* BRA -2: hot single-instruction loop. */
    vm.rom_used = 0x102u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 1000u) == 1);
    CHECK(cpu.pc == loop_pc);
    CHECK(exec.stats.instructions == 1000u);
    CHECK(exec.stats.cache_misses == 1u);
    CHECK(exec.stats.cache_hits == 999u);
    CHECK(exec.stats.exits == 0u);

    amivm_exec_invalidate_all(&exec);
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(exec.stats.instructions == 1001u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.cache_hits == 999u);

    amivm_exec_reset(&exec);
    CHECK(exec.backend == backend);
    CHECK(exec.stats.instructions == 0u);
    CHECK(exec.stats.cache_hits == 0u);
    CHECK(exec.stats.cache_misses == 0u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.10 execution-core tests: PASS");
    return 0;
}
