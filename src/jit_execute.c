#include "jit.h"

int amivm_jit_execute_base(const struct amivm_jit_code *code,
                           struct amivm_cpu_state *cpu);

int amivm_jit_execute_context(const struct amivm_jit_code *code,
                              struct amivm_cpu_state *cpu,
                              struct amivm_jit_context *context)
{
    struct amivm_jit_runtime runtime;
    int rc;

    if (code == NULL || cpu == NULL) return AMIVM_JIT_INVALID;
    if (code->requires_context && context == NULL) return AMIVM_JIT_INVALID;

    if (!code->requires_context)
        return amivm_jit_execute_base(code, cpu);

    amivm_jit_runtime_init(&runtime);
    rc = amivm_jit_runtime_prepare(&runtime, code);
    if (rc != AMIVM_JIT_OK) return rc;
    rc = amivm_jit_runtime_execute_context(&runtime, cpu, context);
    amivm_jit_runtime_release(&runtime);
    return rc;
}

int amivm_jit_execute(const struct amivm_jit_code *code,
                      struct amivm_cpu_state *cpu)
{
    if (code == NULL || cpu == NULL) return AMIVM_JIT_INVALID;
    if (code->requires_context) return AMIVM_JIT_INVALID;
    return amivm_jit_execute_base(code, cpu);
}
