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

struct amivm_cpu_profile {
    enum amivm_cpu_profile_id id;
    const char *name;
    unsigned isa_level;
    bool has_mmu;
    bool has_fpu;
    bool hyper;
};

const struct amivm_cpu_profile *amivm_cpu_profile_default(void);
const struct amivm_cpu_profile *amivm_cpu_profile_by_name(const char *name);
const struct amivm_cpu_profile *amivm_cpu_profile_by_id(enum amivm_cpu_profile_id id);

#endif
