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
};

struct amivm_jit_runtime {
    void *mapping;
    size_t mapping_size;
};

int amivm_jit_host_arch(void);
void amivm_jit_code_init(struct amivm_jit_code *code);
int amivm_jit_compile(const struct amivm_ir_block *block,
                      struct amivm_jit_code *code);
int amivm_jit_execute(const struct amivm_jit_code *code,
                      struct amivm_cpu_state *cpu);

void amivm_jit_runtime_init(struct amivm_jit_runtime *runtime);
int amivm_jit_runtime_prepare(struct amivm_jit_runtime *runtime,
                              const struct amivm_jit_code *code);
int amivm_jit_runtime_execute(const struct amivm_jit_runtime *runtime,
                              struct amivm_cpu_state *cpu);
void amivm_jit_runtime_release(struct amivm_jit_runtime *runtime);

#endif
