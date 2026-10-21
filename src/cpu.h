#ifndef AMIVM_CPU_H
#define AMIVM_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "fpu.h"

struct amivm_vm;

enum amivm_cpu_fault {
    AMIVM_CPU_FAULT_NONE = 0,
    AMIVM_CPU_FAULT_BUS,
    AMIVM_CPU_FAULT_ADDRESS,
    AMIVM_CPU_FAULT_ILLEGAL,
    AMIVM_CPU_FAULT_PRIVILEGE,
    AMIVM_CPU_FAULT_MMU,
};

enum amivm_cpu_exception_vector {
    AMIVM_VECTOR_BUS_ERROR = 2,
    AMIVM_VECTOR_ADDRESS_ERROR = 3,
    AMIVM_VECTOR_ILLEGAL_INSTRUCTION = 4,
    AMIVM_VECTOR_PRIVILEGE_VIOLATION = 8,
    AMIVM_VECTOR_AUTOVECTOR_BASE = 24,
};

enum amivm_control_register {
    AMIVM_CR_SFC = 0x000,
    AMIVM_CR_DFC = 0x001,
    AMIVM_CR_CACR = 0x002,
    AMIVM_CR_TC = 0x003,
    AMIVM_CR_VBR = 0x801,
    AMIVM_CR_MSP = 0x803,
    AMIVM_CR_ISP = 0x804,
    AMIVM_CR_MMUSR = 0x805,
    AMIVM_CR_URP = 0x806,
    AMIVM_CR_SRP = 0x807,
};

enum amivm_mmu_result {
    AMIVM_MMU_OK = 0,
    AMIVM_MMU_FAULT_ROOT = -1,
    AMIVM_MMU_FAULT_PAGE = -2,
    AMIVM_MMU_FAULT_WRITE_PROTECT = -3,
    AMIVM_MMU_FAULT_TABLE_BUS = -4,
};

/* Bounded per-CPU, opt-in TLB storage; disabled by default. */
#define AMIVM_TLB_ENTRY_COUNT 16u

struct amivm_tlb_entry {
    uint32_t logical_page;
    uint32_t physical_page;
    uint32_t root;
    uint32_t tc;
    uint64_t vm_instance_id;
    uint32_t l1_descriptor_address;
    uint32_t l2_descriptor_address;
    uint64_t l1_generation;
    uint64_t l2_generation;
    bool supervisor;
    bool write_protected;
    bool valid;
};

struct amivm_cpu_state {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t usp;
    uint32_t isp;
    uint32_t msp;
    uint32_t pc;
    uint32_t vbr;
    uint32_t cacr;
    uint32_t tc;
    uint32_t urp;
    uint32_t srp;
    uint32_t mmusr;
    /* M322: translation counters; cache remains disabled by default. */
    uint64_t mmu_translation_walks;
    uint64_t mmu_translation_bypasses;
    bool tlb_enabled;
    uint64_t tlb_hits;
    uint64_t tlb_misses;
    uint64_t tlb_invalidations;
    struct amivm_tlb_entry tlb[AMIVM_TLB_ENTRY_COUNT];
    uint8_t sfc;
    uint8_t dfc;
    uint16_t sr;
    struct amivm_fpu_state fpu;
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
/* Invalidate cached translations after VM replacement or host MMU changes. */
void amivm_mmu_tlb_flush(struct amivm_cpu_state *cpu);
int amivm_mmu_translate(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, bool write, bool supervisor,
                        uint32_t *physical);

#endif
