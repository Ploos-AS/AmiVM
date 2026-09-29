#include "cpu.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

int main(void)
{
    struct amivm_cpu_state cpu;
    struct amivm_cpu_profile p;
    uint32_t value=0u;

    memset(&cpu,0,sizeof cpu);
    CHECK(amivm_cpu_profile_attach_mmu(&p,amivm_cpu_profile_by_name("68020"),AMIVM_MMU_68851));
    amivm_cpu_set_profile(&cpu,&p);

    CHECK(amivm_pmmu51_write_register(&cpu,AMIVM_PMMU51_REG_TC,0x80000000u));
    CHECK(amivm_pmmu51_write_register(&cpu,AMIVM_PMMU51_REG_CRP,0x10002000u));
    CHECK(amivm_pmmu51_write_register(&cpu,AMIVM_PMMU51_REG_SRP,0x10004000u));
    CHECK(amivm_pmmu51_read_register(&cpu,AMIVM_PMMU51_REG_TC,&value) && value==0x80000000u);
    CHECK(amivm_pmmu51_read_register(&cpu,AMIVM_PMMU51_REG_CRP,&value) && value==0x10002000u);
    CHECK(amivm_pmmu51_read_register(&cpu,AMIVM_PMMU51_REG_SRP,&value) && value==0x10004000u);

    cpu.pmmu_psr=AMIVM_PMMU51_PSR_PAGE;
    CHECK(amivm_pmmu51_read_register(&cpu,AMIVM_PMMU51_REG_PSR,&value));
    CHECK(value==AMIVM_PMMU51_PSR_PAGE);
    CHECK(!amivm_pmmu51_write_register(&cpu,AMIVM_PMMU51_REG_PSR,0u));

    amivm_cpu_set_profile(&cpu,amivm_cpu_profile_by_name("68040"));
    CHECK(!amivm_pmmu51_read_register(&cpu,AMIVM_PMMU51_REG_TC,&value));
    CHECK(!amivm_pmmu51_write_register(&cpu,AMIVM_PMMU51_REG_TC,0u));

    puts("AmiVM M2.91 68851 PMOVE register foundation: PASS");
    return 0;
}
