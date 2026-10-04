#include <stdio.h>
#include <string.h>

static const char *workloads[] = {
    "cpu.integer",
    "cpu.floating",
    "cpu.memory",
    "amiga.scene.copper",
    "amiga.scene.blitter",
    "render.raytrace",
    "scene.generation",
    "build.amiga",
    "build.m68k-linux",
    "build.m68k-netbsd"
};

static int is_workload(const char *id)
{
    size_t i;
    for (i = 0u; i < sizeof(workloads) / sizeof(workloads[0]); ++i)
        if (strcmp(id, workloads[i]) == 0)
            return 1;
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --list | --workload <id> [--mode <mode>] [--output <file>]\n",
            argv0);
}

int main(int argc, char **argv)
{
    const char *workload = NULL;
    const char *mode = "FAST";
    const char *output = NULL;
    size_t i;

    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (i = 0u; i < sizeof(workloads) / sizeof(workloads[0]); ++i)
            puts(workloads[i]);
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
            usage(argv[0]);
            return 2;
        }
    }

    if (!workload || !is_workload(workload)) {
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

    /*
     * M2.214 establishes the stable runner contract. Execution dispatch is
     * deliberately implemented by the workload registry in a later step;
     * this avoids shelling out from the runner and keeps CI deterministic.
     */
    printf("workload=%s mode=%s", workload, mode);
    if (output)
        printf(" output=%s", output);
    putchar('\n');
    return 0;
}
