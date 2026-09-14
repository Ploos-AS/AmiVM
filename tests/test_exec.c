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
    const uint32_t fallback_pc = AMIVM_ROM_BASE + 0x120u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x60feu); /* BRA -2: hot single-instruction loop. */
    put16_be(&vm.rom[0x120], 0x4280u); /* CLR.L D0: interpreter fallback. */
    put16_be(&vm.rom[0x122], 0x60feu);
    vm.rom_used = 0x124u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 1000u) == 1);
    CHECK(cpu.pc == loop_pc);
    CHECK(exec.stats.instructions == 1000u);
    CHECK(exec.stats.cache_misses == 1u);
    CHECK(exec.stats.cache_hits == 999u);
    CHECK(exec.stats.stale_write_misses == 0u);
    CHECK(exec.stats.ir_blocks == 1000u);
    CHECK(exec.stats.ir_instructions == 1000u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.exits == 0u);

    amivm_exec_invalidate_all(&exec);
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(exec.stats.instructions == 1001u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.cache_hits == 999u);

    cpu.pc = fallback_pc;
    cpu.d[0] = 0xffffffffu;
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(cpu.d[0] == 0u);
    CHECK(cpu.pc == fallback_pc + 2u);
    CHECK(exec.stats.fallbacks == 1u);
    CHECK(exec.stats.instructions == 1002u);

    /* The following supported branch compiles into a separate cached IR block. */
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(cpu.pc == fallback_pc + 2u);
    CHECK(exec.stats.ir_blocks == 1002u);
    CHECK(exec.stats.fallbacks == 1u);

    amivm_exec_reset(&exec);
    CHECK(exec.backend == backend);
    CHECK(exec.stats.instructions == 0u);
    CHECK(exec.stats.cache_hits == 0u);
    CHECK(exec.stats.cache_misses == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.fallbacks == 0u);

    /* M2.13: translate instruction fetch through the supervisor page tables. */
    {
        const uint32_t srp = AMIVM_RAM_BASE + 0x4000u;
        const uint32_t sl2 = AMIVM_RAM_BASE + 0x5000u;
        const uint32_t code_page = AMIVM_RAM_BASE + 0x9000u;
        const uint32_t logical_pc = 0x00400100u;
        const size_t code_offset = 0x9000u + 0x100u;
        uint64_t generation_before_write;

        put32_be(&vm.ram[0x4000u + 4u], sl2 | 1u); /* L1[1] -> L2. */
        put32_be(&vm.ram[0x5000u], code_page | 1u); /* L2[0] -> code page. */
        put16_be(&vm.ram[code_offset], 0x60feu);    /* BRA -2. */
        put16_be(&vm.ram[code_offset + 2u], 0x60feu);

        CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
        cpu.srp = srp;
        cpu.tc = 0x80000000u;
        cpu.pc = logical_pc;
        amivm_exec_reset(&exec);

        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(cpu.pc == logical_pc);
        CHECK(exec.stats.ir_blocks == 1u);
        CHECK(exec.stats.fallbacks == 0u);
        CHECK(exec.stats.cache_misses == 1u);

        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.cache_hits == 1u);
        CHECK(exec.stats.ir_blocks == 2u);

        generation_before_write = vm.memory_write_generation;
        CHECK(amivm_write8(&vm, code_page + 0x100u, 0x70u));
        CHECK(amivm_write8(&vm, code_page + 0x101u, 0x07u)); /* MOVEQ #7,D0. */
        CHECK(vm.memory_write_generation > generation_before_write);

        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.stale_write_misses == 1u);
        CHECK(exec.stats.cache_misses == 2u);
        CHECK(exec.stats.fallbacks == 0u);
        CHECK(cpu.d[0] == 7u);
        CHECK(cpu.pc == logical_pc + 2u);
    }

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.13 MMU-aware IR/write-invalidation tests: PASS");
    return 0;
}
