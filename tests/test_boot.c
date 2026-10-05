#include "boot.h"
#include "vm.h"

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_boot_config boot = {
        AMIVM_BOOT_AMIGA_ROM,
        AMIVM_ROM_BASE,
        AMIVM_RAM_BASE + 0x1000u,
        0x2000u
    };

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (amivm_boot_prepare(&vm, &boot) != 0) {
        amivm_vm_destroy(&vm);
        return 2;
    }

    if (vm.m68k.pc != AMIVM_ROM_BASE ||
        vm.m68k.a[7] != AMIVM_RAM_BASE + 0x1000u ||
        vm.m68k.sr != 0x2000u) {
        amivm_vm_destroy(&vm);
        return 3;
    }

    amivm_vm_destroy(&vm);
    return 0;
}
