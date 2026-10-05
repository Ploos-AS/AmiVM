#include "guest.h"

int main(void)
{
    struct amivm_guest guest = {
        AMIVM_GUEST_AMIGAOS,
        "amigaos.rom",
        "run",
        1000u
    };

    if (amivm_guest_validate(&guest) != 0)
        return 1;
    if (amivm_guest_boot(&guest) != 0)
        return 2;
    if (amivm_guest_run(&guest) != 0)
        return 3;
    return 0;
}
