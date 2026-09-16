#ifndef AMIVM_JIT_H
#define AMIVM_JIT_H

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "ir.h"

#define AMIVM_JIT_CODE_CAPACITY 512u

enum amivm_jit_arch {
    AMIVM_JIT_ARCH_NONE = 0,
    AMIVM_JIT_ARCH_X86_64,
};

enum amivm_jit_result {
    AMIVM_JIT_OK = 0,
    AMIVM_JIT_UNSUPPORTED = 1,
    AMIVM_JIT_NO_SPACE = -1,
    AMIVM_JIT_INVALID = -2,
    AMIVM_JIT_EXEC_UNAVAILABLE = -3,
};

struct amivm_jit_code {
    enum amivm_jit_arch arch;
    uint8_t bytes[AMIVM_JIT_CODE_CAPACITY];
    size_t size;
    size_t guest_instructions;
    uint32_t guest_start_pc;
    uint32_t guest_end_pc;
    int requires_context;
};

struct amivm_jit_runtime {
    void *mapping;
    size_t mapping_size;
    int requires_context;
};

struct amivm_jit_context;

int amivm_jit_host_arch(void);
void amivm_jit_code_init(struct amivm_jit_code *code);
int amivm_jit_compile(const struct amivm_ir_block *block,
                      struct amivm_jit_code *code);
/* Legacy CPU-only direct entry. Context-requiring code is rejected. */
int amivm_jit_execute(const struct amivm_jit_code *code,
                      struct amivm_cpu_state *cpu);
/* Safe direct entry for generated code that requires VM-aware helpers. */
int amivm_jit_execute_context(const struct amivm_jit_code *code,
                              struct amivm_cpu_state *cpu,
                              struct amivm_jit_context *context);

void amivm_jit_runtime_init(struct amivm_jit_runtime *runtime);
int amivm_jit_runtime_prepare(struct amivm_jit_runtime *runtime,
                              const struct amivm_jit_code *code);
/* Legacy CPU-only entry point retained for qualification/backward compatibility.
   Context-requiring blocks are rejected rather than entered with NULL context. */
int amivm_jit_runtime_execute(const struct amivm_jit_runtime *runtime,
                              struct amivm_cpu_state *cpu);
/* Native entry ABI: RDI=cpu, RSI=context on x86-64 SysV. */
int amivm_jit_runtime_execute_context(const struct amivm_jit_runtime *runtime,
                                      struct amivm_cpu_state *cpu,
                                      struct amivm_jit_context *context);
void amivm_jit_runtime_release(struct amivm_jit_runtime *runtime);

#endif
