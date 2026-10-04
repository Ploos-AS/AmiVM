#include "powerload_runner.h"
#include "powerload.h"
#include "exec.h"
#include "vm.h"

#include <stdint.h>
#include <time.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static uint64_t cpu_integer_workload(uint64_t iterations)
{
    uint64_t value = 0u;
    uint64_t i;
    for (i = 0u; i < iterations; ++i) {
        value += 1u;
        value ^= value << 7u;
        value += 3u;
        value ^= value >> 3u;
    }
    return value;
}

static void put16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

static int run_cpu_integer(const struct amivm_powerload_context *context)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_powerload_result result;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t loop_pc = AMIVM_ROM_BASE + 0x100u;
    const uint64_t vm_budget = 1000000u;
    FILE *stream = stdout;
    clock_t begin;
    clock_t end;
    double seconds;
    const uint64_t expected_instructions = 131074u;

    if (!context)
        return 2;
    if (context->output_path) {
        stream = fopen(context->output_path, "w");
        if (!stream)
            return 2;
    }

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        goto fail;

    put32_be(&vm.rom[0], AMIVM_RAM_BASE + 0x1000u);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x7000u);
    put16_be(&vm.rom[0x102], 0x7200u);
    put16_be(&vm.rom[0x104], 0x5280u);
    put16_be(&vm.rom[0x106], 0x51c9u);
    put16_be(&vm.rom[0x108], 0xfffau);
    vm.rom_used = 0x10au;

    if (amivm_cpu_reset(&cpu, &vm, backend) != 0)
        goto fail_vm;
    amivm_exec_init(&exec, backend);

    begin = clock();
    if (amivm_exec_run(&exec, &cpu, &vm, vm_budget) <= 0)
        goto fail_vm;
    end = clock();

    seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    amivm_powerload_result_init(&result, "cpu.integer", context->mode);
    result.wall_clock_seconds = seconds;
    result.instructions = exec.stats.instructions;
    result.throughput = seconds > 0.0 ?
        (double)exec.stats.instructions / seconds : 0.0;
    result.throughput_unit = "instructions_per_second";
    result.vm_config = "reference-interpreter";
    result.measurement_method = "process-clock";
    result.reproducible = true;
    result.notes = "AmiVM 68k integer execution";

    if (amivm_powerload_result_write_json(stream, &result) != 0)
        goto fail_vm;

    amivm_vm_destroy(&vm);
    if (stream != stdout)
        fclose(stream);
    return 0;

fail_vm:
    amivm_vm_destroy(&vm);
fail:
    if (stream != stdout)
        fclose(stream);
    return 1;
}

static struct amivm_powerload_workload workloads[] = {
    {"cpu.integer", "cpu", "Integer arithmetic and branch-heavy 68k workload", run_cpu_integer},
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

static int run_selected(const struct amivm_powerload_workload *workload,
                         const char *mode, const char *output)
{
    struct amivm_powerload_context context;
    if (!workload || !workload->run)
        return 3;
    context.mode = mode;
    context.output_path = output;
    return workload->run(&context);
}

const struct amivm_powerload_workload *amivm_powerload_find(const char *id)
{
    size_t i;
    if (!id) return NULL;
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
int amivm_powerload_run(const char *id, const char *mode, const char *output)
{
    const struct amivm_powerload_workload *workload =
        amivm_powerload_find(id);
    return run_selected(workload, mode ? mode : "FAST", output);
}


int main(int argc, char **argv)
{
    const char *workload = NULL;
    const char *mode = "FAST";
    const char *output = NULL;
    size_t i;

    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (i = 0u; i < amivm_powerload_count(); ++i)
            puts(amivm_powerload_at(i)->id);
        return 0;
    }

    for (i = 1u; i < (size_t)argc; ++i) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1u < (size_t)argc)
            workload = argv[++i];
        else if (strcmp(argv[i], "--mode") == 0 && i + 1u < (size_t)argc)
            mode = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1u < (size_t)argc)
            output = argv[++i];
        else {
            fprintf(stderr,
                    "usage: %s --list | --workload <id> [--mode <mode>] [--output <file>]\n",
                    argv[0]);
            return 2;
        }
    }

    if (!workload || !amivm_powerload_find(workload)) {
        fprintf(stderr, "unsupported or missing workload: %s\n",
                workload ? workload : "(none)");
        return 2;
    }

    if (strcmp(mode, "FAST") != 0 &&
        strcmp(mode, "DETERMINISTIC") != 0 &&
        strcmp(mode, "DEBUG") != 0) {
        fprintf(stderr, "unsupported mode: %s\n", mode);
        return 2;
    }

    return amivm_powerload_run(workload, mode, output);
}
