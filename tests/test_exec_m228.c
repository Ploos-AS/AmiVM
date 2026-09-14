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
    const uint32_t start_pc = AMIVM_ROM_BASE + 0x200u;
    const size_t index = (size_t)((start_pc >> 1u) & (AMIVM_EXEC_CACHE_ENTRIES - 1u));
    void *first_mapping;
    size_t i;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], start_pc);
    put16_be(&vm.rom[0x200], 0x7001u); /* MOVEQ #1,D0 */
    put16_be(&vm.rom[0x202], 0x5280u); /* ADDQ.L #1,D0 */
    for (i = 2u; i < AMIVM_EXEC_DECODE_WORDS; ++i)
        put16_be(&vm.rom[0x200u + (i * 2u)], 0x4e71u); /* NOP */
    vm.rom_used = 0x200u + (AMIVM_EXEC_DECODE_WORDS * 2u);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    amivm_exec_init(&exec, backend);

    CHECK(amivm_exec_run(&exec, &cpu, &vm, AMIVM_EXEC_DECODE_WORDS) == 1);
    CHECK(cpu.d[0] == 2u);
#if defined(__x86_64__) && defined(__linux__)
    CHECK(exec.stats.jit_prepares == 1u);
    CHECK(exec.stats.jit_blocks == 1u);
    CHECK(exec.cache[index].jit_valid == 1);
    CHECK(exec.cache[index].jit_runtime.mapping != NULL);
    first_mapping = exec.cache[index].jit_runtime.mapping;

    cpu.pc = start_pc;
    cpu.d[0] = 0u;
    CHECK(amivm_exec_run(&exec, &cpu, &vm, AMIVM_EXEC_DECODE_WORDS) == 1);
    CHECK(cpu.d[0] == 2u);
    CHECK(exec.stats.jit_prepares == 1u);
    CHECK(exec.stats.jit_blocks == 2u);
    CHECK(exec.cache[index].jit_runtime.mapping == first_mapping);

    amivm_exec_invalidate_all(&exec);
    CHECK(exec.cache[index].jit_runtime.mapping == NULL);
    cpu.pc = start_pc;
    cpu.d[0] = 0u;
    CHECK(amivm_exec_run(&exec, &cpu, &vm, AMIVM_EXEC_DECODE_WORDS) == 1);
    CHECK(cpu.d[0] == 2u);
    CHECK(exec.stats.jit_prepares == 2u);
    CHECK(exec.stats.jit_blocks == 3u);
#else
    first_mapping = NULL;
    (void)first_mapping;
    CHECK(exec.stats.jit_blocks == 0u);
#endif

    amivm_exec_reset(&exec);
    amivm_vm_destroy(&vm);
    puts("AmiVM M2.28 persistent executable JIT cache tests: PASS");
    return 0;
}
