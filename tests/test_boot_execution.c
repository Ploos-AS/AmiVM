#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

static const unsigned char rom[] = {
    0x10,0x00,0x00,0x00,
    0x00,0xF0,0x00,0x08,
    0x23,0xFC,0x12,0x34,0x56,0x78,
    0x10,0x00,0x00,0x10,
    0x4E,0x75
};

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_boot_config boot = { AMIVM_BOOT_AMIGA_ROM, 0u, 0u, 0u };
    uint32_t value = 0u;

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
    if (amivm_m68k_read_u32(&vm, AMIVM_RAM_BASE + 0x10u, &value) != 0)
        goto fail;
    if (value != 0x12345678u)
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;
fail:
    amivm_vm_destroy(&vm);
    return 1;
}
