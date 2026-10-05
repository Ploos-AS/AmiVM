#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

static const unsigned char rom[] = {
    0x10,0x00,0x00,0x00,
    0x00,0x10,0x00,0x20,
    0x4E,0x71,
    0x4E,0x75
};

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_boot_config boot = {
        AMIVM_BOOT_AMIGA_ROM, 0u, 0u, 0u
    };

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;
    if (amivm_rom_install(&vm, rom, sizeof(rom)) != 0) {
        amivm_vm_destroy(&vm);
        return 2;
    }
    if (amivm_boot_prepare(&vm, &boot) != 0) {
        amivm_vm_destroy(&vm);
        return 3;
    }
    if (vm.m68k.pc != 0x00100020u || vm.m68k.a[7] != 0x10000000u) {
        amivm_vm_destroy(&vm);
        return 4;
    }
    amivm_vm_destroy(&vm);
    return 0;
}
