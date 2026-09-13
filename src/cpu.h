#ifndef AMIVM_CPU_H
#define AMIVM_CPU_H

#include <stdbool.h>
#include <stdint.h>

struct amivm_vm;

struct amivm_cpu_state {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
    bool stopped;
};

struct amivm_cpu_backend {
    const char *name;
    int (*reset)(struct amivm_cpu_state *cpu, struct amivm_vm *vm);
    int (*step)(struct amivm_cpu_state *cpu, struct amivm_vm *vm);
};

const struct amivm_cpu_backend *amivm_cpu_reference_backend(void);
int amivm_cpu_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                    const struct amivm_cpu_backend *backend);
int amivm_cpu_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   const struct amivm_cpu_backend *backend);

#endif
