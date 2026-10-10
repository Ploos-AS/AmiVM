#include "exec.h"
#include "vm.h"

#include <stdio.h>
#include <time.h>

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
    const uint64_t budget = 1000000u;
    const uint32_t subroutine_pc = AMIVM_ROM_BASE + 0x110u;
    clock_t begin;
    clock_t end;
    double seconds;
    double ips;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) return 1;

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    /* M300: JSR absolute-long -> RTS -> BRA back to call site.
     * This exercises the optimized call/return path under a stable loop. */
    put16_be(&vm.rom[0x100], 0x4eb9u);
    put32_be(&vm.rom[0x102], subroutine_pc);
    put16_be(&vm.rom[0x106], 0x60f8u);
    put16_be(&vm.rom[0x110], 0x4e75u);
    vm.rom_used = 0x112u;

    if (amivm_cpu_reset(&cpu, &vm, backend) != 0) {
        amivm_vm_destroy(&vm);
        return 1;
    }
    amivm_exec_init(&exec, backend);

    begin = clock();
    if (amivm_exec_run(&exec, &cpu, &vm, budget) <= 0) {
        amivm_vm_destroy(&vm);
        return 1;
    }
    end = clock();

    seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    ips = seconds > 0.0 ? (double)exec.stats.instructions / seconds : 0.0;
    printf("AmiVM M300 JSR/RTS control-flow benchmark\n");
    printf("instructions=%llu seconds=%.6f ips=%.0f cache_hits=%llu cache_misses=%llu\n",
           (unsigned long long)exec.stats.instructions, seconds, ips,
           (unsigned long long)exec.stats.cache_hits,
           (unsigned long long)exec.stats.cache_misses);

    printf("jit_blocks=%llu ir_blocks=%llu fallbacks=%llu jit_fallbacks=%llu\n",
           (unsigned long long)exec.stats.jit_blocks,
           (unsigned long long)exec.stats.ir_blocks,
           (unsigned long long)exec.stats.fallbacks,
           (unsigned long long)exec.stats.jit_fallbacks);
    if (exec.stats.instructions != budget || exec.stats.fallbacks != 0u) {
        amivm_vm_destroy(&vm);
        return 1;
    }
    amivm_vm_destroy(&vm);
    return 0;
}
