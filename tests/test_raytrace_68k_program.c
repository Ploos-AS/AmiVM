#include "raytrace_68k_program.h"

int main(void)
{
    size_t size = 0u;
    const unsigned char *p = amivm_raytrace_68k_program(&size);

    if (!p || size < 10u)
        return 1;

    /* MOVEA.L #$2000,A0 and a DBRA-based scene traversal must be present. */
    if (p[0] != 0x20u || p[1] != 0x7Cu ||
        p[6] != 0x70u || p[8] != 0x74u)
        return 2;

    if (p[size - 2u] != 0x4Eu || p[size - 1u] != 0x75u)
        return 3;

    return 0;
}
