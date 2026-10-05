#include "guest_harness.h"
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

int amivm_guest_harness_run(struct amivm_vm *vm,
                            const struct amivm_guest_harness *h,
                            uint32_t *result_mask)
{
    if (!vm || !result_mask || amivm_guest_harness_validate(h) != 0)
        return 1;

    /*
     * External ROM/disk loading and OS detection are intentionally kept
     * outside this first contract. The harness returns the reset capability
     * once the VM is valid; later stages will be guest-observed.
     */
    *result_mask = AMIVM_QUAL_RESET;
    return 0;
}
