#include "powerload_runner.h"
#include "powerload.h"

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

static int run_cpu_integer(const struct amivm_powerload_context *context)
{
    const uint64_t iterations = 100000000u;
    uint64_t value;
    clock_t begin;
    clock_t end;
    double seconds;
    struct amivm_powerload_result result;
    FILE *stream = stdout;

    if (!context)
        return 2;

    if (context->output_path) {
        stream = fopen(context->output_path, "w");
        if (!stream)
            return 2;
    }

    begin = clock();
    value = cpu_integer_workload(iterations);
    end = clock();
    seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;

    amivm_powerload_result_init(&result, "cpu.integer", context->mode);
    result.wall_clock_seconds = seconds;
    result.throughput = seconds > 0.0 ? (double)iterations / seconds : 0.0;
    result.throughput_unit = "iterations_per_second";
    result.vm_config = "host-baseline";
    result.measurement_method = "process-clock";
    result.reproducible = true;
    result.notes = "runner host baseline";

    if (amivm_powerload_result_write_json(stream, &result) != 0) {
        if (stream != stdout)
            fclose(stream);
        return 1;
    }

    if (stream != stdout)
        fclose(stream);

    (void)value;
    return 0;
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
