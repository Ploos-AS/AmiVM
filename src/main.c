#include "vm.h"
#include "cpu_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMIVM_VERSION "0.1.0-m1"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--version] [--dump-machine] [--selftest] [--cpu PROFILE] [--ram-mib N] [--rom PATH]\n",
            prog);
}

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    int i;
    bool dump_machine = false;
    const struct amivm_cpu_profile *cpu_profile = amivm_cpu_profile_default();

    amivm_config_init(&config);

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("AmiVM %s\n", AMIVM_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--selftest") == 0) {
            return amivm_selftest();
        }
        if (strcmp(argv[i], "--dump-machine") == 0) {
            dump_machine = true;
            continue;
        }
        if (strcmp(argv[i], "--cpu") == 0) {
            if (++i >= argc || (cpu_profile = amivm_cpu_profile_by_name(argv[i])) == NULL) {
                fprintf(stderr, "Invalid --cpu profile (use 68020, 68030, 68040, 68060, hyper040 or hyper060)\n");
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--ram-mib") == 0) {
            if (++i >= argc || !amivm_parse_size_mib(argv[i], &config.ram_size)) {
                fprintf(stderr, "Invalid --ram-mib value\n");
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--rom") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing --rom path\n");
                return 2;
            }
            config.rom_path = argv[i];
            continue;
        }

        usage(argv[0]);
        return 2;
    }

    if (amivm_vm_init(&vm, &config) != 0) {
        fprintf(stderr, "Failed to initialize AmiVM\n");
        return 1;
    }

    if (dump_machine) {
        amivm_dump_machine(&vm, stdout);
    } else {
        puts("AmiVM M1 VM core skeleton initialized");
        printf("CPU: %s\n", cpu_profile->name);
        printf("RAM: %zu MiB\n", vm.ram_size / (1024u * 1024u));
        if (vm.rom_used != 0u) {
            printf("ROM: %zu bytes loaded\n", vm.rom_used);
        }
    }

    amivm_vm_destroy(&vm);
    return 0;
}
