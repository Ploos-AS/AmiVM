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
    const uint32_t block = AMIVM_ROM_BASE + 0x200u;
    size_t i;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], block);
    for (i = 0u; i < AMIVM_EXEC_DECODE_WORDS; ++i)
        put16_be(&vm.rom[0x200u + (i * 2u)], 0x4e71u);
    vm.rom_used = 0x200u + (AMIVM_EXEC_DECODE_WORDS * 2u);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);

    /* A full JIT-compatible cached block should execute natively on Linux/x86-64. */
    CHECK(amivm_exec_run(&exec, &cpu, &vm, AMIVM_EXEC_DECODE_WORDS) == 1);
    CHECK(exec.stats.instructions == AMIVM_EXEC_DECODE_WORDS);
    CHECK(cpu.pc == block + (AMIVM_EXEC_DECODE_WORDS * 2u));
#if defined(__x86_64__) && defined(__linux__)
    CHECK(exec.stats.jit_blocks == 1u);
    CHECK(exec.stats.jit_instructions == AMIVM_EXEC_DECODE_WORDS);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);
#else
    CHECK(exec.stats.jit_blocks == 0u);
    CHECK(exec.stats.ir_instructions == AMIVM_EXEC_DECODE_WORDS);
#endif

    /* Re-enter the same guest PC: the cached block must be reused. */
    cpu.pc = block;
    CHECK(amivm_exec_run(&exec, &cpu, &vm, AMIVM_EXEC_DECODE_WORDS) == 1);
    CHECK(exec.stats.instructions == AMIVM_EXEC_DECODE_WORDS * 2u);
    CHECK(exec.stats.cache_hits >= 1u);
#if defined(__x86_64__) && defined(__linux__)
    CHECK(exec.stats.jit_blocks == 2u);
    CHECK(exec.stats.jit_instructions == AMIVM_EXEC_DECODE_WORDS * 2u);
#endif

    /* A budget shorter than the cached block must remain exact and use IR. */
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_reset(&exec);
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 1u) == 1);
    CHECK(exec.stats.instructions == 1u);
    CHECK(exec.stats.ir_instructions == 1u);
    CHECK(exec.stats.jit_instructions == 0u);
    CHECK(cpu.pc == block + 2u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.26 JIT execution-cache integration tests: PASS");
    return 0;
}
