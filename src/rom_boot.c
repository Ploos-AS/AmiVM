#include "rom_boot.h"
#include "vm.h"

#include <string.h>

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

int amivm_rom_install(struct amivm_vm *vm,
                      const uint8_t *image, size_t size)
{
    if (!vm || !image || size == 0u || size > AMIVM_ROM_SIZE)
        return 1;

    memcpy(vm->rom, image, size);
    vm->rom_used = size;
    return 0;
}

int amivm_rom_reset(struct amivm_vm *vm)
{
    uint32_t sp;
    uint32_t pc;

    if (!vm || vm->rom_used < 8u)
        return 1;

    sp = be32(&vm->rom[AMIVM_ROM_RESET_VECTOR_SP]);
    pc = be32(&vm->rom[AMIVM_ROM_RESET_VECTOR_PC]);

    vm->m68k.a[7] = sp;
    vm->m68k.pc = pc;
    vm->m68k.sr = 0x2000u;
    vm->m68k.stopped = false;
    vm->pending_exception = AMIVM_M68K_EXC_NONE;
    vm->pending_exception_vector = 0u;
    return 0;
}
