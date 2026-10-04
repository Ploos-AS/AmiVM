#include <stdio.h>
#include <stdlib.h>
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

static int implemented(const char *id)
{
    return strcmp(id, "cpu.integer") == 0 ||
           strcmp(id, "render.raytrace") == 0;
}

int main(int argc, char **argv)
{
    const char *output_dir = ".";
    size_t i;
    char path[512];

    if (argc == 3 && strcmp(argv[1], "--output-dir") == 0)
        output_dir = argv[2];
    else if (argc != 1) {
        fprintf(stderr, "usage: %s [--output-dir DIR]\n", argv[0]);
        return 2;
    }

    for (i = 0u; i < sizeof(workloads) / sizeof(workloads[0]); ++i) {
        if (!implemented(workloads[i])) {
            fprintf(stderr, "MAX workload not implemented: %s\n", workloads[i]);
            return 1;
        }
        if (snprintf(path, sizeof(path), "%s/%s.json", output_dir,
                     workloads[i]) >= (int)sizeof(path))
            return 2;
        printf("MAX workload: %s -> %s\n", workloads[i], path);
    }

    return 0;
}
