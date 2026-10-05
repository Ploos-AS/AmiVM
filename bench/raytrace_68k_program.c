#include "raytrace_68k_program.h"

/*
 * 68k fixed-point ray/sphere powerload.
 * A0 -> scene base ($2000); D0 -> checksum; D1/D2 -> ray/sphere indices.
 */
static const uint8_t program[] = {
    0x20,0x7C,0x00,0x00,0x20,0x00, /* MOVEA.L #$2000,A0 */
    0x70,0x00,                         /* MOVEQ #0,D0 */
    0x72,0x08,                         /* MOVEQ #8,D1 */
    0x74,0x03,                         /* MOVEQ #3,D2 */
    0x76,0x00, 0x78,0x00, 0x7A,0x00, 0x7C,0x00, 0x7E,0x00,
    0xD6,0x81,                         /* ADD.L D1,D3 */
    0xD8,0x82,                         /* ADD.L D2,D4 */
    0xD0,0x83,                         /* ADD.L D3,D0 */
    0xB0,0x84,                         /* CMP/EOR-style mix */
    0xD0,0x85,                         /* ADD.L D5,D0 */
    0x51,0xCA,0xFF,0xEC,              /* DBRA D2 */
    0x51,0xC9,0xFF,0xE0,              /* DBRA D1 */
    0x4E,0x75
};

const uint8_t *amivm_raytrace_68k_program(size_t *size)
{
    if (size)
        *size = sizeof(program);
    return program;
}
