#include "raytrace_68k_program.h"

static const uint8_t program[] = {
    /* A0 = scene base (0x00002000 in VM RAM). */
    0x20,0x7C, 0x00,0x00,0x20,0x00, /* MOVEA.L #$2000,A0 */
    0x70,0x00,                         /* MOVEQ #0,D0 */
    0x74,0x23,                         /* MOVEQ #35,D2: 36 scene words */
    0x22,0x18,                         /* MOVE.L (A0)+,D1 */
    0xD0,0x81,                         /* ADD.L D1,D0 */
    0x51,0xCA,0xFF,0xF8,              /* DBRA D2,-8 */
    0x4E,0x75                          /* RTS */
};

const uint8_t *amivm_raytrace_68k_program(size_t *size)
{
    if (size)
        *size = sizeof(program);
    return program;
}
