#include "guest.h"

#include <stddef.h>

int amivm_guest_validate(const struct amivm_guest *guest)
{
    if (!guest || !guest->boot_artifact || !guest->command)
        return 1;
    if (guest->timeout_ms == 0u)
        return 1;
    switch (guest->platform) {
    case AMIVM_GUEST_AMIGAOS:
    case AMIVM_GUEST_M68K_LINUX:
    case AMIVM_GUEST_M68K_NETBSD:
        return 0;
    default:
        return 1;
    }
}

int amivm_guest_boot(struct amivm_guest *guest)
{
    if (amivm_guest_validate(guest) != 0)
        return 1;

    /*
     * Boot orchestration is intentionally not implemented here yet.
     * This contract separates guest lifecycle from the execution core.
     */
    return 0;
}

int amivm_guest_run(struct amivm_guest *guest)
{
    if (amivm_guest_validate(guest) != 0)
        return 1;
    return 0;
}
