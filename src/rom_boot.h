#ifndef AMIVM_ROM_BOOT_H
#define AMIVM_ROM_BOOT_H

#include <stddef.h>
#include <stdint.h>

struct amivm_vm;

#define AMIVM_ROM_RESET_VECTOR_SP 0u
#define AMIVM_ROM_RESET_VECTOR_PC 4u

int amivm_rom_install(struct amivm_vm *vm,
                      const uint8_t *image, size_t size);
int amivm_rom_reset(struct amivm_vm *vm);

#endif
