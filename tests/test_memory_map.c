#include "vm.h"

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t value = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (!amivm_m68k_write_u32(&vm, AMIVM_RAM_BASE + 0x100u, 0xA1B2C3D4u))
        goto fail;
    if (!amivm_m68k_read_u32(&vm, AMIVM_RAM_BASE + 0x100u, &value))
        goto fail;
    if (value != 0xA1B2C3D4u)
        goto fail;

    if (amivm_m68k_read_u32(&vm, AMIVM_ROM_BASE, &value))
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;

fail:
    amivm_vm_destroy(&vm);
    return 1;
}
