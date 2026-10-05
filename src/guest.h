#ifndef AMIVM_GUEST_H
#define AMIVM_GUEST_H

#include <stdint.h>

enum amivm_guest_platform {
    AMIVM_GUEST_AMIGAOS,
    AMIVM_GUEST_M68K_LINUX,
    AMIVM_GUEST_M68K_NETBSD
};

struct amivm_guest {
    enum amivm_guest_platform platform;
    const char *boot_artifact;
    const char *command;
    uint64_t timeout_ms;
};

int amivm_guest_validate(const struct amivm_guest *guest);
int amivm_guest_boot(struct amivm_guest *guest);
int amivm_guest_run(struct amivm_guest *guest);

#endif
