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

    {
        struct frame_case { enum amivm_cpu_profile_id cpu; uint8_t frame; };
        static const struct frame_case supported[] = {
            { AMIVM_CPU_68020, AMIVM_FRAME_68020_BUS },
            { AMIVM_CPU_68030, AMIVM_FRAME_68030_COPROC_MID },
            { AMIVM_CPU_68030, AMIVM_FRAME_68030_SHORT_BUS },
            { AMIVM_CPU_68030, AMIVM_FRAME_68030_LONG_BUS },
            { AMIVM_CPU_68040, AMIVM_FRAME_68040_ACCESS },
            { AMIVM_CPU_HYPER040, AMIVM_FRAME_68040_ACCESS },
            { AMIVM_CPU_68060, AMIVM_FRAME_68060_ACCESS },
            { AMIVM_CPU_HYPER060, AMIVM_FRAME_68060_ACCESS }
        };
        for (size_t i = 0; i < sizeof supported / sizeof supported[0]; ++i) {
            p = amivm_cpu_profile_by_id(supported[i].cpu);
            CHECK(p != NULL);
            CHECK(amivm_m68k_frame_layout(supported[i].frame) != NULL);
            switch (p->exception_frame_family) {
            case AMIVM_FRAME_FAMILY_68020:
                CHECK(supported[i].frame == AMIVM_FRAME_68020_BUS);
                break;
            case AMIVM_FRAME_FAMILY_68030:
                CHECK(supported[i].frame == AMIVM_FRAME_68030_COPROC_MID ||
                      supported[i].frame == AMIVM_FRAME_68030_SHORT_BUS ||
                      supported[i].frame == AMIVM_FRAME_68030_LONG_BUS);
                break;
            case AMIVM_FRAME_FAMILY_68040:
                CHECK(supported[i].frame == AMIVM_FRAME_68040_ACCESS);
                break;
            case AMIVM_FRAME_FAMILY_68060:
                CHECK(supported[i].frame == AMIVM_FRAME_68060_ACCESS);
                break;
            default:
                CHECK(0);
            }
        }
    }

    {
        const enum amivm_cpu_profile_id ids[] = {
            AMIVM_CPU_68020, AMIVM_CPU_68030, AMIVM_CPU_68040,
            AMIVM_CPU_68060, AMIVM_CPU_HYPER040, AMIVM_CPU_HYPER060
        };
        const uint8_t frames[] = {
            AMIVM_FRAME_68020_BUS, AMIVM_FRAME_68030_COPROC_MID,
            AMIVM_FRAME_68030_SHORT_BUS, AMIVM_FRAME_68030_LONG_BUS,
            AMIVM_FRAME_68040_ACCESS, AMIVM_FRAME_68060_ACCESS
        };
        for (size_t i = 0; i < sizeof ids / sizeof ids[0]; ++i) {
            p = amivm_cpu_profile_by_id(ids[i]);
            CHECK(p != NULL);
            for (size_t j = 0; j < sizeof frames / sizeof frames[0]; ++j) {
                bool compatible =
                    (p->exception_frame_family == AMIVM_FRAME_FAMILY_68020 &&
                     frames[j] == AMIVM_FRAME_68020_BUS) ||
                    (p->exception_frame_family == AMIVM_FRAME_FAMILY_68030 &&
                     (frames[j] == AMIVM_FRAME_68030_COPROC_MID ||
                      frames[j] == AMIVM_FRAME_68030_SHORT_BUS ||
                      frames[j] == AMIVM_FRAME_68030_LONG_BUS)) ||
                    (p->exception_frame_family == AMIVM_FRAME_FAMILY_68040 &&
                     frames[j] == AMIVM_FRAME_68040_ACCESS) ||
                    (p->exception_frame_family == AMIVM_FRAME_FAMILY_68060 &&
                     frames[j] == AMIVM_FRAME_68060_ACCESS);
                CHECK(compatible ==
                      ((p->exception_frame_family == AMIVM_FRAME_FAMILY_68020 &&
                        frames[j] == AMIVM_FRAME_68020_BUS) ||
                       (p->exception_frame_family == AMIVM_FRAME_FAMILY_68030 &&
                        (frames[j] == AMIVM_FRAME_68030_COPROC_MID ||
                         frames[j] == AMIVM_FRAME_68030_SHORT_BUS ||
                         frames[j] == AMIVM_FRAME_68030_LONG_BUS)) ||
                       (p->exception_frame_family == AMIVM_FRAME_FAMILY_68040 &&
                        frames[j] == AMIVM_FRAME_68040_ACCESS) ||
                       (p->exception_frame_family == AMIVM_FRAME_FAMILY_68060 &&
                        frames[j] == AMIVM_FRAME_68060_ACCESS)));
            }
        }
    }

    {
        const struct amivm_cpu_profile *base040 = amivm_cpu_profile_by_name("68040");
        const struct amivm_cpu_profile *hyper040 = amivm_cpu_profile_by_name("hyper040");
        const struct amivm_cpu_profile *base060 = amivm_cpu_profile_by_name("68060");
        const struct amivm_cpu_profile *hyper060 = amivm_cpu_profile_by_name("hyper060");

        CHECK(base040 && hyper040 && base060 && hyper060);
        CHECK(hyper040->isa_level == base040->isa_level);
        CHECK(hyper040->has_mmu == base040->has_mmu);
        CHECK(hyper040->has_fpu == base040->has_fpu);
        CHECK(hyper040->mmu_model == base040->mmu_model);
        CHECK(hyper040->fpu_model == base040->fpu_model);
        CHECK(hyper040->has_master_stack == base040->has_master_stack);
        CHECK(hyper040->default_exception_frame_class == base040->default_exception_frame_class);
        CHECK(hyper040->exception_frame_family == base040->exception_frame_family);
        CHECK(hyper040->hyper && !base040->hyper);

        CHECK(hyper060->isa_level == base060->isa_level);
        CHECK(hyper060->has_mmu == base060->has_mmu);
        CHECK(hyper060->has_fpu == base060->has_fpu);
        CHECK(hyper060->mmu_model == base060->mmu_model);
        CHECK(hyper060->fpu_model == base060->fpu_model);
        CHECK(hyper060->has_master_stack == base060->has_master_stack);
        CHECK(hyper060->default_exception_frame_class == base060->default_exception_frame_class);
        CHECK(hyper060->exception_frame_family == base060->exception_frame_family);
        CHECK(hyper060->hyper && !base060->hyper);

        CHECK(amivm_m68k_frame_layout_format(AMIVM_FRAME_68040_ACCESS) == 0x7u);
        CHECK(amivm_m68k_frame_layout_format(AMIVM_FRAME_68060_ACCESS) == 0x4u);
    }

    puts("AmiVM M2.91 Hyper CPU exception-semantic equivalence: PASS");
    return 0;
}
