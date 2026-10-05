#ifndef AMIVM_FIRMWARE_H
#define AMIVM_FIRMWARE_H

#include <stdint.h>

struct amivm_vm;

enum amivm_firmware_stage {
    AMIVM_FIRMWARE_RESET = 0,
    AMIVM_FIRMWARE_VECTOR_TABLE,
    AMIVM_FIRMWARE_MEMORY,
    AMIVM_FIRMWARE_DEVICES,
    AMIVM_FIRMWARE_INTERRUPTS,
    AMIVM_FIRMWARE_READY
};

struct amivm_firmware_state {
    enum amivm_firmware_stage stage;
    uint32_t vector_base;
    uint32_t ram_size;
    uint32_t device_mask;
    uint32_t irq_mask;
};

int amivm_firmware_init(struct amivm_vm *vm,
                        struct amivm_firmware_state *state);
int amivm_firmware_step(struct amivm_vm *vm,
                        struct amivm_firmware_state *state);
int amivm_firmware_ready(const struct amivm_firmware_state *state);

#endif
