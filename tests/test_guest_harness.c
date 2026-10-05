#include "guest_harness.h"
#include "vm.h"

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

    amivm_config_init(&config);
    config.ram_size = 10u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (amivm_guest_harness_run_image(&vm, &h, rom, sizeof(rom), &result) != 0) {
        amivm_vm_destroy(&vm);
        return 2;
    }

    if ((result & (AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION)) !=
        (AMIVM_QUAL_RESET | AMIVM_QUAL_EXECUTION)) {
        amivm_vm_destroy(&vm);
        return 3;
    }

    if (!vm.m68k.stopped) {
        amivm_vm_destroy(&vm);
        return 4;
    }

    amivm_vm_destroy(&vm);
    return 0;
}
