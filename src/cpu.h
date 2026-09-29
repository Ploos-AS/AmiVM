#ifndef AMIVM_CPU_H
#define AMIVM_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "fpu.h"
#include "cpu_profile.h"

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

enum amivm_pmmu51_register {
    AMIVM_PMMU51_REG_TC = 0,
    AMIVM_PMMU51_REG_CRP,
    AMIVM_PMMU51_REG_SRP,
    AMIVM_PMMU51_REG_PSR,
};

enum amivm_pmmu51_root_type {
    AMIVM_PMMU51_ROOT_INVALID = 0,
    AMIVM_PMMU51_ROOT_PAGE = 1,
    AMIVM_PMMU51_ROOT_TABLE_SHORT = 2,
    AMIVM_PMMU51_ROOT_TABLE_LONG = 3,
};

struct amivm_pmmu51_long_descriptor {
    uint16_t limit;
    bool lower_limit;
    bool shared_globally;
    bool supervisor_only;
    uint8_t read_access_level;
    uint8_t write_access_level;
    uint32_t table_address;
    enum amivm_pmmu51_root_type type;
};

struct amivm_pmmu51_root {
    uint16_t limit;
    bool lower_limit;
    bool shared_globally;
    uint32_t table_address;
    enum amivm_pmmu51_root_type type;
};

enum amivm_pmmu51_status {
    AMIVM_PMMU51_PSR_OK = 0u,
    AMIVM_PMMU51_PSR_ROOT = 1u << 0,
    AMIVM_PMMU51_PSR_PAGE = 1u << 1,
    AMIVM_PMMU51_PSR_WRITE_PROTECT = 1u << 2,
    AMIVM_PMMU51_PSR_TABLE_BUS = 1u << 3,
    AMIVM_PMMU51_PSR_LIMIT = 1u << 4,
    AMIVM_PMMU51_PSR_SUPERVISOR = 1u << 5,
};

enum amivm_mmu_result {
    AMIVM_MMU_OK = 0,
    AMIVM_MMU_FAULT_ROOT = -1,
    AMIVM_MMU_FAULT_PAGE = -2,
    AMIVM_MMU_FAULT_WRITE_PROTECT = -3,
    AMIVM_MMU_FAULT_TABLE_BUS = -4,
    AMIVM_MMU_FAULT_LIMIT = -5,
    AMIVM_MMU_FAULT_SUPERVISOR = -6,
};

struct amivm_cpu_state {
    const struct amivm_cpu_profile *profile;
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
    /* External 68851 PMMU state. Kept separate from 040/060 URP/SRP. */
    uint64_t pmmu_crp;
    uint64_t pmmu_srp;
    uint32_t pmmu_tc;
    uint32_t pmmu_psr;
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
void amivm_cpu_set_profile(struct amivm_cpu_state *cpu,
                           const struct amivm_cpu_profile *profile);
const struct amivm_cpu_profile *amivm_cpu_get_profile(const struct amivm_cpu_state *cpu);
bool amivm_pmmu51_read_register(const struct amivm_cpu_state *cpu,
                                enum amivm_pmmu51_register reg,
                                uint32_t *value);
bool amivm_pmmu51_write_register(struct amivm_cpu_state *cpu,
                                 enum amivm_pmmu51_register reg,
                                 uint32_t value);
bool amivm_pmmu51_read_root(const struct amivm_cpu_state *cpu,
                            enum amivm_pmmu51_register reg,
                            uint64_t *value);
bool amivm_pmmu51_write_root(struct amivm_cpu_state *cpu,
                             enum amivm_pmmu51_register reg,
                             uint64_t value);
bool amivm_pmmu51_decode_root(uint64_t raw, struct amivm_pmmu51_root *root);
bool amivm_pmmu51_decode_long_descriptor(uint64_t raw,
                                         struct amivm_pmmu51_long_descriptor *desc);
int amivm_cpu_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                    const struct amivm_cpu_backend *backend);
int amivm_cpu_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   const struct amivm_cpu_backend *backend);
int amivm_mmu_translate(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, bool write, bool supervisor,
                        uint32_t *physical);

#endif
