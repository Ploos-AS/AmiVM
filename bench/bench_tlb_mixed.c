#include "cpu.h"
#include "vm.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static bool put32(struct amivm_vm *vm, uint32_t offset, uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
        (uint8_t)(value >> 8u), (uint8_t)value
    };
    return amivm_ram_write(vm, AMIVM_RAM_BASE + offset, bytes, sizeof(bytes));
}

static int run_case(struct amivm_vm *vm, bool revalidate, unsigned same_interval)
{
    struct amivm_cpu_state cpu = {0};
    const uint32_t logical = 0x00401000u;
    const uint32_t expected = AMIVM_RAM_BASE + 0xb000u;
    const unsigned iterations = 100000u;
    const unsigned same_page_updates = (iterations - 1u) / same_interval;
    const unsigned unrelated_updates = (iterations - 1u) / (same_interval / 2u) - same_page_updates;
    uint32_t physical = 0u;
    clock_t start, end;
    unsigned i;
    cpu.tc = 0x80000000u;
    cpu.srp = AMIVM_RAM_BASE + 0x2000u;
    cpu.tlb_enabled = true;
    cpu.tlb_descriptor_revalidation = revalidate;
    start = clock();
    for (i = 0u; i < iterations; ++i) {
        if (i > 0u && i % same_interval == 0u) {
            vm->ram[0x7100u] ^= 1u;
            if (!amivm_ram_mark_dirty(vm, AMIVM_RAM_BASE + 0x7100u, 1u))
                return -1;
        } else if (i > 0u && i % (same_interval / 2u) == 0u) {
            vm->ram[0x9000u] ^= 1u;
            if (!amivm_ram_mark_dirty(vm, AMIVM_RAM_BASE + 0x9000u, 1u))
                return -1;
        }
        if (amivm_mmu_translate(&cpu, vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != expected) return -1;
    }
    end = clock();
    printf("scenario=%s same_interval=%u iterations=%u ns_per_iteration=%.3f "
           "mmu_walks=%" PRIu64 " tlb_hits=%" PRIu64
           " tlb_invalidations=%" PRIu64 "\n",
           revalidate ? "mixed-revalidate" : "mixed-conservative",
           same_interval, iterations, (double)(end - start) * 1e9 /
               ((double)CLOCKS_PER_SEC * (double)iterations),
           cpu.mmu_translation_walks, cpu.tlb_hits, cpu.tlb_invalidations);
    return cpu.mmu_translation_walks == (revalidate ? 1u : same_page_updates + 1u) &&
           cpu.tlb_hits == (revalidate ? iterations - 1u : iterations - same_page_updates - 1u) &&
           cpu.tlb_invalidations == (revalidate ? 0u : same_page_updates) &&
           unrelated_updates > 0u ? 0 : -1;
}

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    int rc;
    bool reverse = false;
    unsigned same_interval = 20u;
    int arg;
    for (arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "reverse") == 0) reverse = true;
        else if (strcmp(argv[arg], "low") == 0) same_interval = 200u;
        else if (strcmp(argv[arg], "high") == 0) same_interval = 4u;
        else return 2;
    }
    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) return 1;
    if (!put32(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) ||
        !put32(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u)) {
        amivm_vm_destroy(&vm);
        return 1;
    }
    if (reverse) {
        rc = run_case(&vm, true, same_interval);
        if (rc == 0) rc = run_case(&vm, false, same_interval);
    } else {
        rc = run_case(&vm, false, same_interval);
        if (rc == 0) rc = run_case(&vm, true, same_interval);
    }
    amivm_vm_destroy(&vm);
    if (rc != 0) fputs("M356 mixed DMA/TLB benchmark invariant failed\n", stderr);
    return rc == 0 ? 0 : 1;
}
