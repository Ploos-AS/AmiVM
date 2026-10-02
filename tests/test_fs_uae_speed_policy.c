#include "fs_uae.h"
#include "cpu_profile.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const char *path = "m2_162.fs-uae.conf";
    FILE *f = fopen(path, "w");
    struct amivm_config config;
    struct amivm_fsuae_report report;

    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fputs("cpu=68020\n", f);
    fputs("uae_cpu_speed=max\n", f);
    fputs("uae_cpu_multiplier=0\n", f);
    fputs("uae_cpu_frequency=7\n", f);
    fputs("uae_cpu_throttle=true\n", f);
    fputs("warp_mode=false\n", f);
    fputs("fullscreen=true\n", f);
    fputs("chip_memory=2M\n", f);
    fclose(f);

    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, true) == 0);
    CHECK(config.cpu_profile == amivm_cpu_profile_by_name("68020"));
    CHECK(report.ignored_speed == 5u);
    CHECK(report.ignored_host == 1u);
    CHECK(report.unsupported == 0u);
    CHECK(report.malformed == 0u);
    CHECK(config.ram_size == 2u * 1024u * 1024u);

    remove(path);
    puts("AmiVM M2.162 FS-UAE speed-policy qualification: PASS");
    return 0;
}
