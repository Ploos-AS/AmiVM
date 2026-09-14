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

static int write32_vm(struct amivm_vm *vm, uint32_t addr, uint32_t value)
{
    return amivm_write8(vm, addr, (uint8_t)(value >> 24u)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)(value >> 16u)) &&
           amivm_write8(vm, addr + 2u, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, addr + 3u, (uint8_t)value);
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
    const uint32_t ir_pc = AMIVM_ROM_BASE + 0x120u;
    const uint32_t block_pc = AMIVM_ROM_BASE + 0x200u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x60feu);
    put16_be(&vm.rom[0x120], 0x4280u);
    put16_be(&vm.rom[0x122], 0x60feu);

    /* M2.15 two-block loop. Block A is four IR ops, block B is two. */
    put16_be(&vm.rom[0x200], 0x7001u); /* MOVEQ #1,D0 */
    put16_be(&vm.rom[0x202], 0x7202u); /* MOVEQ #2,D1 */
    put16_be(&vm.rom[0x204], 0x4e71u); /* NOP */
    put16_be(&vm.rom[0x206], 0x6018u); /* BRA 0x220 */
    put16_be(&vm.rom[0x220], 0x7403u); /* MOVEQ #3,D2 */
    put16_be(&vm.rom[0x222], 0x60dcu); /* BRA 0x200 */
    vm.rom_used = 0x224u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 1000u) == 1);
    CHECK(cpu.pc == loop_pc);
    CHECK(exec.stats.instructions == 1000u);
    CHECK(exec.stats.cache_misses == 1u);
    CHECK(exec.stats.cache_hits == 999u);
    CHECK(exec.stats.stale_page_misses == 0u);
    CHECK(exec.stats.context_misses == 0u);
    CHECK(exec.stats.dispatches == 1u);
    CHECK(exec.stats.chain_hits == 999u);
    CHECK(exec.stats.chain_misses == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.jit_blocks == 1000u);
    CHECK(exec.stats.jit_instructions == 1000u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.exits == 0u);

    amivm_exec_invalidate_all(&exec);
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(exec.stats.instructions == 1001u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.cache_hits == 999u);
    CHECK(exec.stats.dispatches == 2u);
    CHECK(exec.stats.jit_blocks == 1001u);
    CHECK(exec.stats.jit_instructions == 1001u);

    /* M2.30: CLR.L plus the terminating BRA now remains native JIT. */
    cpu.pc = ir_pc;
    cpu.d[0] = 0xffffffffu;
    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(cpu.d[0] == 0u);
    CHECK(cpu.pc == ir_pc + 2u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.instructions == 1003u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.jit_blocks == 1002u);
    CHECK(exec.stats.jit_instructions == 1003u);

    CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
    CHECK(cpu.pc == ir_pc + 2u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.jit_blocks == 1003u);
    CHECK(exec.stats.jit_instructions == 1004u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.instructions == 1004u);

    amivm_exec_reset(&exec);
    CHECK(exec.backend == backend);
    CHECK(exec.stats.instructions == 0u);
    CHECK(exec.stats.cache_hits == 0u);
    CHECK(exec.stats.cache_misses == 0u);
    CHECK(exec.stats.dispatches == 0u);
    CHECK(exec.stats.chain_hits == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.jit_blocks == 0u);
    CHECK(exec.stats.fallbacks == 0u);

    /* M2.30: larger direct-branch blocks are native and still chain. */
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.pc = block_pc;
    amivm_exec_reset(&exec);
    CHECK(amivm_exec_run(&exec, &cpu, &vm, 600u) == 1);
    CHECK(cpu.pc == block_pc);
    CHECK(cpu.d[0] == 1u);
    CHECK(cpu.d[1] == 2u);
    CHECK(cpu.d[2] == 3u);
    CHECK(exec.stats.instructions == 600u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.ir_blocks == 0u);
    CHECK(exec.stats.jit_instructions == 600u);
    CHECK(exec.stats.jit_blocks == 200u);
    CHECK(exec.stats.cache_misses == 2u);
    CHECK(exec.stats.dispatches == 2u);
    CHECK(exec.stats.chain_misses == 1u);
    CHECK(exec.stats.chain_hits == 198u);
    CHECK(exec.stats.cache_hits == 198u);
    CHECK(exec.stats.fallbacks == 0u);

    amivm_exec_reset(&exec);
    CHECK(exec.backend == backend);

    /* M2.14: MMU-aware code dependencies remain page granular with JIT. */
    {
        const uint32_t srp = AMIVM_RAM_BASE + 0x4000u;
        const uint32_t sl2 = AMIVM_RAM_BASE + 0x5000u;
        const uint32_t data_page = AMIVM_RAM_BASE + 0x7000u;
        const uint32_t code_page = AMIVM_RAM_BASE + 0x9000u;
        const uint32_t alt_code_page = AMIVM_RAM_BASE + 0xa000u;
        const uint32_t logical_pc = 0x00400100u;
        const size_t code_offset = 0x9000u + 0x100u;
        const size_t alt_code_offset = 0xa000u + 0x100u;
        uint64_t code_generation;
        uint64_t data_generation;

        put32_be(&vm.ram[0x4000u + 4u], sl2 | 1u);
        put32_be(&vm.ram[0x5000u], code_page | 1u);
        put16_be(&vm.ram[code_offset], 0x60feu);
        put16_be(&vm.ram[code_offset + 2u], 0x60feu);
        put16_be(&vm.ram[alt_code_offset], 0x7009u);
        put16_be(&vm.ram[alt_code_offset + 2u], 0x60feu);

        CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
        cpu.srp = srp;
        cpu.tc = 0x80000000u;
        cpu.pc = logical_pc;
        amivm_exec_reset(&exec);

        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(cpu.pc == logical_pc);
        CHECK(exec.stats.jit_blocks == 1u);
        CHECK(exec.stats.ir_blocks == 0u);
        CHECK(exec.stats.cache_misses == 1u);
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.cache_hits == 1u);

        CHECK(amivm_ram_page_generation(&vm, code_page, &code_generation));
        CHECK(amivm_ram_page_generation(&vm, data_page, &data_generation));
        CHECK(amivm_write8(&vm, data_page + 0x20u, 0xa5u));
        CHECK(amivm_ram_page_generation(&vm, data_page, &data_generation));
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.cache_hits == 2u);
        CHECK(exec.stats.stale_page_misses == 0u);

        CHECK(amivm_write8(&vm, code_page + 0x100u, 0x70u));
        CHECK(amivm_write8(&vm, code_page + 0x101u, 0x07u));
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.stale_page_misses == 1u);
        CHECK(exec.stats.cache_misses == 2u);
        CHECK(cpu.d[0] == 7u);
        CHECK(cpu.pc == logical_pc + 2u);

        CHECK(amivm_write8(&vm, code_page + 0x100u, 0x60u));
        CHECK(amivm_write8(&vm, code_page + 0x101u, 0xfeu));
        cpu.pc = logical_pc;
        cpu.d[0] = 0u;
        amivm_exec_reset(&exec);
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(cpu.pc == logical_pc);
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.cache_hits == 1u);

        CHECK(write32_vm(&vm, sl2, alt_code_page | 1u));
        CHECK(amivm_exec_step(&exec, &cpu, &vm) == 1);
        CHECK(exec.stats.stale_page_misses == 1u);
        CHECK(exec.stats.cache_misses == 2u);
        CHECK(cpu.d[0] == 9u);
        CHECK(cpu.pc == logical_pc + 2u);
    }

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.30 JIT-aware execution tests: PASS");
    return 0;
}
