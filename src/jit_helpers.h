#ifndef AMIVM_JIT_HELPERS_H
#define AMIVM_JIT_HELPERS_H

#include <stdint.h>

#include "cpu.h"
#include "vm.h"

struct amivm_jit_context {
    struct amivm_cpu_state *cpu;
    struct amivm_vm *vm;
};

void amivm_jit_context_init(struct amivm_jit_context *context,
                            struct amivm_cpu_state *cpu,
                            struct amivm_vm *vm);
int amivm_jit_helper_stack_push_long(struct amivm_jit_context *context,
                                     uint32_t value);
int amivm_jit_helper_stack_pop_long(struct amivm_jit_context *context,
                                    uint32_t *value);
int amivm_jit_helper_bsr(struct amivm_jit_context *context,
                         uint32_t return_pc, uint32_t target_pc);
int amivm_jit_helper_rts(struct amivm_jit_context *context);
int amivm_jit_helper_jsr_an(struct amivm_jit_context *context,
                            uint32_t return_pc, uint32_t address_register);
int amivm_jit_helper_jmp_an(struct amivm_jit_context *context,
                            uint32_t address_register);
int amivm_jit_helper_jmp_abs(struct amivm_jit_context *context,
                             uint32_t target_pc);
/* M2.50 register-base displacement control flow. */
int amivm_jit_helper_jsr_d16_an(struct amivm_jit_context *context,
                                uint32_t return_pc, uint32_t encoded);
int amivm_jit_helper_jmp_d16_an(struct amivm_jit_context *context,
                                uint32_t encoded);
/* M2.52 brief indexed control flow. encoded carries EA/index metadata. */
int amivm_jit_helper_jsr_indexed(struct amivm_jit_context *context,
                                 uint32_t return_pc, uint32_t encoded);
int amivm_jit_helper_jmp_indexed(struct amivm_jit_context *context,
                                 uint32_t pc_base, uint32_t encoded);
/* M2.54 non-indirect 68020+ full indexed control flow. */
int amivm_jit_helper_jsr_full_indexed(struct amivm_jit_context *context,
                                      uint32_t return_pc, uint32_t encoded,
                                      uint32_t base_displacement);
int amivm_jit_helper_jmp_full_indexed(struct amivm_jit_context *context,
                                      uint32_t pc_base, uint32_t encoded,
                                      uint32_t base_displacement);

#endif
