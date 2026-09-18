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
    uint32_t reg; int32_t disp; uint32_t target_pc; int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = decode_d16_an(encoded, &reg, &disp); if (rc != 0) return rc;
    target_pc = context->cpu->a[reg] + (uint32_t)disp;
    rc = amivm_jit_helper_stack_push_long(context, return_pc); if (rc != 0) return rc;
    context->cpu->pc = target_pc; return 0;
}

int amivm_jit_helper_jmp_d16_an(struct amivm_jit_context *context,
                                uint32_t encoded)
{
    uint32_t reg; int32_t disp; int rc;
    if (context == NULL || context->cpu == NULL) return -4;
    rc = decode_d16_an(encoded, &reg, &disp); if (rc != 0) return rc;
    context->cpu->pc = context->cpu->a[reg] + (uint32_t)disp; return 0;
}

/* encoded: bits 0..7 d8, 8..10 index reg, 11 A/D, 12 long,
 * 13..14 scale shift, 15 PC base, 16..18 address base reg. */
static int indexed_target(struct amivm_jit_context *context, uint32_t pc_base,
                          uint32_t encoded, uint32_t *target)
{
    uint32_t index_reg, index, base, scale;
    int32_t disp;
    if (context == NULL || context->cpu == NULL || target == NULL) return -4;
    index_reg = (encoded >> 8u) & 7u;
    index = (encoded & (1u << 11u)) != 0u ? context->cpu->a[index_reg] : context->cpu->d[index_reg];
    if ((encoded & (1u << 12u)) == 0u) index = (uint32_t)(int32_t)(int16_t)(index & 0xffffu);
    scale = 1u << ((encoded >> 13u) & 3u);
    disp = (int32_t)(int8_t)(encoded & 0xffu);
    if ((encoded & (1u << 15u)) != 0u) base = pc_base;
    else {
        uint32_t base_reg = (encoded >> 16u) & 7u;
        base = context->cpu->a[base_reg];
    }
    *target = base + (uint32_t)disp + index * scale;
    return 0;
}

int amivm_jit_helper_jsr_indexed(struct amivm_jit_context *context,
                                 uint32_t return_pc, uint32_t encoded)
{
    uint32_t target; int rc;
    rc = indexed_target(context, return_pc - 2u, encoded, &target); if (rc != 0) return rc;
    rc = amivm_jit_helper_stack_push_long(context, return_pc); if (rc != 0) return rc;
    context->cpu->pc = target; return 0;
}

int amivm_jit_helper_jmp_indexed(struct amivm_jit_context *context,
                                 uint32_t pc_base, uint32_t encoded)
{
    uint32_t target; int rc = indexed_target(context, pc_base, encoded, &target);
    if (rc != 0) return rc;
    context->cpu->pc = target; return 0;
}

/* M2.54 encoded: bits 0..2 index reg, 3 A/D, 4 long, 5..6 scale shift,
 * 7 PC base, 8..10 address base reg, 11 base suppress, 12 index suppress. */
static int read_logical_long(struct amivm_jit_context *context, uint32_t logical,
                             uint32_t *value)
{
    uint32_t physical[4]; uint8_t byte[4]; unsigned i; int rc;
    if (value == NULL) return -4;
    rc = translate_long(context, logical, 0, physical); if (rc != 0) return rc;
    for (i = 0u; i < 4u; ++i)
        if (!amivm_read8(context->vm, physical[i], &byte[i])) return -4;
    *value = ((uint32_t)byte[0] << 24u) | ((uint32_t)byte[1] << 16u) |
             ((uint32_t)byte[2] << 8u) | (uint32_t)byte[3];
    return 0;
}

static int full_indexed_target(struct amivm_jit_context *context, uint32_t pc_base,
                               uint32_t encoded, uint32_t base_displacement,
                               uint32_t outer_displacement, uint32_t *target)
{
    uint32_t index = 0u, base = 0u, scale, indirect;
    uint32_t index_reg, indirect_mode; int rc;
    if (context == NULL || context->cpu == NULL || target == NULL) return -4;
    if ((encoded & (1u << 11u)) == 0u) {
        if ((encoded & (1u << 7u)) != 0u) base = pc_base;
        else base = context->cpu->a[(encoded >> 8u) & 7u];
    }
    if ((encoded & (1u << 12u)) == 0u) {
        index_reg = encoded & 7u;
        index = (encoded & (1u << 3u)) != 0u ?
            context->cpu->a[index_reg] : context->cpu->d[index_reg];
        if ((encoded & (1u << 4u)) == 0u)
            index = (uint32_t)(int32_t)(int16_t)(index & 0xffffu);
        scale = 1u << ((encoded >> 5u) & 3u);
        index *= scale;
    }
    indirect_mode = (encoded >> 13u) & 3u;
    if (indirect_mode == AMIVM_EA_INDIRECT_NONE) {
        *target = base + base_displacement + index;
        return 0;
    }
    if (indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED) {
        rc = read_logical_long(context, base + base_displacement + index, &indirect);
        if (rc != 0) return rc;
        *target = indirect + outer_displacement;
        return 0;
    }
    if (indirect_mode == AMIVM_EA_INDIRECT_POSTINDEXED) {
        rc = read_logical_long(context, base + base_displacement, &indirect);
        if (rc != 0) return rc;
        *target = indirect + index + outer_displacement;
        return 0;
    }
    return -4;
}

int amivm_jit_helper_jsr_full_indexed(struct amivm_jit_context *context,
                                      uint32_t return_pc, uint32_t encoded,
                                      uint32_t base_displacement,
                                      uint32_t outer_displacement)
{
    uint32_t target; int rc;
    rc = full_indexed_target(context, return_pc - 2u, encoded,
                             base_displacement, outer_displacement, &target);
    if (rc != 0) return rc;
    rc = amivm_jit_helper_stack_push_long(context, return_pc);
    if (rc != 0) return rc;
    context->cpu->pc = target;
    return 0;
}

int amivm_jit_helper_jmp_full_indexed(struct amivm_jit_context *context,
                                      uint32_t pc_base, uint32_t encoded,
                                      uint32_t base_displacement,
                                      uint32_t outer_displacement)
{
    uint32_t target; int rc;
    rc = full_indexed_target(context, pc_base, encoded, base_displacement,
                             outer_displacement, &target);
    if (rc != 0) return rc;
    context->cpu->pc = target;
    return 0;
}
