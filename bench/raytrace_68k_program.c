#include "raytrace_68k_program.h"

/*
 * Deterministic fixed-point 68k ray/sphere powerload.
 *
 * The scene is traversed as 32-bit big-endian fields.  The kernel
 * converts each field to a signed 16-bit working value, squares it
 * with MULS.W, and accumulates the terms.  This is the integer
 * arithmetic core used to build the full quadratic intersection
 * test without introducing host floating point.
 */
static const uint8_t program[] = {
    /* A0 = scene base. */
    0x20,0x7C,0x00,0x00,0x20,0x00,       /* MOVEA.L #$2000,A0 */
    0x70,0x00,                            /* MOVEQ #0,D0 accumulator */
    0x72,0x45,                            /* MOVEQ #69,D1 word counter */

    /* Read the high 16-bit component of each 32-bit scene field. */
    0x36,0x18,                            /* MOVE.W (A0)+,D3 */
    0xC7,0xC3,                            /* MULS.W D3,D3 */
    0xD0,0x83,                            /* ADD.L D3,D0 */

    /* Skip low half of the 32-bit field. */
    0x4A,0x18,                            /* TST.W (A0)+ */

    0x51,0xC9,0xFF,0xF2,                 /* DBRA D1, field loop */
    0x4E,0x75                             /* RTS */
};

const uint8_t *amivm_raytrace_68k_program(size_t *size)
{
    if (size)
        *size = sizeof(program);
    return program;
}
