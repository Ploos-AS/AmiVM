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
/* M2.48 absolute control flow. */
int amivm_jit_helper_jmp_abs(struct amivm_jit_context *context,
                             uint32_t target_pc);

#endif
