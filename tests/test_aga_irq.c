#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

static const unsigned char rom[] = {
    0x10,0x00,0x00,0x00,
    0x00,0xF0,0x00,0x08,
    0x13,0xFC,0x00,0x01,       /* enable VBlank IRQ */
    0xFF,0x00,0x50,0x0D,
    0x4E,0x71
};

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_boot_config boot = { AMIVM_BOOT_AMIGA_ROM, 0u, 0u, 0u };
    unsigned i;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;
    if (amivm_rom_install(&vm, rom, sizeof(rom)) != 0)
        goto fail;
    if (amivm_boot_prepare(&vm, &boot) != 0)
        goto fail;
    if (amivm_boot_step(&vm, 1u) != 0)
        goto fail;

    for (i = 0u; i < 400000u && !vm.irq_pending; ++i) {
        if (amivm_vm_step(&vm) != 0)
            goto fail;
    }

    if (!vm.irq_pending || vm.irq_level != 3u)
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;
fail:
    amivm_vm_destroy(&vm);
    return 1;
}
