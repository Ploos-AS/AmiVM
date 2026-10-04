#include "raytrace_68k.h"

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8u);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24u);
    p[1] = (uint8_t)(v >> 16u);
    p[2] = (uint8_t)(v >> 8u);
    p[3] = (uint8_t)v;
}

int amivm_raytrace_68k_run(struct amivm_cpu_state *cpu,
                           struct amivm_vm *vm,
                           struct amivm_exec_engine *exec)
{
    const uint32_t entry = AMIVM_ROM_BASE + 0x100u;

    if (!cpu || !vm || !exec)
        return 2;

    put32(&vm->rom[0], AMIVM_RAM_BASE + 0x1000u);
    put32(&vm->rom[4], entry);

    /*
     * Deterministic 68k raytrace-shaped kernel:
     * D0 = accumulated checksum
     * D1 = sphere counter (4)
     * D2 = ray counter (9)
     * D3 = inner arithmetic state
     *
     * The nested loops deliberately use only integer 68k operations.
     * This is the first real 68k workload representation; the exact
     * scene semantics remain checked by the host-side oracle.
     */
    put16(&vm->rom[0x100], 0x7000u); /* MOVEQ #0,D0 */
    put16(&vm->rom[0x102], 0x7203u); /* MOVEQ #3,D1 */
    put16(&vm->rom[0x104], 0x7408u); /* MOVEQ #8,D2 */
    put16(&vm->rom[0x106], 0x7600u); /* MOVEQ #0,D3 */

    put16(&vm->rom[0x108], 0x7203u); /* MOVEQ #3,D1 */
    put16(&vm->rom[0x10A], 0xD680u); /* ADD.L D0,D3 */
    put16(&vm->rom[0x10C], 0xB183u); /* EOR.L D1,D3 */
    put16(&vm->rom[0x10E], 0xD082u); /* ADD.L D2,D0 */
    put16(&vm->rom[0x110], 0x51C9u); /* DBRA D1 */
    put16(&vm->rom[0x112], 0xFFF6u); /* -> 0x10A */
    put16(&vm->rom[0x114], 0x51CAu); /* DBRA D2 */
    put16(&vm->rom[0x116], 0xFFF0u); /* -> 0x108 */
    put16(&vm->rom[0x118], 0x4E75u); /* RTS */

    vm->rom_used = 0x11Au;

    if (amivm_cpu_reset(cpu, vm, exec->backend) != 0)
        return 1;

    amivm_exec_init(exec, exec->backend);
    return amivm_exec_run(exec, cpu, vm, 1000000u) > 0 ? 0 : 1;
}
