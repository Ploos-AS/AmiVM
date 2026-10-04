#include "powerload_runner.h"

#include <string.h>

static const struct amivm_powerload_workload workloads[] = {
    {"cpu.integer", "cpu", "Integer arithmetic and branch-heavy 68k workload", NULL},
    {"cpu.floating", "cpu", "Floating-point workload", NULL},
    {"cpu.memory", "cpu", "Memory throughput and latency workload", NULL},
    {"amiga.scene.copper", "amiga-scene", "Copper and raster-oriented scene workload", NULL},
    {"amiga.scene.blitter", "amiga-scene", "Blitter-heavy scene workload", NULL},
    {"render.raytrace", "rendering", "68k raytracing workload", NULL},
    {"scene.generation", "scene-generation", "Geometry and procedural scene generation workload", NULL},
    {"build.amiga", "development", "Representative Amiga software build", NULL},
    {"build.m68k-linux", "development", "Native m68k Linux software build", NULL},
    {"build.m68k-netbsd", "development", "Native m68k NetBSD software build", NULL}
};

const struct amivm_powerload_workload *amivm_powerload_find(const char *id)
{
    size_t i;
    if (!id)
        return NULL;
    for (i = 0u; i < amivm_powerload_count(); ++i)
        if (strcmp(workloads[i].id, id) == 0)
            return &workloads[i];
    return NULL;
}

size_t amivm_powerload_count(void)
{
    return sizeof(workloads) / sizeof(workloads[0]);
}

const struct amivm_powerload_workload *amivm_powerload_at(size_t index)
{
    return index < amivm_powerload_count() ? &workloads[index] : NULL;
}
