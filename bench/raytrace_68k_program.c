#include "raytrace_68k_program.h"

static const uint8_t program[] = {
    0x70,0x00,             /* MOVEQ #0,D0 checksum */
    0x72,0x03,             /* MOVEQ #3,D1 sphere loop */
    0x74,0x08,             /* MOVEQ #8,D2 ray loop */
    0x76,0x00,             /* MOVEQ #0,D3 */

    0xD6,0x80,             /* ADD.L D0,D3 */
    0xB1,0x83,             /* EOR.L D1,D3 */
    0xD0,0x82,             /* ADD.L D2,D0 */
    0xD6,0x81,             /* ADD.L D1,D3 */

    0x51,0xC9,0xFF,0xF2, /* DBRA D1, inner loop */
    0x51,0xCA,0xFF,0xE8, /* DBRA D2, ray loop */
    0x4E,0x75              /* RTS */
};

const uint8_t *amivm_raytrace_68k_program(size_t *size)
{
    if (size)
        *size = sizeof(program);
    return program;
}
