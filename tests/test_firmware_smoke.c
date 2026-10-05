#include "firmware.h"
#include "vm.h"

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_firmware_state state;
    unsigned i;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (amivm_firmware_init(&vm, &state) != 0)
        goto fail;

    for (i = 0u; i < 8u && !amivm_firmware_ready(&state); ++i)
        if (amivm_firmware_step(&vm, &state) != 0)
            goto fail;

    if (!amivm_firmware_ready(&state))
        goto fail;
    if (state.vector_base != AMIVM_RAM_BASE + 0x400u)
        goto fail;
    if (state.ram_size != 2u * 1024u * 1024u)
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;

fail:
    amivm_vm_destroy(&vm);
    return 1;
}
