#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_number(const char *path, const char *key, double *value)
{
    FILE *f;
    char buf[16384];
    size_t n;
    char *p;
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1u, sizeof(buf) - 1u, f);
    fclose(f);
    buf[n] = '\0';
    p = strstr(buf, key);
    if (!p) return 0;
    *value = strtod(p + strlen(key), NULL);
    return *value >= 0.0;
}

static int has_string(const char *path, const char *key, const char *value)
{
    FILE *f;
    char buf[16384];
    size_t n;
    char needle[256];
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1u, sizeof(buf) - 1u, f);
    fclose(f);
    buf[n] = '\0';
    snprintf(needle, sizeof(needle), "\"%s\":\"%s\"", key, value);
    return strstr(buf, needle) != NULL;
}

int main(int argc, char **argv)
{
    double score = 0.0;
    int count = 0;
    int i;
    if (argc < 3 || ((argc - 1) % 2) != 0) {
        fprintf(stderr, "usage: %s result.json baseline.json [...]\n", argv[0]);
        return 2;
    }
    for (i = 1; i < argc; i += 2) {
        double result, base, ratio;
        if (!read_number(argv[i], "\"throughput\":", &result) ||
            !read_number(argv[i + 1], "\"throughput\":", &base) || base <= 0.0 ||
            !has_string(argv[i], "reproducible", "true") ||
            !has_string(argv[i + 1], "reproducible", "true")) {
            fprintf(stderr, "MAX gate: correctness/metadata failure for %s\n", argv[i]);
            return 1;
        }
        ratio = result / base;
        score += ratio;
        ++count;
    }
    if (count == 0) return 1;
    score /= (double)count;
    printf("AmiVM MAX gate: %.6f\n", score);
    return score >= 1.0 ? 0 : 1;
}
