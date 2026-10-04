#include "raytrace_68k.h"
#include "raytrace_68k_program.h"
#include "raytrace_scene_blob.h"

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
    size_t scene_size;
    const uint8_t *scene = amivm_raytrace_scene_blob(&scene_size);
    size_t program_size;
    const uint8_t *program = amivm_raytrace_68k_program(&program_size);
    const uint32_t scene_addr = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t entry = AMIVM_ROM_BASE + 0x100u;

    if (scene_size > vm->ram_size - 0x2000u || program_size > sizeof(vm->rom) - 0x100u)
        return 2;
    for (size_t i = 0u; i < scene_size; ++i)
        vm->ram[0x2000u + i] = scene[i];
    (void)scene_addr;
    for (size_t i = 0u; i < program_size; ++i)
        vm->rom[0x100u + i] = program[i];

    if (amivm_cpu_reset(cpu, vm, exec->backend) != 0)
        return 1;

    amivm_exec_init(exec, exec->backend);
    return amivm_exec_run(exec, cpu, vm, 1000000u) > 0 ? 0 : 1;
}
