#include "cpu_profile.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const struct amivm_cpu_profile *p;

    p = amivm_cpu_profile_default();
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER040 && p->hyper);

    p = amivm_cpu_profile_by_name("68020");
    CHECK(p != NULL && p->isa_level == 20u && !p->has_mmu && !p->has_fpu && !p->hyper);

    p = amivm_cpu_profile_by_name("68030");
    CHECK(p != NULL && p->has_mmu && !p->has_fpu);

    p = amivm_cpu_profile_by_name("68040");
    CHECK(p != NULL && p->has_mmu && p->has_fpu && !p->hyper);

    p = amivm_cpu_profile_by_name("68060");
    CHECK(p != NULL && p->isa_level == 60u && p->has_mmu && p->has_fpu);

    p = amivm_cpu_profile_by_name("hyper040");
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER040 && p->hyper);

    p = amivm_cpu_profile_by_name("hyper060");
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER060 && p->hyper);

    CHECK(amivm_cpu_profile_by_name("68000") == NULL);
    CHECK(amivm_cpu_profile_by_name("unknown") == NULL);
    CHECK(amivm_cpu_profile_by_name(NULL) == NULL);

    puts("AmiVM M2.81 CPU profile capability tests: PASS");
    return 0;
}
