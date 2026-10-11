#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>

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

    amivm_vm_destroy(&vm);
    puts("M306 stack translation invariants: PASS");
    return 0;
fail:
    fprintf(stderr, "M306 stack translation invariants: FAIL (rc=%d sp=%08x)\n",
            rc, cpu.a[7]);
    amivm_vm_destroy(&vm);
    return 1;
}
