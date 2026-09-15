#include "exec.h"
#include "vm.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

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
    const uint32_t sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t caller = AMIVM_ROM_BASE + 0x200u;
    const uint32_t callee = AMIVM_ROM_BASE + 0x220u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    put32_be(&vm.rom[0], sp);
    put32_be(&vm.rom[4], caller);

    /* caller: BSR callee; BRA caller. callee: RTS. */
    put16_be(&vm.rom[0x200], 0x611eu);
    put16_be(&vm.rom[0x202], 0x60fcu);
    put16_be(&vm.rom[0x220], 0x4e75u);
    vm.rom_used = 0x222u;

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;
    amivm_exec_init(&exec, backend);

    CHECK(amivm_exec_run(&exec, &cpu, &vm, 300u) == 1);
    CHECK(cpu.pc == caller);
    CHECK(cpu.a[7] == sp);
    CHECK(cpu.isp == sp);
    CHECK(exec.stats.instructions == 300u);
    CHECK(exec.stats.jit_instructions == 300u);
    CHECK(exec.stats.ir_instructions == 0u);
    CHECK(exec.stats.fallbacks == 0u);
    CHECK(exec.stats.jit_fallbacks == 0u);

    /* Three native one-instruction blocks become hot; after warm-up all
       BSR->RTS, RTS->return-site, and BRA->caller edges are chainable. */
    CHECK(exec.stats.cache_misses == 3u);
    CHECK(exec.stats.jit_prepares == 3u);
    CHECK(exec.stats.dispatches == 3u);
    CHECK(exec.stats.chain_misses == 2u);
    CHECK(exec.stats.jit_chain_misses == 2u);
    CHECK(exec.stats.chain_hits == 297u);
    CHECK(exec.stats.jit_chain_hits == 297u);

    amivm_exec_reset(&exec);
    amivm_vm_destroy(&vm);
    puts("AmiVM M2.42 native call/return chaining tests: PASS");
    return 0;
}
