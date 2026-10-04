#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *f;
    long size;
    char *data;

    f = fopen(path, "rb");
    if (!f || fseek(f, 0, SEEK_END) != 0) return NULL;
    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    data = malloc((size_t)size + 1u);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1u, (size_t)size, f) != (size_t)size) {
        free(data); fclose(f); return NULL;
    }
    data[size] = '\0';
    fclose(f);
    return data;
}

static int field(const char *json, const char *name, char *out, size_t size)
{
    char needle[128];
    const char *p, *q;
    size_t n;

    snprintf(needle, sizeof(needle), "\"%s\":", name);
    p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '"') {
        ++p;
        q = strchr(p, '"');
    } else {
        q = p;
        while (*q && *q != ',' && *q != '}') ++q;
    }
    if (!q) return 0;
    n = (size_t)(q - p);
    if (n >= size) return 0;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

int main(int argc, char **argv)
{
    char *json;
    char workload[128], mode[64], throughput[64], instructions[64];
    char config[128], method[128], reproducible[32];
    const char *expected_workload;
    const char *expected_mode;
    const char *expected_config;
    const char *expected_method;
    FILE *out;

    if (argc != 3 && argc != 7) {
        fprintf(stderr, "usage: %s <result.json> <baseline.json>\n", argv[0]);
        return 2;
    }

    expected_workload = expected_mode = expected_config = expected_method = NULL;
    if (argc == 7) {
        if (strcmp(argv[3], "--expect-workload") != 0 ||
            strcmp(argv[5], "--expect-mode") != 0) {
            fprintf(stderr, "invalid expectation arguments\\n");
            return 2;
        }
        expected_workload = argv[4];
        expected_mode = argv[6];
    }

    json = read_file(argv[1]);
    if (!json) return 2;

    if (!field(json, "workload_id", workload, sizeof(workload)) ||
        !field(json, "mode", mode, sizeof(mode)) ||
        !field(json, "throughput", throughput, sizeof(throughput)) ||
        !field(json, "instructions", instructions, sizeof(instructions)) ||
        !field(json, "vm_config", config, sizeof(config)) ||
        !field(json, "measurement_method", method, sizeof(method)) ||
        !field(json, "reproducible", reproducible, sizeof(reproducible))) {
        free(json);
        return 2;
    }

    if (expected_workload && strcmp(workload, expected_workload) != 0) {
        fprintf(stderr, "workload does not match expected baseline profile\\n");
        free(json);
        return 1;
    }
    if (expected_mode && strcmp(mode, expected_mode) != 0) {
        fprintf(stderr, "mode does not match expected baseline profile\\n");
        free(json);
        return 1;
    }

    if (strcmp(reproducible, "true") != 0 ||
        strtoul(instructions, NULL, 10) == 0u ||
        strtod(throughput, NULL) <= 0.0) {
        fprintf(stderr, "result is not eligible for a production baseline\\n");
        free(json);
        return 1;
    }

    out = fopen(argv[2], "w");
    if (!out) {
        free(json);
        return 2;
    }

    fprintf(out,
        "{\n"
        "  \"schema_version\": 1,\n"
        "  \"workload_id\": \"%s\",\n"
        "  \"mode\": \"%s\",\n"
        "  \"wall_clock_seconds\": 0.0,\n"
        "  \"throughput\": %s,\n"
        "  \"instructions\": %s,\n"
        "  \"throughput_unit\": \"instructions_per_second\",\n"
        "  \"vm_config\": \"%s\",\n"
        "  \"measurement_method\": \"%s\",\n"
        "  \"reproducible\": %s,\n"
        "  \"notes\": \"Generated from measured Powerload result.\"\n"
        "}\n",
        workload, mode, throughput, instructions, config, method, reproducible);

    fclose(out);
    free(json);
    return 0;
}
