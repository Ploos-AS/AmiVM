#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>

static int put32_be(struct amivm_vm *vm, uint32_t offset, uint32_t v)
{
    const uint8_t bytes[4] = {
        (uint8_t)(v >> 24u), (uint8_t)(v >> 16u),
        (uint8_t)(v >> 8u), (uint8_t)v
    };
    return amivm_ram_write(vm, AMIVM_RAM_BASE + offset, bytes, sizeof(bytes)) ? 0 : -1;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu = {0};
    struct amivm_jit_context ctx;
    uint32_t value = 0u;
    uint32_t original_sp;
    int rc;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) return 1;
    amivm_jit_context_init(&ctx, &cpu, &vm);
    cpu.sr = 0x2000u;
    original_sp = AMIVM_RAM_BASE + 0x1000u;
    cpu.a[7] = original_sp;
    cpu.isp = original_sp;

    rc = amivm_jit_helper_stack_push_long(&ctx, 0x12345678u);
    if (rc != 0 || cpu.a[7] != original_sp - 4u ||
        cpu.isp != cpu.a[7]) goto fail;
    rc = amivm_jit_helper_stack_pop_long(&ctx, &value);
    if (rc != 0 || value != 0x12345678u ||
        cpu.a[7] != original_sp || cpu.isp != original_sp) goto fail;

    /* Odd stack addresses must fail without updating the stack pointer. */
    cpu.a[7] = original_sp + 1u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_pop_long(&ctx, &value);
    if (rc == 0 || cpu.a[7] != original_sp + 1u ||
        cpu.isp != original_sp + 1u) goto fail;

    /* The same invariant must hold for an invalid push address. */
    cpu.a[7] = original_sp + 1u;
    rc = amivm_jit_helper_stack_push_long(&ctx, 0xdeadbeefu);
    if (rc == 0 || cpu.a[7] != original_sp + 1u) goto fail;

    /* Longword spans two 4 KiB pages: bytes at 0xffe..0x1001. */
    cpu.a[7] = AMIVM_RAM_BASE + AMIVM_RAM_PAGE_SIZE + 2u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0xa1b2c3d4u);
    if (rc != 0 || cpu.a[7] != AMIVM_RAM_BASE + AMIVM_RAM_PAGE_SIZE - 2u ||
        cpu.isp != cpu.a[7]) goto fail;
    value = 0u;
    rc = amivm_jit_helper_stack_pop_long(&ctx, &value);
    if (rc != 0 || value != 0xa1b2c3d4u ||
        cpu.a[7] != AMIVM_RAM_BASE + AMIVM_RAM_PAGE_SIZE + 2u ||
        cpu.isp != cpu.a[7]) goto fail;

    /* A failed out-of-RAM pop must preserve SP and the output value. */
    cpu.a[7] = AMIVM_RAM_BASE + (uint32_t)config.ram_size - 2u;
    cpu.isp = cpu.a[7];
    value = 0xfeedfaceu;
    rc = amivm_jit_helper_stack_pop_long(&ctx, &value);
    if (rc == 0 || cpu.a[7] != AMIVM_RAM_BASE + (uint32_t)config.ram_size - 2u ||
        cpu.isp != cpu.a[7] || value != 0xfeedfaceu) goto fail;

    /* MMU-enabled: two adjacent logical pages map to nonadjacent RAM pages. */
    cpu.tc = 0x80000000u;
    cpu.sr = 0x2000u;
    cpu.srp = AMIVM_RAM_BASE + 0x2000u;
    if (put32_be(&vm, 0x2000u + 4u, (AMIVM_RAM_BASE + 0x3000u) | 1u) != 0) goto fail;
    if (put32_be(&vm, 0x3000u + 0u, (AMIVM_RAM_BASE + 0x8000u) | 1u) != 0) goto fail;
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xa000u) | 1u) != 0) goto fail;
    cpu.a[7] = 0x00401002u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x89abcdefu);
    if (rc != 0 || cpu.a[7] != 0x00400ffeu) goto fail;
    value = 0u;
    rc = amivm_jit_helper_stack_pop_long(&ctx, &value);
    if (rc != 0 || value != 0x89abcdefu || cpu.a[7] != 0x00401002u) goto fail;

    /* A protected second page must reject the entire translation. */
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xa000u) | 3u) != 0) goto fail;
    cpu.a[7] = 0x00401002u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x12345678u);
    if (rc == 0 || cpu.a[7] != 0x00401002u || cpu.mmusr == 0u) goto fail;

    /*
     * M312: page-table mutations must be visible immediately. These tests
     * establish the invalidation contract before adding any TLB.
     */
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xa000u) | 1u) != 0) goto fail;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x11223344u);
    if (rc != 0 || vm.ram[0xa000u] != 0x11u) goto fail;
    /* Remap the same logical page without resetting the CPU. */
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0) goto fail;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x55667788u);
    if (rc != 0 || vm.ram[0xb000u] != 0x55u ||
        vm.ram[0xa000u] != 0x11u) goto fail;
    /* Write protection must take effect after a previously valid write. */
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xb000u) | 3u) != 0) goto fail;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0xdeadbeefu);
    if (rc == 0 || cpu.a[7] != 0x00401004u ||
        vm.ram[0xb000u] != 0x55u || cpu.mmusr == 0u) goto fail;
    /* Restore permissions, then switch supervisor root and verify mapping. */
    if (put32_be(&vm, 0x3000u + 4u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0) goto fail;
    if (put32_be(&vm, 0x5000u + 4u, (AMIVM_RAM_BASE + 0x6000u) | 1u) != 0) goto fail;
    if (put32_be(&vm, 0x6000u + 4u, (AMIVM_RAM_BASE + 0xc000u) | 1u) != 0) goto fail;
    cpu.srp = AMIVM_RAM_BASE + 0x5000u;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0xaabbccddu);
    if (rc != 0 || vm.ram[0xc000u] != 0xaau ||
        vm.ram[0xb000u] != 0x55u) goto fail;
    /* User mode must use URP rather than the supervisor root. */
    cpu.urp = AMIVM_RAM_BASE + 0x2000u;
    cpu.sr = 0u;
    cpu.a[7] = 0x00401004u;
    cpu.usp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x99aabbccu);
    if (rc != 0 || vm.ram[0xb000u] != 0x99u ||
        vm.ram[0xc000u] != 0xaau || cpu.usp != 0x00401000u) goto fail;

    /*
     * M313 cache contract: a changed L1 descriptor, an invalidated L2
     * descriptor and a disabled MMU must all be observed immediately.
     */
    cpu.sr = 0x2000u;
    cpu.srp = AMIVM_RAM_BASE + 0x2000u;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x01020304u);
    if (rc != 0 || vm.ram[0xb000u] != 0x01u) goto fail;
    /* Replace the L1 table pointer while retaining the same logical page. */
    if (put32_be(&vm, 0x7000u + 4u, (AMIVM_RAM_BASE + 0xc000u) | 1u) != 0) goto fail;
    if (put32_be(&vm, 0x2000u + 4u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0) goto fail;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x10203040u);
    if (rc != 0 || vm.ram[0xc000u] != 0x10u ||
        vm.ram[0xb000u] != 0x01u) goto fail;
    /* Invalidating the L2 entry must fault instead of using a stale hit. */
    if (put32_be(&vm, 0x7000u + 4u, 0u) != 0) goto fail;
    cpu.a[7] = 0x00401004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0x55667788u);
    if (rc == 0 || cpu.a[7] != 0x00401004u ||
        vm.ram[0xc000u] != 0x10u || cpu.mmusr == 0u) goto fail;
    /* Turning translation off must bypass stale table entries entirely. */
    cpu.tc = 0u;
    cpu.a[7] = AMIVM_RAM_BASE + 0xd004u;
    cpu.isp = cpu.a[7];
    rc = amivm_jit_helper_stack_push_long(&ctx, 0xabcdef12u);
    if (rc != 0 || vm.ram[0xd000u] != 0xabu ||
        cpu.mmusr != 0u) goto fail;

    /*
     * M314: VM writes to page-table RAM must advance generation counters.
     * Direct host writes are deliberately not tracked by this API.
     */
    {
        uint64_t before = 0u, after = 0u;
        const uint32_t descriptor_addr = AMIVM_RAM_BASE + 0x7000u + 4u;
        if (!amivm_ram_page_generation(&vm, descriptor_addr, &before)) goto fail;
        if (!amivm_write8(&vm, descriptor_addr + 3u, 1u)) goto fail;
        if (!amivm_ram_page_generation(&vm, descriptor_addr, &after) ||
            after == before) goto fail;
    }

    /* M316: bulk writes track both pages and reject partial out-of-range writes. */
    {
        const uint8_t bytes[4] = {0x12u, 0x34u, 0x56u, 0x78u};
        uint64_t g0, g1, h0, h1;
        const uint32_t start = AMIVM_RAM_BASE + 0xeffeu;
        if (!amivm_ram_page_generation(&vm, start, &g0) ||
            !amivm_ram_page_generation(&vm, start + 2u, &h0)) goto fail;
        if (!amivm_ram_write(&vm, start, bytes, sizeof(bytes))) goto fail;
        if (!amivm_ram_page_generation(&vm, start, &g1) ||
            !amivm_ram_page_generation(&vm, start + 2u, &h1) ||
            g1 == g0 || h1 == h0 ||
            vm.ram[0xeffeu] != 0x12u || vm.ram[0xf001u] != 0x78u) goto fail;
        if (amivm_ram_write(&vm, AMIVM_RAM_BASE + (uint32_t)vm.ram_size - 2u,
                            bytes, sizeof(bytes)) ||
            vm.ram[vm.ram_size - 2u] == 0x12u) goto fail;
    }

    /*
     * M319: exact generation increments are required for predictable TLB
     * validation. A rejected bulk write must change neither memory nor
     * generation counters.
     */
    {
        const uint8_t bytes[4] = {0x91u, 0x92u, 0x93u, 0x94u};
        const uint32_t boundary = AMIVM_RAM_BASE + 0x11ffeu;
        uint64_t first_before, second_before, untouched_before;
        uint64_t first_after, second_after, untouched_after;
        uint64_t global_before;
        if (!amivm_ram_page_generation(&vm, boundary, &first_before) ||
            !amivm_ram_page_generation(&vm, boundary + 2u, &second_before) ||
            !amivm_ram_page_generation(&vm, boundary + 0x1002u, &untouched_before))
            goto fail;
        global_before = vm.memory_write_generation;
        if (!amivm_ram_write(&vm, boundary, bytes, sizeof(bytes)) ||
            !amivm_ram_page_generation(&vm, boundary, &first_after) ||
            !amivm_ram_page_generation(&vm, boundary + 2u, &second_after) ||
            !amivm_ram_page_generation(&vm, boundary + 0x1002u, &untouched_after) ||
            first_after != first_before + 1u ||
            second_after != second_before + 1u ||
            untouched_after != untouched_before ||
            vm.memory_write_generation != global_before + 2u) goto fail;
        global_before = vm.memory_write_generation;
        if (amivm_ram_write(&vm, boundary, bytes, 0u) ||
            amivm_ram_write(&vm, AMIVM_RAM_BASE + (uint32_t)vm.ram_size - 1u,
                            bytes, sizeof(bytes)) ||
            vm.memory_write_generation != global_before ||
            vm.ram[0x11ffeu] != 0x91u || vm.ram[0x12001u] != 0x94u)
            goto fail;
    }

    /* M320: direct host writes can explicitly invalidate touched RAM pages. */
    {
        const uint32_t start = AMIVM_RAM_BASE + 0x13ffeu;
        uint64_t first, second, untouched;
        uint64_t after_first, after_second, after_untouched;
        uint64_t global_before;
        if (!amivm_ram_page_generation(&vm, start, &first) ||
            !amivm_ram_page_generation(&vm, start + 2u, &second) ||
            !amivm_ram_page_generation(&vm, start + 0x1002u, &untouched))
            goto fail;
        vm.ram[0x13ffeu] = 0xacu;
        vm.ram[0x14001u] = 0xbdu;
        global_before = vm.memory_write_generation;
        if (!amivm_ram_mark_dirty(&vm, start, 4u) ||
            !amivm_ram_page_generation(&vm, start, &after_first) ||
            !amivm_ram_page_generation(&vm, start + 2u, &after_second) ||
            !amivm_ram_page_generation(&vm, start + 0x1002u, &after_untouched) ||
            after_first != first + 1u || after_second != second + 1u ||
            after_untouched != untouched ||
            vm.memory_write_generation != global_before + 2u) goto fail;
        global_before = vm.memory_write_generation;
        if (amivm_ram_mark_dirty(&vm, start, 0u) ||
            amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + (uint32_t)vm.ram_size - 1u, 4u) ||
            vm.memory_write_generation != global_before) goto fail;
    }

    /*
     * M321: host-written MMU descriptors must be explicitly marked dirty.
     * Reuse the existing supervisor mapping, restore its L2 descriptor
     * through raw RAM, and prove translation observes the new target.
     */
    {
        const uint32_t descriptor = AMIVM_RAM_BASE + 0x7004u;
        const uint32_t logical = 0x00401000u;
        uint64_t before, after;
        uint32_t physical = 0u;
        const uint32_t mapped = AMIVM_RAM_BASE + 0xb000u;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.sr = 0x2000u;
        /* Restore the L1 pointer to the 0x7000 L2 table. */
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0)
            goto fail;
        if (!amivm_ram_page_generation(&vm, descriptor, &before)) goto fail;
        vm.ram[0x7004u] = (uint8_t)((mapped | 1u) >> 24u);
        vm.ram[0x7005u] = (uint8_t)((mapped | 1u) >> 16u);
        vm.ram[0x7006u] = (uint8_t)((mapped | 1u) >> 8u);
        vm.ram[0x7007u] = (uint8_t)(mapped | 1u);
        if (!amivm_ram_mark_dirty(&vm, descriptor, 4u) ||
            !amivm_ram_page_generation(&vm, descriptor, &after) ||
            after != before + 1u ||
            amivm_mmu_translate(&cpu, &vm, logical, true, true, &physical) != AMIVM_MMU_OK ||
            physical != mapped) goto fail;
    }

    /* M322: count real table walks and MMU-disabled bypasses separately. */
    {
        uint32_t physical = 0u;
        const uint64_t walks = cpu.mmu_translation_walks;
        const uint64_t bypasses = cpu.mmu_translation_bypasses;
        cpu.tc = 0u;
        if (amivm_mmu_translate(&cpu, &vm, AMIVM_RAM_BASE, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE ||
            cpu.mmu_translation_bypasses != bypasses + 1u ||
            cpu.mmu_translation_walks != walks) goto fail;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.mmu_translation_bypasses != bypasses + 1u) goto fail;
    }

    /* M324: reset invalidates every TLB entry and disables opt-in caching. */
    {
        size_t i;
        vm.rom_used = vm.rom_used < 8u ? 8u : vm.rom_used;
        cpu.tlb_enabled = true;
        cpu.tlb_hits = 7u;
        cpu.tlb_misses = 8u;
        cpu.tlb_invalidations = 9u;
        cpu.tlb[0].valid = true;
        if (amivm_cpu_reset(&cpu, &vm, amivm_cpu_reference_backend()) != 0 ||
            cpu.tlb_enabled || cpu.tlb_hits != 0u || cpu.tlb_misses != 0u ||
            cpu.tlb_invalidations != 0u || cpu.mmu_translation_walks != 0u ||
            cpu.mmu_translation_bypasses != 0u) goto fail;
        for (i = 0u; i < AMIVM_TLB_ENTRY_COUNT; ++i)
            if (cpu.tlb[i].valid) goto fail;
    }

    /* M325: opt-in TLB hits, tracked remap invalidation and WP faults. */
    {
        const uint32_t logical = 0x00401000u;
        const uint32_t descriptor = AMIVM_RAM_BASE + 0x7004u;
        uint32_t physical = 0u;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.tlb_misses != 1u || cpu.tlb_hits != 0u) goto fail;
        if (amivm_mmu_translate(&cpu, &vm, logical + 2u, false, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb002u ||
            cpu.tlb_hits != 1u || cpu.mmu_translation_walks != 1u) goto fail;
        if (put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xc000u) | 3u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.tlb_invalidations != 1u || cpu.tlb_misses != 2u) goto fail;
        if (amivm_mmu_translate(&cpu, &vm, logical, true, true, &physical) !=
                AMIVM_MMU_FAULT_WRITE_PROTECT ||
            cpu.tlb_hits != 2u) goto fail;
        /* Restore writable mapping and verify the stale WP entry is rejected. */
        if (put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, true, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.tlb_invalidations != 2u) goto fail;
        (void)descriptor;
    }

    /* M326: TLB must not leak across privilege, root, or MMU mode. */
    {
        const uint32_t logical = 0x00401000u;
        uint32_t physical = 0u;
        uint64_t misses, bypasses;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.urp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        misses = cpu.tlb_misses;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, false, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.tlb_misses != misses + 1u) goto fail;
        cpu.srp = 0u;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) !=
                AMIVM_MMU_FAULT_ROOT || cpu.mmusr == 0u) goto fail;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tc = 0u;
        bypasses = cpu.mmu_translation_bypasses;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) != AMIVM_MMU_OK ||
            physical != logical || cpu.mmu_translation_bypasses != bypasses + 1u)
            goto fail;
        cpu.tc = 0x80000000u;
        if (amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        /* Invalid L2 descriptors must fault, never reuse a stale hit. */
        if (put32_be(&vm, 0x7004u, 0u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true, &physical) !=
                AMIVM_MMU_FAULT_PAGE || cpu.mmusr == 0u) goto fail;
    }

    /* M332: explicit flush must force a new walk and preserve opt-in mode. */
    {
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        if (put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0)
            goto fail;
        cpu.tlb_enabled = true;
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        amivm_mmu_tlb_flush(&cpu);
        if (!cpu.tlb_enabled || cpu.tlb_invalidations != invalidations + 1u)
            goto fail;
        amivm_mmu_tlb_flush(&cpu);
        if (cpu.tlb_invalidations != invalidations + 1u) goto fail;
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks + 1u) goto fail;
    }

    /* M333: reusing a CPU with a different VM requires an explicit flush. */
    {
        struct amivm_vm other;
        uint32_t physical = 0u;
        uint64_t walks;
        if (amivm_vm_init(&other, &config) != 0) goto fail;
        if (put32_be(&other, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&other, 0x7004u, (AMIVM_RAM_BASE + 0xc000u) | 1u) != 0) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        /* Populate the old VM's cache entry before switching. */
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        walks = cpu.mmu_translation_walks;
        /* M334: VM identity prevents stale hits even without explicit flush. */
        if (amivm_mmu_translate(&cpu, &other, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.mmu_translation_walks != walks + 1u) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        walks = cpu.mmu_translation_walks;
        amivm_mmu_tlb_flush(&cpu);
        if (amivm_mmu_translate(&cpu, &other, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.mmu_translation_walks != walks + 1u) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        /* M338: switching back without flushing must recover the original
           mapping, even when the same direct-mapped TLB slot was replaced. */
        walks = cpu.mmu_translation_walks;
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks + 1u) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        /* A subsequent lookup within the original VM must now hit. */
        walks = cpu.mmu_translation_walks;
        if (amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks) {
            amivm_vm_destroy(&other);
            goto fail;
        }
        amivm_vm_destroy(&other);
    }

    /* M337: destroying and reinitializing at the same VM address must
       not allow stale TLB entries to match a new VM lifetime. */
    {
        struct amivm_vm reused;
        uint64_t previous_id, walks;
        uint32_t physical = 0u;
        if (amivm_vm_init(&reused, &config) != 0) goto fail;
        if (put32_be(&reused, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&reused, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0) {
            amivm_vm_destroy(&reused);
            goto fail;
        }
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        if (amivm_mmu_translate(&cpu, &reused, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) {
            amivm_vm_destroy(&reused);
            goto fail;
        }
        previous_id = reused.instance_id;
        walks = cpu.mmu_translation_walks;
        amivm_vm_destroy(&reused);
        if (amivm_vm_init(&reused, &config) != 0) goto fail;
        if (reused.instance_id == previous_id ||
            put32_be(&reused, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&reused, 0x7004u, (AMIVM_RAM_BASE + 0xd000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &reused, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xd000u ||
            cpu.mmu_translation_walks != walks + 1u) {
            amivm_vm_destroy(&reused);
            goto fail;
        }
        amivm_vm_destroy(&reused);
    }

    /* M339: host/DMA edits to an L2 descriptor require a dirty notification.
       The next translation must invalidate the cached mapping and rewalk. */
    {
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        /* Direct RAM writes emulate a host/DMA writer bypassing the VM API. */
        vm.ram[0x7004u] = (uint8_t)(((AMIVM_RAM_BASE + 0xc000u) | 1u) >> 24u);
        vm.ram[0x7005u] = (uint8_t)(((AMIVM_RAM_BASE + 0xc000u) | 1u) >> 16u);
        vm.ram[0x7006u] = (uint8_t)(((AMIVM_RAM_BASE + 0xc000u) | 1u) >> 8u);
        vm.ram[0x7007u] = (uint8_t)((AMIVM_RAM_BASE + 0xc000u) | 1u);
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x7004u, 4u) ||
            amivm_mmu_translate(&cpu, &vm, 0x00401000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
    }

    /* M340: a single DMA dirty notification spanning two RAM pages must
       invalidate cached L2 descriptors on either touched page. */
    {
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        const uint32_t old_descriptor = (AMIVM_RAM_BASE + 0xb000u) | 1u;
        const uint32_t new_descriptor = (AMIVM_RAM_BASE + 0xd000u) | 1u;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7ffcu, old_descriptor) != 0 ||
            amivm_mmu_translate(&cpu, &vm, 0x007ff000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        vm.ram[0x7ffcu] = (uint8_t)(new_descriptor >> 24u);
        vm.ram[0x7ffdu] = (uint8_t)(new_descriptor >> 16u);
        vm.ram[0x7ffeu] = (uint8_t)(new_descriptor >> 8u);
        vm.ram[0x7fffu] = (uint8_t)new_descriptor;
        /* Touch the adjacent page as part of the same simulated DMA burst. */
        vm.ram[0x8000u] ^= 1u;
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x7ffcu, 5u) ||
            amivm_mmu_translate(&cpu, &vm, 0x007ff000u, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xd000u ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
    }

    /* M341: one dirty notification invalidates two distinct cached pages
       whose L2 descriptors reside in the same modified RAM page. */
    {
        const uint32_t logical_a = 0x00401000u;
        const uint32_t logical_b = 0x00402000u;
        const uint32_t old_a = (AMIVM_RAM_BASE + 0xb000u) | 1u;
        const uint32_t old_b = (AMIVM_RAM_BASE + 0xc000u) | 1u;
        const uint32_t new_a = (AMIVM_RAM_BASE + 0xd000u) | 1u;
        const uint32_t new_b = (AMIVM_RAM_BASE + 0xe000u) | 1u;
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        amivm_mmu_tlb_flush(&cpu);
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, old_a) != 0 ||
            put32_be(&vm, 0x7008u, old_b) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical_a, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            amivm_mmu_translate(&cpu, &vm, logical_b, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        vm.ram[0x7004u] = (uint8_t)(new_a >> 24u);
        vm.ram[0x7005u] = (uint8_t)(new_a >> 16u);
        vm.ram[0x7006u] = (uint8_t)(new_a >> 8u);
        vm.ram[0x7007u] = (uint8_t)new_a;
        vm.ram[0x7008u] = (uint8_t)(new_b >> 24u);
        vm.ram[0x7009u] = (uint8_t)(new_b >> 16u);
        vm.ram[0x700au] = (uint8_t)(new_b >> 8u);
        vm.ram[0x700bu] = (uint8_t)new_b;
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x7004u, 8u) ||
            amivm_mmu_translate(&cpu, &vm, logical_a, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xd000u ||
            amivm_mmu_translate(&cpu, &vm, logical_b, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xe000u ||
            cpu.mmu_translation_walks != walks + 2u ||
            cpu.tlb_invalidations != invalidations + 2u) goto fail;
    }

    /* M342: a dirty notification on unrelated RAM must preserve a cached
       MMU translation and must not increment the invalidation counter. */
    {
        const uint32_t logical = 0x00401000u;
        uint32_t physical = 0u;
        uint64_t walks, hits, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        amivm_mmu_tlb_flush(&cpu);
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        hits = cpu.tlb_hits;
        invalidations = cpu.tlb_invalidations;
        vm.ram[0x9000u] ^= 1u;
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x9000u, 1u) ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks ||
            cpu.tlb_hits != hits + 1u ||
            cpu.tlb_invalidations != invalidations) goto fail;
    }

    /* M343: generation tracking is intentionally page-granular. A host
       write elsewhere on the L2 descriptor's RAM page must conservatively
       invalidate a cached translation, even if its descriptor is unchanged. */
    {
        const uint32_t logical = 0x00401000u;
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        amivm_mmu_tlb_flush(&cpu);
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        vm.ram[0x7100u] ^= 1u;
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x7100u, 1u) ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
    }

    /* M348: opt-in descriptor revalidation preserves an unchanged L2
       mapping after a same-page DMA write, but never hides remapping. */
    {
        const uint32_t logical = 0x00401000u;
        uint32_t physical = 0u;
        uint64_t walks, hits, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        cpu.tlb_descriptor_revalidation = true;
        amivm_mmu_tlb_flush(&cpu);
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        hits = cpu.tlb_hits;
        invalidations = cpu.tlb_invalidations;
        vm.ram[0x7100u] ^= 1u;
        if (!amivm_ram_mark_dirty(&vm, AMIVM_RAM_BASE + 0x7100u, 1u) ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u ||
            cpu.mmu_translation_walks != walks ||
            cpu.tlb_hits != hits + 1u ||
            cpu.tlb_invalidations != invalidations) goto fail;
        if (put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xc000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
        cpu.tlb_descriptor_revalidation = false;
    }

    /* M349: opt-in revalidation must not mask L1 replacement, page
       invalidation or write-protection faults after descriptor changes. */
    {
        const uint32_t logical = 0x00401000u;
        uint32_t physical = 0u;
        uint64_t walks, invalidations;
        cpu.tc = 0x80000000u;
        cpu.srp = AMIVM_RAM_BASE + 0x2000u;
        cpu.tlb_enabled = true;
        cpu.tlb_descriptor_revalidation = true;
        amivm_mmu_tlb_flush(&cpu);
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x7000u) | 1u) != 0 ||
            put32_be(&vm, 0x7004u, (AMIVM_RAM_BASE + 0xb000u) | 1u) != 0 ||
            put32_be(&vm, 0x8004u, (AMIVM_RAM_BASE + 0xc000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xb000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        if (put32_be(&vm, 0x2004u, (AMIVM_RAM_BASE + 0x8000u) | 1u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        if (put32_be(&vm, 0x8004u, 0u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_FAULT_PAGE ||
            cpu.mmu_translation_walks != walks + 1u ||
            cpu.tlb_invalidations != invalidations + 1u) goto fail;
        if (put32_be(&vm, 0x8004u, (AMIVM_RAM_BASE + 0xc000u) | 3u) != 0 ||
            amivm_mmu_translate(&cpu, &vm, logical, false, true,
                                &physical) != AMIVM_MMU_OK ||
            physical != AMIVM_RAM_BASE + 0xc000u) goto fail;
        walks = cpu.mmu_translation_walks;
        invalidations = cpu.tlb_invalidations;
        if (amivm_mmu_translate(&cpu, &vm, logical, true, true,
                                &physical) != AMIVM_MMU_FAULT_WRITE_PROTECT ||
            cpu.mmu_translation_walks != walks ||
            cpu.tlb_invalidations != invalidations) goto fail;
        cpu.tlb_descriptor_revalidation = false;
    }

    amivm_vm_destroy(&vm);
    puts("M349 descriptor revalidation faults: PASS");
    return 0;
fail:
    fprintf(stderr, "M306 stack translation invariants: FAIL (rc=%d sp=%08x)\n",
            rc, cpu.a[7]);
    amivm_vm_destroy(&vm);
    return 1;
}
