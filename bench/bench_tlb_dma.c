#include "cpu.h"
#include "vm.h"

#include <inttypes.h>
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

static int run_case(struct amivm_vm *vm, bool same_page, bool revalidate)
{
    struct amivm_cpu_state cpu = {0};
    const uint32_t logical = 0x00401000u;
    const uint32_t expected = AMIVM_RAM_BASE + 0xb000u;
    const uint32_t dirty_offset = same_page ? 0x7100u : 0x9000u;
    const unsigned iterations = 100000u;
    uint32_t physical = 0u;
    clock_t start, end;
    unsigned i;
    cpu.tc = 0x80000000u;
    cpu.srp = AMIVM_RAM_BASE + 0x2000u;
    cpu.tlb_enabled = true;
    cpu.tlb_descriptor_revalidation = revalidate;
    start = clock();
    for (i = 0u; i < iterations; ++i) {
        vm->ram[dirty_offset] ^= 1u;
        if (!amivm_ram_mark_dirty(vm, AMIVM_RAM_BASE + dirty_offset, 1u) ||
            amivm_mmu_translate(&cpu, vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != expected)
            return -1;
    }
    end = clock();
    printf("scenario=%s iterations=%u ns_per_iteration=%.3f "
           "mmu_walks=%" PRIu64 " tlb_hits=%" PRIu64
           " tlb_misses=%" PRIu64 " tlb_invalidations=%" PRIu64 "\n",
           revalidate ? "same-descriptor-page-revalidate" :
               (same_page ? "same-descriptor-page" : "unrelated-page"),
           iterations, (double)(end - start) * 1e9 /
               ((double)CLOCKS_PER_SEC * (double)iterations),
           cpu.mmu_translation_walks, cpu.tlb_hits,
           cpu.tlb_misses, cpu.tlb_invalidations);
    if (same_page && revalidate)
        return cpu.mmu_translation_walks == 1u &&
               cpu.tlb_hits == iterations - 1u &&
               cpu.tlb_invalidations == 0u ? 0 : -1;
    if (same_page)
        return cpu.mmu_translation_walks == iterations &&
               cpu.tlb_invalidations == iterations - 1u ? 0 : -1;
    return cpu.mmu_translation_walks == 1u &&
           cpu.tlb_hits == iterations - 1u &&
           cpu.tlb_invalidations == 0u ? 0 : -1;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    int rc;
    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;
    if (!put32(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) ||
        !put32(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u)) {
        amivm_vm_destroy(&vm);
        return 1;
    }
    rc = run_case(&vm, false, false);
    if (rc == 0)
        rc = run_case(&vm, true, false);
    if (rc == 0)
        rc = run_case(&vm, true, true);
    amivm_vm_destroy(&vm);
    if (rc != 0)
        fputs("M345 DMA TLB benchmark invariant failed\n", stderr);
    return rc == 0 ? 0 : 1;
}
