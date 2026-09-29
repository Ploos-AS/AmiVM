#ifndef AMIVM_CPU_PROFILE_H
#define AMIVM_CPU_PROFILE_H

#include <stdbool.h>

enum amivm_cpu_profile_id {
    AMIVM_CPU_68020 = 0,
    AMIVM_CPU_68030,
    AMIVM_CPU_68040,
    AMIVM_CPU_68060,
    AMIVM_CPU_HYPER040,
    AMIVM_CPU_HYPER060,
};

enum amivm_mmu_model {
    AMIVM_MMU_NONE = 0,
    AMIVM_MMU_68030,
    AMIVM_MMU_68040,
    AMIVM_MMU_68060,
};

enum amivm_fpu_model {
    AMIVM_FPU_NONE = 0,
    AMIVM_FPU_68040,
    AMIVM_FPU_68060,
};

struct amivm_cpu_profile {
    enum amivm_cpu_profile_id id;
    const char *name;
    unsigned isa_level;
    bool has_mmu;
    bool has_fpu;
    enum amivm_mmu_model mmu_model;
    enum amivm_fpu_model fpu_model;
    bool has_master_stack;
    bool hyper;
};

const struct amivm_cpu_profile *amivm_cpu_profile_default(void);
const struct amivm_cpu_profile *amivm_cpu_profile_by_name(const char *name);
const struct amivm_cpu_profile *amivm_cpu_profile_by_id(enum amivm_cpu_profile_id id);

#endif
