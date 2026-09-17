#include "jit_helpers.h"

#include <stddef.h>

static int supervisor_mode(const struct amivm_cpu_state *cpu)
{
    return (cpu->sr & 0x2000u) != 0u;
}

static void sync_a7_bank(struct amivm_cpu_state *cpu)
{
    if ((cpu->sr & 0x2000u) == 0u) cpu->usp = cpu->a[7];
    else if ((cpu->sr & 0x1000u) != 0u) cpu->msp = cpu->a[7];
    else cpu->isp = cpu->a[7];
}

static int translate_long(struct amivm_jit_context *context, uint32_t logical,
                          int write, uint32_t physical[4])
{
    unsigned i;
    if (context == NULL || context->cpu == NULL || context->vm == NULL ||
        (logical & 1u) != 0u) return -4;
    for (i = 0u; i < 4u; ++i) {
        if (amivm_mmu_translate(context->cpu, context->vm, logical + i,
                                write != 0, supervisor_mode(context->cpu),
                                &physical[i]) != AMIVM_MMU_OK) return -4;
    }
    return 0;
}

void amivm_jit_context_init(struct amivm_jit_context *context,
                            struct amivm_cpu_state *cpu,
                            struct amivm_vm *vm)
{
    if (context == NULL) return;
    context->cpu = cpu;
    context->vm = vm;
}

int amivm_jit_helper_stack_push_long(struct amivm_jit_context *context,
                                     uint32_t value)
{
    uint32_t physical[4]; uint32_t new_sp; unsigned i; int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    new_sp = context->cpu->a[7] - 4u;
    rc = translate_long(context, new_sp, 1, physical); if (rc != 0) return rc;
    for (i = 0u; i < 4u; ++i) {
        const unsigned shift = (3u - i) * 8u;
        if (!amivm_write8(context->vm, physical[i], (uint8_t)(value >> shift))) return -4;
    }
    context->cpu->a[7] = new_sp; sync_a7_bank(context->cpu); return 0;
}

int amivm_jit_helper_stack_pop_long(struct amivm_jit_context *context,
                                    uint32_t *value)
{
    uint32_t physical[4]; uint8_t byte[4]; unsigned i; int rc;
    if (context == NULL || context->cpu == NULL || value == NULL) return -4;
    rc = translate_long(context, context->cpu->a[7], 0, physical); if (rc != 0) return rc;
    for (i = 0u; i < 4u; ++i) if (!amivm_read8(context->vm, physical[i], &byte[i])) return -4;
    *value = ((uint32_t)byte[0] << 24u) | ((uint32_t)byte[1] << 16u) |
             ((uint32_t)byte[2] << 8u) | (uint32_t)byte[3];
    context->cpu->a[7] += 4u; sync_a7_bank(context->cpu); return 0;
}

int amivm_jit_helper_bsr(struct amivm_jit_context *context,
                         uint32_t return_pc, uint32_t target_pc)
{
    int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = amivm_jit_helper_stack_push_long(context, return_pc); if (rc != 0) return rc;
    context->cpu->pc = target_pc; return 0;
}

int amivm_jit_helper_rts(struct amivm_jit_context *context)
{
    uint32_t target_pc; int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = amivm_jit_helper_stack_pop_long(context, &target_pc); if (rc != 0) return rc;
    context->cpu->pc = target_pc; return 0;
}

int amivm_jit_helper_jsr_an(struct amivm_jit_context *context,
                            uint32_t return_pc, uint32_t address_register)
{
    uint32_t target_pc; int rc;
    if (context == NULL || context->cpu == NULL || address_register >= 8u) return -4;
    target_pc = context->cpu->a[address_register];
    rc = amivm_jit_helper_stack_push_long(context, return_pc); if (rc != 0) return rc;
    context->cpu->pc = target_pc; return 0;
}

int amivm_jit_helper_jmp_an(struct amivm_jit_context *context,
                            uint32_t address_register)
{
    if (context == NULL || context->cpu == NULL || address_register >= 8u) return -4;
    context->cpu->pc = context->cpu->a[address_register]; return 0;
}

int amivm_jit_helper_jmp_abs(struct amivm_jit_context *context,
                             uint32_t target_pc)
{
    if (context == NULL || context->cpu == NULL) return -4;
    context->cpu->pc = target_pc;
    return 0;
}

static int decode_d16_an(uint32_t encoded, uint32_t *reg, int32_t *disp)
{
    if (reg == NULL || disp == NULL) return -4;
    *reg = encoded >> 16u;
    if (*reg >= 8u) return -4;
    *disp = (int32_t)(int16_t)(encoded & 0xffffu);
    return 0;
}

int amivm_jit_helper_jsr_d16_an(struct amivm_jit_context *context,
                                uint32_t return_pc, uint32_t encoded)
{
    uint32_t reg;
    int32_t disp;
    uint32_t target_pc;
    int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = decode_d16_an(encoded, &reg, &disp);
    if (rc != 0) return rc;
    target_pc = context->cpu->a[reg] + (uint32_t)disp;
    rc = amivm_jit_helper_stack_push_long(context, return_pc);
    if (rc != 0) return rc;
    context->cpu->pc = target_pc;
    return 0;
}

int amivm_jit_helper_jmp_d16_an(struct amivm_jit_context *context,
                                uint32_t encoded)
{
    uint32_t reg;
    int32_t disp;
    int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = decode_d16_an(encoded, &reg, &disp);
    if (rc != 0) return rc;
    context->cpu->pc = context->cpu->a[reg] + (uint32_t)disp;
    return 0;
}
