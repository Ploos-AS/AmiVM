#include "guest_harness.h"
#include "vm.h"

#include <stdio.h>

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_guest_harness h = {
        "aros-m68k",
        "external/aros-m68k/rom.bin",
        "external/aros-m68k/system.hdf",
        30000u,
        100000000u,
        AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION
    };
    static const unsigned char rom[] = {
        0x00,0x00,0x10,0x00,
        0x00,0xF0,0x00,0x08,
        0x4E,0x71,
        0x4E,0x72,0x27,0x00
    };
    uint32_t result = 0u;
    FILE *disk;

    amivm_config_init(&config);
    config.ram_size = 10u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    disk = fopen("amivm-guest-test.img", "wb");
    if (!disk) { amivm_vm_destroy(&vm); return 2; }
    fputc(0x41, disk);
    fclose(disk);
    h.disk_path = "amivm-guest-test.img";
    if (amivm_guest_harness_run_external(&vm, &h, &result) != 0) {
        remove("amivm-guest-test.img");
        amivm_vm_destroy(&vm);
        return 2;
    }

    if (vm.hard_drive[0].type != AMIVM_MEDIA_PATH) {
        remove("amivm-guest-test.img");
        amivm_vm_destroy(&vm);
        return 3;
    }

    if ((result & (AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION)) !=
        (AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION)) {
        amivm_vm_destroy(&vm);
        remove("amivm-guest-test.img");
        return 4;
    }

    if (!vm.m68k.stopped) {
        amivm_vm_destroy(&vm);
        remove("amivm-guest-test.img");
        return 5;
    }

    remove("amivm-guest-test.img");
    amivm_vm_destroy(&vm);
    return 0;
}
