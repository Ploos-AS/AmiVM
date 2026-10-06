#include "vm.h"
#include "cpu_profile.h"
#include "fs_uae.h"
#include "guest_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMIVM_VERSION "0.1.0-m1"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--version] [--dump-machine] [--config FILE] [--config-report] [--strict-config] [--selftest] [--cpu PROFILE] [--mmu 68851] [--ram-mib N] [--rom PATH] [--guest-aros ROM DISK] [--guest-max-instructions N]\n",
            prog);
}

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    int i;
    bool dump_machine = false;
    bool config_report = false;
    bool strict_config = false;
    const char *config_path = NULL;
    struct amivm_fsuae_report fs_report;
    const struct amivm_cpu_profile *cpu_profile;
    const char *guest_rom = NULL;
    const char *guest_disk = NULL;
    uint64_t guest_max_instructions = 100000000u;

    amivm_config_init(&config);
    cpu_profile = config.cpu_profile;
    memset(&fs_report, 0, sizeof fs_report);

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0 || strncmp(argv[i], "--config=", 9u) == 0) {
            if (argv[i][8] == '=')
                config_path = argv[i] + 9u;
            else if (++i < argc)
                config_path = argv[i];
            else {
                fprintf(stderr, "Missing --config path\n");
                return 2;
            }
            if (amivm_fsuae_load_config(config_path, &config, &fs_report,
                                        strict_config) != 0) {
                fprintf(stderr, "Failed to import FS-UAE config: %s\n", config_path);
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--config-report") == 0) {
            config_report = true;
            continue;
        }
        if (strcmp(argv[i], "--strict-config") == 0) {
            strict_config = true;
            continue;
        }
        if (strcmp(argv[i], "--guest-aros") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Usage: --guest-aros ROM DISK\n");
                return 2;
            }
            guest_rom = argv[++i];
            guest_disk = argv[++i];
            config.cpu_profile = amivm_cpu_profile_by_name("68020");
            config.ram_size = 10u * 1024u * 1024u;
            continue;
        }
        if (strcmp(argv[i], "--guest-max-instructions") == 0) {
            char *end = NULL;
            unsigned long long value;
            if (++i >= argc) {
                fprintf(stderr, "Missing --guest-max-instructions value\n");
                return 2;
            }
            value = strtoull(argv[i], &end, 0);
            if (!end || *end != '\0' || value == 0u) {
                fprintf(stderr, "Invalid --guest-max-instructions value\n");
                return 2;
            }
            guest_max_instructions = (uint64_t)value;
            continue;
        }
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
        if (strncmp(argv[i], "--cpu=", 6u) == 0) {
            cpu_profile = amivm_cpu_profile_by_name(argv[i] + 6u);
            if (cpu_profile == NULL) {
                fprintf(stderr, "Invalid --cpu profile (use 68020, 68030, 68040, 68060, hyper040 or hyper060)\n");
                return 2;
            }
            config.cpu_profile = cpu_profile;
            continue;
        }
        if (strcmp(argv[i], "--mmu=68851") == 0) {
            config.external_mmu = AMIVM_MMU_68851;
            continue;
        }
        if (strncmp(argv[i], "--mmu=", 6u) == 0) {
            fprintf(stderr, "Invalid --mmu value (currently only 68851 is supported)\n");
            return 2;
        }
        if (strcmp(argv[i], "--cpu") == 0) {
            if (++i >= argc || (cpu_profile = amivm_cpu_profile_by_name(argv[i])) == NULL) {
                fprintf(stderr, "Invalid --cpu profile (use 68020, 68030, 68040, 68060, hyper040 or hyper060)\n");
                return 2;
            }
            config.cpu_profile = cpu_profile;
            continue;
        }
        if (strcmp(argv[i], "--mmu") == 0) {
            if (++i >= argc || strcmp(argv[i], "68851") != 0) {
                fprintf(stderr, "Invalid --mmu value (currently only 68851 is supported)\n");
                return 2;
            }
            config.external_mmu = AMIVM_MMU_68851;
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

    if (config_report)
        amivm_fsuae_report(&fs_report, stdout);

    if (amivm_vm_init(&vm, &config) != 0) {
        fprintf(stderr, "Failed to initialize AmiVM\n");
        return 1;
    }

    if (guest_rom != NULL) {
        struct amivm_guest_harness harness = {
            "aros-m68k", guest_rom, guest_disk, 30000u,
            guest_max_instructions,
            AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION |
            AMIVM_QUAL_OS_DETECTED | AMIVM_QUAL_FILESYSTEM |
            AMIVM_QUAL_SHELL,
            "AROS", "Workbench", "Shell", AMIVM_SERIAL_LOG_SIZE
        };
        uint32_t result_mask = 0u;
        int rc = amivm_guest_harness_run_external(&vm, &harness, &result_mask);
        printf("\nGuest profile: %s\n", harness.profile);
        printf("Qualification mask: 0x%08x\n", result_mask);
        printf("Qualification complete: %s\n",
               amivm_guest_harness_qualification_complete(&harness, result_mask)
                   ? "PASS" : "NOT QUALIFIED");
        if (!amivm_guest_harness_qualification_complete(&harness, result_mask)) {
            char diagnostic[1024];
            if (amivm_guest_harness_format_failure(&vm, &harness, result_mask,
                                                    diagnostic, sizeof diagnostic) == 0)
                printf("Guest diagnostic: %s\n", diagnostic);
        }
        amivm_vm_destroy(&vm);
        return rc == 0 ? 0 : 1;
    } else if (dump_machine) {
        amivm_dump_machine(&vm, stdout);
    } else {
        puts("AmiVM M1 VM core skeleton initialized");
        printf("CPU: %s\n", vm.cpu_profile.name);
        if (vm.cpu_profile.mmu_model == AMIVM_MMU_68851)
            puts("MMU: 68851");
        printf("RAM: %zu MiB\n", vm.ram_size / (1024u * 1024u));
        if (vm.rom_used != 0u) {
            printf("ROM: %zu bytes loaded\n", vm.rom_used);
        }
    }

    amivm_vm_destroy(&vm);
    return 0;
}
