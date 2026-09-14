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

static int run_budget(struct amivm_vm *vm,
                      const struct amivm_cpu_backend *backend,
                      uint64_t budget, uint32_t expected_pc,
                      uint32_t d0, uint32_t d1, uint32_t d2)
{
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;

    CHECK(amivm_cpu_reset(&cpu, vm, backend) == 0);
    amivm_exec_init(&exec, backend);
    CHECK(amivm_exec_run(&exec, &cpu, vm, budget) == 1);
    CHECK(exec.stats.instructions == budget);
    CHECK(exec.stats.ir_instructions + exec.stats.jit_instructions == budget);
    CHECK(cpu.pc == expected_pc);
    CHECK(cpu.d[0] == d0);
    CHECK(cpu.d[1] == d1);
    CHECK(cpu.d[2] == d2);
    CHECK(exec.stats.fallbacks == 0u);
    return 0;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t block_a = AMIVM_ROM_BASE + 0x200u;
    const uint32_t block_b = AMIVM_ROM_BASE + 0x220u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], block_a);

    put16_be(&vm.rom[0x200], 0x7001u); /* MOVEQ #1,D0 */
    put16_be(&vm.rom[0x202], 0x7202u); /* MOVEQ #2,D1 */
    put16_be(&vm.rom[0x204], 0x4e71u); /* NOP */
    put16_be(&vm.rom[0x206], 0x6018u); /* BRA block_b */
    put16_be(&vm.rom[0x220], 0x7403u); /* MOVEQ #3,D2 */
    put16_be(&vm.rom[0x222], 0x60dcu); /* BRA block_a */
    vm.rom_used = 0x224u;

    /* M2.23: stopping inside a cached multi-op block must be exact. */
    CHECK(run_budget(&vm, backend, 1u, block_a + 2u, 1u, 0u, 0u) == 0);
    CHECK(run_budget(&vm, backend, 2u, block_a + 4u, 1u, 2u, 0u) == 0);
    CHECK(run_budget(&vm, backend, 3u, block_a + 6u, 1u, 2u, 0u) == 0);

    /* Exact block boundary executes the branch but nothing in the successor. */
    CHECK(run_budget(&vm, backend, 4u, block_b, 1u, 2u, 0u) == 0);

    /* One instruction into the chained successor must stop at block_b + 2. */
    CHECK(run_budget(&vm, backend, 5u, block_b + 2u, 1u, 2u, 3u) == 0);

    /* A non-multiple budget spanning both blocks must also remain exact. */
    CHECK(run_budget(&vm, backend, 7u, block_a + 2u, 1u, 2u, 3u) == 0);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.23 precise execution budget tests: PASS");
    return 0;
}
