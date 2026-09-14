#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "jit.h"

#include <string.h>

#if defined(__x86_64__) && defined(__linux__)
#include <sys/mman.h>
#endif

void amivm_jit_runtime_init(struct amivm_jit_runtime *runtime)
{
    if (runtime == NULL) return;
    runtime->mapping = NULL;
    runtime->mapping_size = 0u;
}

int amivm_jit_runtime_prepare(struct amivm_jit_runtime *runtime,
                              const struct amivm_jit_code *code)
{
#if defined(__x86_64__) && defined(__linux__)
    void *mapping;

    if (runtime == NULL || code == NULL || code->size == 0u ||
        code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_INVALID;

    amivm_jit_runtime_release(runtime);
    mapping = mmap(NULL, code->size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return AMIVM_JIT_EXEC_UNAVAILABLE;
    memcpy(mapping, code->bytes, code->size);
    if (mprotect(mapping, code->size, PROT_READ | PROT_EXEC) != 0) {
        (void)munmap(mapping, code->size);
        return AMIVM_JIT_EXEC_UNAVAILABLE;
    }
    runtime->mapping = mapping;
    runtime->mapping_size = code->size;
    return AMIVM_JIT_OK;
#else
    (void)runtime;
    (void)code;
    return AMIVM_JIT_EXEC_UNAVAILABLE;
#endif
}

int amivm_jit_runtime_execute(const struct amivm_jit_runtime *runtime,
                              struct amivm_cpu_state *cpu)
{
#if defined(__x86_64__) && defined(__linux__)
    typedef int (*jit_fn)(struct amivm_cpu_state *);
    void *entry;
    jit_fn fn;

    if (runtime == NULL || cpu == NULL || runtime->mapping == NULL ||
        runtime->mapping_size == 0u) return AMIVM_JIT_INVALID;
    if (sizeof(fn) != sizeof(entry)) return AMIVM_JIT_EXEC_UNAVAILABLE;

    entry = runtime->mapping;
    memcpy(&fn, &entry, sizeof(fn));
    return fn(cpu);
#else
    (void)runtime;
    (void)cpu;
    return AMIVM_JIT_EXEC_UNAVAILABLE;
#endif
}

void amivm_jit_runtime_release(struct amivm_jit_runtime *runtime)
{
    if (runtime == NULL) return;
#if defined(__x86_64__) && defined(__linux__)
    if (runtime->mapping != NULL && runtime->mapping_size != 0u)
        (void)munmap(runtime->mapping, runtime->mapping_size);
#endif
    runtime->mapping = NULL;
    runtime->mapping_size = 0u;
}
