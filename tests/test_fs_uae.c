#include "fs_uae.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const char *path = "/tmp/amivm-fsuae-test.conf";
    FILE *f = fopen(path, "w");
    struct amivm_config config;
    struct amivm_fsuae_report report;

    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fputs("cpu=68040\n", f);
    fputs("chip_memory=2\n", f);
    fputs("fast_memory=32\n", f);
    fputs("uae_cpu_speed=real\n", f);
    fputs("uae_cpu_multiplier=2\n", f);
    fputs("fullscreen=1\n", f);
    fputs("jit_compiler=1\n", f);
    fputs("bsdsocket_library=1\n", f);
    fputs("mystery_option=1\n", f);
    fclose(f);

    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.cpu_profile != NULL);
    CHECK(strcmp(config.cpu_profile->name, "68040") == 0);
    CHECK(config.ram_size == 34u * 1024u * 1024u);
    CHECK(report.ignored_speed == 2u);
    CHECK(report.ignored_host == 1u);
    CHECK(report.unsupported == 1u);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(strcmp(config.cpu_profile->name, "68020") == 0);
    CHECK(report.supported == 1u);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A500\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(report.unsupported == 1u);

    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, true) != 0);

    remove(path);
    puts("AmiVM M2.141 FS-UAE compatibility qualification: PASS");
    return 0;
}
