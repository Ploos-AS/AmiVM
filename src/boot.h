#ifndef AMIVM_BOOT_H
#define AMIVM_BOOT_H

#include <stdint.h>

struct amivm_vm;

enum amivm_boot_kind {
    AMIVM_BOOT_AMIGA_ROM = 0,
    AMIVM_BOOT_GUEST_IMAGE = 1
};

struct amivm_boot_config {
    enum amivm_boot_kind kind;
    uint32_t initial_pc;
    uint32_t initial_sp;
    uint16_t initial_sr;
};

int amivm_boot_prepare(struct amivm_vm *vm,
                       const struct amivm_boot_config *config);
int amivm_boot_step(struct amivm_vm *vm, uint64_t instruction_budget);

#endif
