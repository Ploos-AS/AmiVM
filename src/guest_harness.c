#include "guest_harness.h"
#include "rom_boot.h"
#include "vm.h"

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

int amivm_guest_harness_validate(const struct amivm_guest_harness *h)
{
    if (!h || !h->profile || !h->rom_path || !h->disk_path)
        return 1;
    if (h->timeout_ms == 0u || h->max_instructions == 0u)
        return 1;
    if (h->qualification_mask == 0u)
        return 1;
    if (!h->os_marker || !h->filesystem_marker || !h->shell_marker)
        return 1;
    if (h->serial_capture_size == 0u || h->serial_capture_size > AMIVM_SERIAL_LOG_SIZE)
        return 1;
    return 0;
}

static bool contains_marker(const char *serial, size_t serial_size,
                         const char *marker)
{
    size_t marker_len;
    size_t i;

    if (!serial || !marker)
        return false;
    marker_len = strlen(marker);
    if (marker_len == 0u || marker_len > serial_size)
        return false;
    for (i = 0u; i + marker_len <= serial_size; ++i)
        if (memcmp(serial + i, marker, marker_len) == 0)
            return true;
    return false;
}

int amivm_guest_harness_classify_serial(const struct amivm_guest_harness *h,
                                        const char *serial,
                                        size_t serial_size,
                                        uint32_t *result_mask)
{
    if (!h || !serial || !result_mask ||
        amivm_guest_harness_validate(h) != 0)
        return 1;

    *result_mask &= ~(AMIVM_QUAL_OS_DETECTED |
                      AMIVM_QUAL_FILESYSTEM |
                      AMIVM_QUAL_SHELL);

    if (contains_marker(serial, serial_size, h->os_marker))
        *result_mask |= AMIVM_QUAL_OS_DETECTED;
    if (contains_marker(serial, serial_size, h->filesystem_marker))
        *result_mask |= AMIVM_QUAL_FILESYSTEM;
    if (contains_marker(serial, serial_size, h->shell_marker))
        *result_mask |= AMIVM_QUAL_SHELL;

    return 0;
}

bool amivm_guest_harness_qualification_complete(
    const struct amivm_guest_harness *h, uint32_t result_mask)
{
    uint32_t required;

    if (!h || amivm_guest_harness_validate(h) != 0)
        return false;
    required = h->qualification_mask;
    return (result_mask & required) == required;
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

int amivm_guest_harness_run_external(struct amivm_vm *vm,
                                     const struct amivm_guest_harness *h,
                                     uint32_t *result_mask)
{
    uint64_t i;

    if (!vm || !result_mask || amivm_guest_harness_validate(h) != 0)
        return 1;

    if (amivm_vm_load_rom(vm, h->rom_path) != 0)
        return 1;

    if (h->disk_path[0] != '\0' &&
        amivm_vm_attach_hard_drive(vm, 0u, h->disk_path) != 0)
        return 1;

    if (amivm_rom_reset(vm) != 0)
        return 1;

    *result_mask = AMIVM_QUAL_RESET;
    for (i = 0u; i < h->max_instructions; ++i) {
        if (vm->m68k.stopped)
            break;
        if (amivm_vm_step(vm) != 0)
            return 1;
        *result_mask |= AMIVM_QUAL_EXECUTION;
    }

    {
        char serial[AMIVM_SERIAL_LOG_SIZE + 1u];
        size_t n = amivm_vm_serial_read(vm, serial, h->serial_capture_size);
        if (n > AMIVM_SERIAL_LOG_SIZE)
            n = AMIVM_SERIAL_LOG_SIZE;
        serial[n] = '\0';
        if (amivm_guest_harness_classify_serial(h, serial, n, result_mask) != 0)
            return 1;
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
