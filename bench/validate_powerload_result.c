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

int main(int argc, char **argv)
{
    char *json;

    if (argc != 2)
        return 2;
    json = read_file(argv[1]);
    if (!json)
        return 2;

    if (!strstr(json, "\"schema_version\":1") ||
        !strstr(json, "\"workload_id\":\"cpu.integer\"") ||
        !strstr(json, "\"mode\":\"FAST\"") ||
        !strstr(json, "\"throughput\":") ||
        !strstr(json, "\"throughput_unit\":\"instructions_per_second\"") ||
        !strstr(json, "\"vm_config\":\"reference-interpreter\"")) {
        free(json);
        return 1;
    }

    free(json);
    puts("Powerload result validation: PASS");
    return 0;
}
