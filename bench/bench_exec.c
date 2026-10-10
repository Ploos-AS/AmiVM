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

int main(int argc, char **argv)
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
    double ns_per_instruction;
    int branch_only = argc > 1 && argv[1][0] == 'b' && argv[1][1] == '\0';

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) return 1;

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    /* M300: JSR absolute-long -> RTS -> BRA back to call site.
     * This exercises the optimized call/return path under a stable loop. */
    put16_be(&vm.rom[0x100], branch_only ? 0x60feu : 0x4eb9u);
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
    ns_per_instruction = exec.stats.instructions > 0u
        ? seconds * 1000000000.0 / (double)exec.stats.instructions : 0.0;
    printf("AmiVM M303 scenario=%s\n", branch_only ? "branch" : "jsr-rts");
    printf("instructions=%llu seconds=%.6f ips=%.0f cache_hits=%llu cache_misses=%llu\n",
           (unsigned long long)exec.stats.instructions, seconds, ips,
           (unsigned long long)exec.stats.cache_hits,
           (unsigned long long)exec.stats.cache_misses);

    printf("ns_per_instruction=%.3f cache_hit_percent=%.3f\n",
           ns_per_instruction,
           exec.stats.cache_hits + exec.stats.cache_misses > 0u
               ? 100.0 * (double)exec.stats.cache_hits /
                 (double)(exec.stats.cache_hits + exec.stats.cache_misses) : 0.0);
    printf("jit_blocks=%llu ir_blocks=%llu fallbacks=%llu jit_fallbacks=%llu\n",
           (unsigned long long)exec.stats.jit_blocks,
           (unsigned long long)exec.stats.ir_blocks,
           (unsigned long long)exec.stats.fallbacks,
           (unsigned long long)exec.stats.jit_fallbacks);
    /* Each iteration is JSR, RTS, BRA. At budget % 3 == 1 the
     * last instruction was JSR, with one return address on stack. */
    if (exec.stats.instructions != budget || exec.stats.fallbacks != 0u ||
        exec.stats.jit_fallbacks != 0u || exec.stats.ir_blocks != 0u ||
        exec.stats.jit_blocks != budget ||
        cpu.pc != (branch_only ? loop_pc : subroutine_pc) ||
        cpu.a[7] != (branch_only ? initial_sp : initial_sp - 4u)) {
        fprintf(stderr, "M301 JIT control-flow invariant failed: pc=%08x sp=%08x\n",
                cpu.pc, cpu.a[7]);
        amivm_vm_destroy(&vm);
        return 1;
    }
    amivm_vm_destroy(&vm);
    return 0;
}
