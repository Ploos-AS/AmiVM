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
    const uint32_t block_b = AMIVM_ROM_BASE + 0x220u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], block_a);

    /*
     * Two fully native conditional-branch blocks.  Each MOVEQ sets the
     * condition consumed by the following Bcc, so the selected successor
     * is dynamic but stable for this hot loop.
     */
    put16_be(&vm.rom[0x200], 0x7001u); /* MOVEQ #1,D0: Z=0 */
    put16_be(&vm.rom[0x202], 0x661cu); /* BNE block_b */
    put16_be(&vm.rom[0x220], 0x7000u); /* MOVEQ #0,D0: Z=1 */
    put16_be(&vm.rom[0x222], 0x67dcu); /* BEQ block_a */
    vm.rom_used = 0x224u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);

    CHECK(amivm_exec_run(&exec, &cpu, &vm, 400u) == 1);
    CHECK(cpu.pc == block_a);
    CHECK(cpu.d[0] == 0u);
    CHECK(exec.stats.instructions == 400u);
    CHECK(exec.stats.jit_instructions == 400u);
    CHECK(exec.stats.jit_blocks == 200u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);

    /* Only the first encounter of each block needs the generic dispatcher. */
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.dispatches == 2u);
    CHECK(exec.stats.jit_prepares == 2u);

    /* First A->B transition misses because B is not compiled yet. */
    CHECK(exec.stats.chain_misses == 1u);
    CHECK(exec.stats.jit_chain_misses == 1u);

    /* Every later transition is a fresh native-to-native cached successor. */
    CHECK(exec.stats.chain_hits == 198u);
    CHECK(exec.stats.jit_chain_hits == 198u);
    CHECK(exec.stats.cache_hits == 198u);

    amivm_exec_reset(&exec);
    CHECK(exec.stats.jit_chain_hits == 0u);
    CHECK(exec.stats.jit_chain_misses == 0u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.31 JIT branch chaining tests: PASS");
    return 0;
}
