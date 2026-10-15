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

    amivm_vm_destroy(&vm);
    puts("M321 host MMU dirty notification: PASS");
    return 0;
fail:
    fprintf(stderr, "M306 stack translation invariants: FAIL (rc=%d sp=%08x)\n",
            rc, cpu.a[7]);
    amivm_vm_destroy(&vm);
    return 1;
}
