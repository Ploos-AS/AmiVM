#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

int amivm_boot_prepare(struct amivm_vm *vm,
                       const struct amivm_boot_config *config)
{
    if (!vm || !config)
        return 1;

    if (config->kind != AMIVM_BOOT_AMIGA_ROM &&
        config->kind != AMIVM_BOOT_GUEST_IMAGE)
        return 1;

    if (config->kind == AMIVM_BOOT_AMIGA_ROM)
        return amivm_rom_reset(vm);

    vm->m68k.a[7] = config->initial_sp;
    vm->m68k.pc = config->initial_pc;
    vm->m68k.sr = config->initial_sr;
    vm->m68k.stopped = false;
    vm->pending_exception = AMIVM_M68K_EXC_NONE;
    vm->pending_exception_vector = 0u;
    vm->exception_halted = false;
    vm->exception_double_fault = false;
    vm->exception_depth = 0u;
    return 0;
}

int amivm_boot_step(struct amivm_vm *vm, uint64_t instruction_budget)
{
    uint64_t i;

    if (!vm || instruction_budget == 0u)
        return 1;

    for (i = 0u; i < instruction_budget; ++i) {
        if (vm->m68k.stopped || vm->exception_halted)
            return 0;
        if (amivm_vm_step(vm) != 0)
            return 1;
    }

    return 0;
}
