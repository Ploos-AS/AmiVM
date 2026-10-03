#include "cpu_profile.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const struct amivm_cpu_profile *p;

    p = amivm_cpu_profile_default();
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER040 && p->hyper);

    p = amivm_cpu_profile_by_name("68020");
    CHECK(p != NULL && p->isa_level == 20u && !p->has_mmu && !p->has_fpu && p->mmu_model == AMIVM_MMU_NONE && p->mmu_maturity == AMIVM_MMU_MATURITY_NONE && p->fpu_model == AMIVM_FPU_NONE && !p->has_master_stack && !p->hyper);

    p = amivm_cpu_profile_by_name("68030");
    CHECK(p != NULL && p->has_mmu && !p->has_fpu && p->mmu_model == AMIVM_MMU_68030 && p->mmu_maturity == AMIVM_MMU_MATURITY_SCAFFOLD && !p->has_master_stack);

    p = amivm_cpu_profile_by_name("68040");
    CHECK(p != NULL && p->has_mmu && p->has_fpu && p->mmu_model == AMIVM_MMU_68040 && p->mmu_maturity == AMIVM_MMU_MATURITY_QUALIFIED && p->fpu_model == AMIVM_FPU_68040 && p->has_master_stack && !p->hyper);

    p = amivm_cpu_profile_by_name("68060");
    CHECK(p != NULL && p->isa_level == 60u && p->has_mmu && p->has_fpu && p->mmu_model == AMIVM_MMU_68060 && p->mmu_maturity == AMIVM_MMU_MATURITY_SCAFFOLD && p->fpu_model == AMIVM_FPU_68060 && p->has_master_stack);

    p = amivm_cpu_profile_by_name("hyper040");
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER040 && p->hyper);

    p = amivm_cpu_profile_by_name("hyper060");
    CHECK(p != NULL && p->id == AMIVM_CPU_HYPER060 && p->hyper);

    CHECK(amivm_cpu_profile_by_name("68000") == NULL);
    CHECK(amivm_cpu_profile_by_name("unknown") == NULL);
    CHECK(amivm_cpu_profile_by_name(NULL) == NULL);

    for (int id = AMIVM_CPU_68020; id <= AMIVM_CPU_HYPER060; ++id) {
        p = amivm_cpu_profile_by_id((enum amivm_cpu_profile_id)id);
        CHECK(p != NULL);
        CHECK((p->id <= AMIVM_CPU_68030) ? p->default_exception_frame_class == 0u : p->default_exception_frame_class == 2u);
        CHECK(p->exception_frame_family ==
              ((p->id == AMIVM_CPU_68020) ? AMIVM_FRAME_FAMILY_68020 :
               (p->id == AMIVM_CPU_68030) ? AMIVM_FRAME_FAMILY_68030 :
               (p->id == AMIVM_CPU_68040 || p->id == AMIVM_CPU_HYPER040) ? AMIVM_FRAME_FAMILY_68040 :
               AMIVM_FRAME_FAMILY_68060));
    }

    {
        const uint8_t expected_frames[][3] = {
            { AMIVM_FRAME_68020_BUS, 0x2u, 6u },
            { AMIVM_FRAME_68030_COPROC_MID, 0x9u, 10u },
            { AMIVM_FRAME_68030_SHORT_BUS, 0xAu, 16u },
            { AMIVM_FRAME_68030_LONG_BUS, 0xBu, 46u },
            { AMIVM_FRAME_68040_ACCESS, 0x7u, 30u },
            { AMIVM_FRAME_68060_ACCESS, 0x4u, 8u }
        };
        for (size_t i = 0; i < sizeof expected_frames / sizeof expected_frames[0]; ++i) {
            const struct amivm_m68k_frame_layout *layout =
                amivm_m68k_frame_layout(expected_frames[i][0]);
            CHECK(layout != NULL && layout->implemented);
            CHECK(amivm_m68k_frame_layout_format(expected_frames[i][0]) == expected_frames[i][1]);
            CHECK(layout->words == expected_frames[i][2]);
        }
    }

    puts("AmiVM M2.83 exception frame family/layout invariants: PASS");
    return 0;
}
