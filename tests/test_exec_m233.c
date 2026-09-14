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
    const uint32_t block_a = AMIVM_ROM_BASE + 0x200u;
    const uint32_t block_b = AMIVM_ROM_BASE + 0x280u;
    const uint32_t block_end = AMIVM_ROM_BASE + 0x300u;
    size_t i;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], block_a);

    /*
     * A and B each fill one complete 64-word decode window.  Neither block
     * terminates explicitly, so A falls through to B and B falls through to
     * block_end.  This isolates M2.33 fallthrough chaining from branch decode
     * semantics (the current IR still only accepts short BRA/Bcc forms).
     */
    for (i = 0u; i < 64u; ++i)
        put16_be(&vm.rom[0x200u + i * 2u], 0x4e71u);
    for (i = 0u; i < 64u; ++i)
        put16_be(&vm.rom[0x280u + i * 2u], 0x4e71u);
    vm.rom_used = 0x300u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);

    /* First pass compiles A and B.  A->B initially misses because B is new. */
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 128u) == 1);
    CHECK(cpu.pc == block_end);
    CHECK(exec.stats.instructions == 128u);
    CHECK(exec.stats.jit_instructions == 128u);
    CHECK(exec.stats.jit_blocks == 2u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.dispatches == 2u);
    CHECK(exec.stats.jit_prepares == 2u);
    CHECK(exec.stats.chain_misses == 1u);
    CHECK(exec.stats.jit_chain_misses == 1u);
    CHECK(exec.stats.chain_hits == 0u);
    CHECK(exec.stats.jit_chain_hits == 0u);

    /*
     * Re-enter A with the same execution cache.  A itself is a normal cache
     * hit; its fallthrough successor B must then be resolved through the
     * direct native-to-native chain fast path.
     */
    cpu.pc = block_a;
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 128u) == 1);
    CHECK(cpu.pc == block_end);
    CHECK(cpu.pc == block_b + 0x80u);
    CHECK(exec.stats.instructions == 256u);
    CHECK(exec.stats.jit_instructions == 256u);
    CHECK(exec.stats.jit_blocks == 4u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.dispatches == 3u);
    CHECK(exec.stats.jit_prepares == 2u);
    CHECK(exec.stats.chain_misses == 1u);
    CHECK(exec.stats.jit_chain_misses == 1u);
    CHECK(exec.stats.chain_hits == 1u);
    CHECK(exec.stats.jit_chain_hits == 1u);
    CHECK(exec.stats.cache_hits == 2u);

    amivm_exec_reset(&exec);
    amivm_vm_destroy(&vm);
    puts("AmiVM M2.33 JIT fallthrough chaining tests: PASS");
    return 0;
}
