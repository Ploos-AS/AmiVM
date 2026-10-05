#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

static const unsigned char rom[] = {
    0x10,0x00,0x10,0x00, /* SP = $00100000 */
    0x00,0x00,0x01,0x00, /* PC = $00000100 */
    0x21,0xFC,0x00,0x00,0x12,0x34,0x56,0x78, /* MOVE.L #$12345678,$00100010 */
    0x00,0x10,0x00,0x10,
    0x4E,0x75              /* RTS */
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
    if (amivm_rom_install(&vm, rom, sizeof(rom)) != 0)
        goto fail;
    if (amivm_boot_prepare(&vm, &boot) != 0)
        goto fail;

    /* The smoke image is copied to the VM ROM address space at $00000000. */
    if (amivm_boot_step(&vm, 2u) != 0)
        goto fail;

    /* The test is intentionally about execution reaching the instruction stream. */
    if (vm.m68k.pc == 0x00000100u) {
        amivm_vm_destroy(&vm);
        return 2;
    }

    amivm_vm_destroy(&vm);
    return 0;

fail:
    amivm_vm_destroy(&vm);
    return 1;
}
