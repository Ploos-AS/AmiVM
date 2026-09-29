#include "cpu_profile.h"

#include <stddef.h>
#include <string.h>

static const struct amivm_cpu_profile profiles[] = {
    { AMIVM_CPU_68020, "68020", 20u, false, false, AMIVM_MMU_NONE,  AMIVM_MMU_MATURITY_NONE,      AMIVM_FPU_NONE,  false, false },
    { AMIVM_CPU_68030, "68030", 30u, true,  false, AMIVM_MMU_68030, AMIVM_MMU_MATURITY_SCAFFOLD,  AMIVM_FPU_NONE,  false, false },
    { AMIVM_CPU_68040, "68040", 40u, true,  true,  AMIVM_MMU_68040, AMIVM_MMU_MATURITY_QUALIFIED, AMIVM_FPU_68040, true,  false },
    { AMIVM_CPU_68060, "68060", 60u, true,  true,  AMIVM_MMU_68060, AMIVM_MMU_MATURITY_SCAFFOLD,  AMIVM_FPU_68060, true,  false },
    { AMIVM_CPU_HYPER040, "hyper040", 40u, true, true, AMIVM_MMU_68040, AMIVM_MMU_MATURITY_QUALIFIED, AMIVM_FPU_68040, true, true },
    { AMIVM_CPU_HYPER060, "hyper060", 60u, true, true, AMIVM_MMU_68060, AMIVM_MMU_MATURITY_SCAFFOLD,  AMIVM_FPU_68060, true, true },
};

const struct amivm_cpu_profile *amivm_cpu_profile_default(void)
{
    return amivm_cpu_profile_by_id(AMIVM_CPU_HYPER040);
}

const struct amivm_cpu_profile *amivm_cpu_profile_by_id(enum amivm_cpu_profile_id id)
{
    size_t i;
    for (i = 0; i < sizeof profiles / sizeof profiles[0]; ++i)
        if (profiles[i].id == id) return &profiles[i];
    return NULL;
}

const struct amivm_cpu_profile *amivm_cpu_profile_by_name(const char *name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof profiles / sizeof profiles[0]; ++i)
        if (strcmp(profiles[i].name, name) == 0) return &profiles[i];
    return NULL;
}


bool amivm_cpu_profile_attach_mmu(struct amivm_cpu_profile *out,
                                  const struct amivm_cpu_profile *base,
                                  enum amivm_mmu_model mmu_model)
{
    if (out == NULL || base == NULL) return false;
    if (base->has_mmu || mmu_model != AMIVM_MMU_68851) return false;
    if (base->id != AMIVM_CPU_68020) return false;

    *out = *base;
    out->has_mmu = true;
    out->mmu_model = AMIVM_MMU_68851;
    out->mmu_maturity = AMIVM_MMU_MATURITY_SCAFFOLD;
    return true;
}
