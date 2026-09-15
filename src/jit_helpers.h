#ifndef AMIVM_JIT_HELPERS_H
#define AMIVM_JIT_HELPERS_H

#include <stdint.h>

#include "cpu.h"
#include "vm.h"

/* Stable C-call boundary for native JIT code that needs guest memory/VM state.
   Helpers return 0 on success and a negative execution fault code on failure. */
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

#endif
