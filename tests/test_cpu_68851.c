#include "cpu_profile.h"

#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const struct amivm_cpu_profile *base=amivm_cpu_profile_by_name("68020");
    const struct amivm_cpu_profile *p30=amivm_cpu_profile_by_name("68030");
    struct amivm_cpu_profile composed;

    CHECK(base != NULL);
    CHECK(!base->has_mmu && base->mmu_model == AMIVM_MMU_NONE);
    CHECK(amivm_cpu_profile_attach_mmu(&composed,base,AMIVM_MMU_68851));
    CHECK(composed.id == AMIVM_CPU_68020);
    CHECK(composed.isa_level == 20u);
    CHECK(composed.has_mmu);
    CHECK(composed.mmu_model == AMIVM_MMU_68851);
    CHECK(composed.mmu_maturity == AMIVM_MMU_MATURITY_SCAFFOLD);
    CHECK(!composed.has_fpu);
    CHECK(!composed.hyper);

    CHECK(!amivm_cpu_profile_attach_mmu(&composed,base,AMIVM_MMU_68030));
    CHECK(!amivm_cpu_profile_attach_mmu(&composed,p30,AMIVM_MMU_68851));
    CHECK(!amivm_cpu_profile_attach_mmu(NULL,base,AMIVM_MMU_68851));

    puts("AmiVM M2.86 68020+68851 composition: PASS");
    return 0;
}
