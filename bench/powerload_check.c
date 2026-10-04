#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int field(const char *json, const char *name, char *out, size_t out_size)
{
    char needle[128];
    const char *p;
    const char *q;
    size_t n;

    (void)snprintf(needle, sizeof(needle), "\"%s\":", name);
    p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '"') {
        ++p;
        q = strchr(p, '"');
        if (!q) return 0;
        n = (size_t)(q - p);
    } else {
        q = p;
        while (*q && *q != ',' && *q != '}' && *q != '\n') ++q;
        n = (size_t)(q - p);
    }
    if (n + 1u > out_size) return 0;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

static char *read_file(const char *path)
{
    FILE *fp;
    long size;
    char *data;

    fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    data = (char *)malloc((size_t)size + 1u);
    if (!data) { fclose(fp); return NULL; }
    if (fread(data, 1u, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }
    data[size] = '\0';
    fclose(fp);
    return data;
}

int main(int argc, char **argv)
{
    char *baseline;
    char *result;
    char base_workload[128], result_workload[128];
    char base_mode[64], result_mode[64];
    char base_throughput[64], result_throughput[64];
    char base_config[128], result_config[128];
    double allowed = 0.05;
    double base;
    double current;
    double regression;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s <baseline.json> <result.json> [max-regression-fraction]\n", argv[0]);
        return 2;
    }
    if (argc == 4) {
        char *end = NULL;
        allowed = strtod(argv[3], &end);
        if (!end || *end != '\0' || allowed < 0.0 || allowed >= 1.0) {
            fprintf(stderr, "invalid regression fraction\n");
            return 2;
        }
    }

    baseline = read_file(argv[1]);
    result = read_file(argv[2]);
    if (!baseline || !result) {
        fprintf(stderr, "unable to read input JSON\n");
        free(baseline);
        free(result);
        return 2;
    }

    if (!field(baseline, "workload_id", base_workload, sizeof(base_workload)) ||
        !field(result, "workload_id", result_workload, sizeof(result_workload)) ||
        !field(baseline, "mode", base_mode, sizeof(base_mode)) ||
        !field(result, "mode", result_mode, sizeof(result_mode)) ||
        !field(baseline, "throughput", base_throughput, sizeof(base_throughput)) ||
        !field(result, "throughput", result_throughput, sizeof(result_throughput)) ||
        !field(baseline, "vm_config", base_config, sizeof(base_config)) ||
        !field(result, "vm_config", result_config, sizeof(result_config))) {
        fprintf(stderr, "missing required Powerload fields\n");
        free(baseline);
        free(result);
        return 2;
    }

    if (strcmp(base_workload, result_workload) != 0 ||
        strcmp(base_mode, result_mode) != 0 ||
        strcmp(base_config, result_config) != 0) {
        fprintf(stderr, "baseline/result workload or mode mismatch\n");
        free(baseline);
        free(result);
        return 2;
    }

    base = strtod(base_throughput, NULL);
    current = strtod(result_throughput, NULL);
    if (base <= 0.0 || current < 0.0) {
        fprintf(stderr, "invalid throughput\n");
        free(baseline);
        free(result);
        return 2;
    }

    regression = (base - current) / base;
    printf("workload=%s mode=%s baseline=%.3f current=%.3f regression=%.3f%% allowed=%.3f%%\n",
           base_workload, base_mode, base, current,
           regression * 100.0, allowed * 100.0);

    free(baseline);
    free(result);
    return regression > allowed ? 1 : 0;
}
