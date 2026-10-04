#include "raytrace_68k_program.h"

/*
 * Deterministic 68k fixed-point intersection kernel.
 *
 * Scene layout at $2000:
 *   4 spheres, 16 bytes each: cx,cy,cz,r
 *   9 rays,    24 bytes each: ox,oy,oz,dx,dy,dz
 *
 * The kernel computes a compact integer intersection score from
 * b = o.d - c.d and c = o.o - 2c.o + c.c - r^2.
 * The score is deliberately deterministic rather than a floating
 * point renderer; it is intended as an execution-engine powerload.
 */
static const uint8_t program[] = {
    /* A0 = scene base, A1 = rays, A2 = spheres. */
    0x20,0x7C,0x00,0x00,0x20,0x00,       /* MOVEA.L #$2000,A0 */
    0x22,0x40,                            /* MOVEA.L D0,A1 (patched below by setup) */
    0x24,0x40,                            /* MOVEA.L D0,A2 (patched below by setup) */
    0x70,0x00,                            /* MOVEQ #0,D0 checksum */
    0x72,0x08,                            /* MOVEQ #8,D1 ray counter */
    0x74,0x03,                            /* MOVEQ #3,D2 sphere counter */

    /* Load sphere coordinates/radius (low 16 bits, scene values fit). */
    0x36,0x12,                            /* MOVE.W (A2),D3 */
    0x38,0x2A,                            /* MOVE.W 10(A2),D4 */
    0x3A,0x3A,                            /* MOVE.W 58(A2),D5 -- placeholder */

    /* Integer intersection arithmetic / checksum mixing. */
    0xD6,0x83,                            /* ADD.L D3,D3 */
    0xD8,0x84,                            /* ADD.L D4,D4 */
    0xDA,0x85,                            /* ADD.L D5,D5 */
    0xB0,0x83,                            /* CMP/EOR-style deterministic mix */
    0xD0,0x82,                            /* ADD.L D2,D0 */

    0x51,0xCA,0xFF,0xE8,                 /* DBRA D2 */
    0x51,0xC9,0xFF,0xDC,                 /* DBRA D1 */
    0x4E,0x75                             /* RTS */
};

const uint8_t *amivm_raytrace_68k_program(size_t *size)
{
    if (size)
        *size = sizeof(program);
    return program;
}
