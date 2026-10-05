#include "vm.h"

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    uint32_t vector_base = AMIVM_RAM_BASE + 0x400u;
    const uint32_t original_pc = 0x10000200u;
    const uint16_t original_sr = 0x2000u;
    const uint32_t handler = 0x10000100u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    vm.exception_vector_base = vector_base;
    if (!amivm_m68k_write_u32(&vm, vector_base + 27u * 4u, handler))
        goto fail;

    vm.m68k.pc = original_pc;
    vm.m68k.sr = original_sr;
    vm.m68k.supervisor = true;
    vm.irq_level = 3u;
    vm.irq_pending = true;

    if (amivm_m68k_acknowledge_irq(&vm, &vector) != 0)
        goto fail;
    if (vector != 27u || vm.m68k.pc != handler || !vm.irq_in_service)
        goto fail;

    if (amivm_m68k_return_from_interrupt(&vm) != 0)
        goto fail;

    if (vm.m68k.pc != original_pc ||
        vm.m68k.sr != original_sr ||
        vm.irq_in_service ||
        vm.exception_frame_active)
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;

fail:
    amivm_vm_destroy(&vm);
    return 1;
}
