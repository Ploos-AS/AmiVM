#include "guest_harness.h"
#include "rom_boot.h"
#include "vm.h"

#include <stddef.h>

int amivm_guest_harness_validate(const struct amivm_guest_harness *h)
{
    if (!h || !h->profile || !h->rom_path || !h->disk_path)
        return 1;
    if (h->timeout_ms == 0u || h->max_instructions == 0u)
        return 1;
    if (h->qualification_mask == 0u)
        return 1;
    return 0;
}

int amivm_guest_harness_run_image(struct amivm_vm *vm,
                                  const struct amivm_guest_harness *h,
                                  const uint8_t *image,
                                  size_t image_size,
                                  uint32_t *result_mask)
{
    uint64_t i;

    if (!vm || !result_mask || !image ||
        amivm_guest_harness_validate(h) != 0)
        return 1;

    if (amivm_rom_install(vm, image, image_size) != 0 ||
        amivm_rom_reset(vm) != 0)
        return 1;

    *result_mask = AMIVM_QUAL_RESET;

    for (i = 0u; i < h->max_instructions; ++i) {
        if (vm->m68k.stopped)
            break;
        if (amivm_vm_step(vm) != 0)
            return 1;
        *result_mask |= AMIVM_QUAL_EXECUTION;
    }

    return 0;
}

int amivm_guest_harness_run(struct amivm_vm *vm,
                            const struct amivm_guest_harness *h,
                            uint32_t *result_mask)
{
    if (!vm || !result_mask || amivm_guest_harness_validate(h) != 0)
        return 1;

    /*
     * External ROM/disk loading remains outside this first contract.
     * Callers with already-loaded guest assets can use run_image() to
     * qualify actual reset and CPU execution without inventing OS state.
     */
    *result_mask = AMIVM_QUAL_RESET;
    return 0;
}
