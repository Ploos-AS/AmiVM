#ifndef AMIVM_GUEST_HARNESS_H
#define AMIVM_GUEST_HARNESS_H

#include <stdint.h>
#include <stddef.h>

struct amivm_vm;

struct amivm_guest_harness {
    const char *profile;
    const char *rom_path;
    const char *disk_path;
    uint64_t timeout_ms;
    uint64_t max_instructions;
    uint32_t qualification_mask;
    const char *os_marker;
    const char *filesystem_marker;
    const char *shell_marker;
};

enum {
    AMIVM_QUAL_RESET       = 1u << 0,
    AMIVM_QUAL_OS_DETECTED = 1u << 1,
    AMIVM_QUAL_FILESYSTEM  = 1u << 2,
    AMIVM_QUAL_SHELL       = 1u << 3,
    AMIVM_QUAL_INTERRUPTS  = 1u << 4,
    AMIVM_QUAL_DEVICE_IO   = 1u << 5,
    AMIVM_QUAL_WORKLOAD    = 1u << 6,
    AMIVM_QUAL_EXECUTION   = 1u << 7
};

int amivm_guest_harness_validate(const struct amivm_guest_harness *h);
int amivm_guest_harness_run(struct amivm_vm *vm,
                            const struct amivm_guest_harness *h,
                            uint32_t *result_mask);
int amivm_guest_harness_run_image(struct amivm_vm *vm,
                                  const struct amivm_guest_harness *h,
                                  const uint8_t *image,
                                  size_t image_size,
                                  uint32_t *result_mask);
int amivm_guest_harness_run_external(struct amivm_vm *vm,
                                     const struct amivm_guest_harness *h,
                                     uint32_t *result_mask);
int amivm_guest_harness_classify_serial(const struct amivm_guest_harness *h,
                                        const char *serial,
                                        size_t serial_size,
                                        uint32_t *result_mask);

#endif
