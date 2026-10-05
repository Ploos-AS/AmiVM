#include "firmware.h"
#include "vm.h"

int amivm_firmware_init(struct amivm_vm *vm,
                        struct amivm_firmware_state *state)
{
    if (!vm || !state)
        return 1;

    state->stage = AMIVM_FIRMWARE_RESET;
    state->vector_base = AMIVM_RAM_BASE + 0x400u;
    state->ram_size = (uint32_t)vm->ram_size;
    state->device_mask = 0u;
    state->irq_mask = 0u;

    if (amivm_m68k_set_exception_vector_base(vm, state->vector_base) != 0)
        return 1;

    return 0;
}

int amivm_firmware_step(struct amivm_vm *vm,
                        struct amivm_firmware_state *state)
{
    if (!vm || !state)
        return 1;

    switch (state->stage) {
    case AMIVM_FIRMWARE_RESET:
        state->stage = AMIVM_FIRMWARE_VECTOR_TABLE;
        break;
    case AMIVM_FIRMWARE_VECTOR_TABLE:
        state->stage = AMIVM_FIRMWARE_MEMORY;
        break;
    case AMIVM_FIRMWARE_MEMORY:
        if (state->ram_size == 0u)
            return 1;
        state->stage = AMIVM_FIRMWARE_DEVICES;
        break;
    case AMIVM_FIRMWARE_DEVICES:
        state->device_mask = 1u;
        state->stage = AMIVM_FIRMWARE_INTERRUPTS;
        break;
    case AMIVM_FIRMWARE_INTERRUPTS:
        state->irq_mask = 1u;
        state->stage = AMIVM_FIRMWARE_READY;
        break;
    case AMIVM_FIRMWARE_READY:
        break;
    default:
        return 1;
    }

    return 0;
}

int amivm_firmware_ready(const struct amivm_firmware_state *state)
{
    return state &&
           state->stage == AMIVM_FIRMWARE_READY &&
           state->ram_size != 0u &&
           state->device_mask != 0u &&
           state->irq_mask != 0u;
}
