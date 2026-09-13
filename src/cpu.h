#ifndef AMIVM_CPU_H
#define AMIVM_CPU_H

#include <stdbool.h>
#include <stdint.h>

struct amivm_vm;

enum amivm_cpu_fault {
    AMIVM_CPU_FAULT_NONE = 0,
    AMIVM_CPU_FAULT_BUS,
    AMIVM_CPU_FAULT_ADDRESS,
    AMIVM_CPU_FAULT_ILLEGAL,
};

enum amivm_cpu_exception_vector {
    AMIVM_VECTOR_BUS_ERROR = 2,
    AMIVM_VECTOR_ADDRESS_ERROR = 3,
    AMIVM_VECTOR_ILLEGAL_INSTRUCTION = 4,
};

struct amivm_cpu_state {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint32_t vbr;
    uint16_t sr;
    bool stopped;
    enum amivm_cpu_fault last_fault;
    uint32_t fault_address;
    uint16_t fault_opcode;
    uint8_t last_exception_vector;
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
