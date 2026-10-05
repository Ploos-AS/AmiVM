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
        AMIVM_QUAL_RESET | AMIVM_QUAL_OS_DETECTED |
        AMIVM_QUAL_FILESYSTEM | AMIVM_QUAL_SHELL |
        AMIVM_QUAL_INTERRUPTS | AMIVM_QUAL_DEVICE_IO |
        AMIVM_QUAL_WORKLOAD
    };
    uint32_t result = 0u;

    amivm_config_init(&config);
    config.ram_size = 10u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (amivm_guest_harness_run(&vm, &h, &result) != 0) {
        amivm_vm_destroy(&vm);
        return 2;
    }

    if ((result & AMIVM_QUAL_RESET) == 0u) {
        amivm_vm_destroy(&vm);
        return 3;
    }

    amivm_vm_destroy(&vm);
    return 0;
}
