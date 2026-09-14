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

static size_t cache_index(uint32_t pc)
{
    return (size_t)((pc >> 1u) & (AMIVM_EXEC_CACHE_ENTRIES - 1u));
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_exec_cache_entry *source;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t block_a = AMIVM_ROM_BASE + 0x200u;
    const uint32_t block_c = AMIVM_ROM_BASE + 0x202u;
    const uint32_t block_b = AMIVM_ROM_BASE + 0x220u;
    int primary_is_b;
    int primary_is_c;
    int alt_is_b;
    int alt_is_c;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], block_a);

    /*
     * A alternates between its taken and fallthrough Bcc successors.
     * B sets Z=1 before returning to A, C sets Z=0 before returning to A.
     */
    put16_be(&vm.rom[0x200], 0x661eu); /* BNE.s block_b */
    put16_be(&vm.rom[0x202], 0x7001u); /* C: MOVEQ #1,D0 -> Z=0 */
    put16_be(&vm.rom[0x204], 0x60fau); /* BRA.s block_a */
    put16_be(&vm.rom[0x220], 0x7000u); /* B: MOVEQ #0,D0 -> Z=1 */
    put16_be(&vm.rom[0x222], 0x60dcu); /* BRA.s block_a */
    vm.rom_used = 0x224u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);

    /* 1 + 2 instructions per half-cycle; 300 is exactly 100 half-cycles. */
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 300u) == 1);
    CHECK(cpu.pc == block_a);
    CHECK(exec.stats.instructions == 300u);
    CHECK(exec.stats.jit_instructions == 300u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);

    source = &exec.cache[cache_index(block_a)];
    CHECK(source->valid);
    CHECK(source->pc == block_a);
    CHECK(source->jit_valid);
    CHECK(source->chain_valid);
    CHECK(source->chain_alt_valid);

    primary_is_b = source->chain_pc == block_b;
    primary_is_c = source->chain_pc == block_c;
    alt_is_b = source->chain_alt_pc == block_b;
    alt_is_c = source->chain_alt_pc == block_c;
    CHECK((primary_is_b && alt_is_c) || (primary_is_c && alt_is_b));

    CHECK(exec.cache[source->chain_index].pc == source->chain_pc);
    CHECK(exec.cache[source->chain_alt_index].pc == source->chain_alt_pc);
    CHECK(exec.stats.jit_chain_hits > 0u);

    amivm_exec_reset(&exec);
    amivm_vm_destroy(&vm);
    puts("AmiVM M2.34 dual conditional successor chain tests: PASS");
    return 0;
}
