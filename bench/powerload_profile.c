#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct profile {
    char workload_id[128];
    char mode[64];
    char vm_config[128];
    char measurement_method[128];
    char throughput_unit[128];
};

static int value(const char *line, const char *key, char *out, size_t size)
{
    char needle[128];
    const char *p, *q;
    size_t n;

    snprintf(needle, sizeof(needle), "%s = \"", key);
    p = strstr(line, needle);
    if (!p) return 0;
    p += strlen(needle);
    q = strchr(p, '"');
    if (!q) return 0;
    n = (size_t)(q - p);
    if (n + 1u > size) return 0;
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

int main(int argc, char **argv)
{
    FILE *in;
    struct profile p = {{0}};
    char line[256];
    unsigned schema = 0u;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <profile.toml>\n", argv[0]);
        return 2;
    }
    in = fopen(argv[1], "r");
    if (!in) return 2;

    while (fgets(line, sizeof(line), in)) {
        if (strstr(line, "schema_version")) {
            if (sscanf(line, "schema_version = %u", &schema) != 1)
                goto invalid;
        } else {
            value(line, "workload_id", p.workload_id, sizeof(p.workload_id));
            value(line, "mode", p.mode, sizeof(p.mode));
            value(line, "vm_config", p.vm_config, sizeof(p.vm_config));
            value(line, "measurement_method", p.measurement_method,
                  sizeof(p.measurement_method));
            value(line, "throughput_unit", p.throughput_unit,
                  sizeof(p.throughput_unit));
        }
    }
    fclose(in);

    if (schema != 1u || !p.workload_id[0] || !p.mode[0] ||
        !p.vm_config[0] || !p.measurement_method[0] ||
        !p.throughput_unit[0])
        return 1;

    printf("profile schema=%u workload=%s mode=%s config=%s method=%s unit=%s\n",
           schema, p.workload_id, p.mode, p.vm_config,
           p.measurement_method, p.throughput_unit);
    return 0;

invalid:
    fclose(in);
    return 1;
}
