#include "fs_uae.h"
#include "cpu_profile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
    char *e;
    while (*s && isspace((unsigned char)*s)) ++s;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) --e;
    *e = '\0';
    return s;
}

static void unquote(char *s)
{
    size_t n = strlen(s);
    if (n >= 2u && ((s[0] == '"' && s[n - 1u] == '"') ||
                    (s[0] == '\'' && s[n - 1u] == '\''))) {
        memmove(s, s + 1u, n - 2u);
        s[n - 2u] = '\0';
    }
}

static bool is_speed_option(const char *k)
{
    return strncmp(k, "uae_cpu_speed", 13u) == 0 ||
           strncmp(k, "uae_cpu_multiplier", 18u) == 0 ||
           strncmp(k, "uae_cpu_frequency", 18u) == 0 ||
           strncmp(k, "uae_cpu_throttle", 17u) == 0 ||
           strcmp(k, "warp_mode") == 0 ||
           strcmp(k, "warp") == 0;
}

static bool is_host_option(const char *k)
{
    return strncmp(k, "video_", 6u) == 0 ||
           strncmp(k, "audio_", 6u) == 0 ||
           strcmp(k, "fullscreen") == 0 ||
           strcmp(k, "window_position") == 0 ||
           strcmp(k, "joystick") == 0 ||
           strcmp(k, "mouse_speed") == 0;
}

static int set_option(const char *key, const char *value,
                      struct amivm_config *config,
                      struct amivm_fsuae_report *r)
{
    const char *cpu_name = NULL;
    size_t mib;
    if (is_speed_option(key)) {
        ++r->ignored_speed;
        return 0;
    }
    if (is_host_option(key)) {
        ++r->ignored_host;
        return 0;
    }
    if (strcmp(key, "cpu") == 0 || strcmp(key, "uae_cpu_model") == 0)
        cpu_name = value;
    else if (strcmp(key, "uae_mmu_model") == 0) {
        if (strcmp(value, "68851") == 0) config->external_mmu = AMIVM_MMU_68851;
        else if (strcmp(value, "68030") == 0 || strcmp(value, "68040") == 0 ||
                 strcmp(value, "68060") == 0) {
            /* Native CPU profiles carry their own MMU semantics. */
        } else { ++r->unsupported; return 1; }
        ++r->supported;
        return 0;
    } else if (strcmp(key, "chip_memory") == 0 || strcmp(key, "fast_memory") == 0) {
        if (!amivm_parse_size_mib(value, &mib)) { ++r->malformed; return 1; }
        if (strcmp(key, "chip_memory") == 0) {
            if (mib > 0u) config->ram_size = mib * 1024u * 1024u;
        } else {
            config->ram_size += mib * 1024u * 1024u;
        }
        ++r->supported;
        return 0;
    } else if (strcmp(key, "kickstart_rom_file") == 0 ||
               strcmp(key, "rom") == 0) {
        {
            size_t n = strlen(value) + 1u;
            char *copy = malloc(n);
            if (!copy) { ++r->malformed; return 1; }
            memcpy(copy, value, n);
            config->rom_path = copy;
        }
        ++r->supported;
        return 0;
    } else if (strcmp(key, "amiga_model") == 0 ||
               strcmp(key, "accelerator") == 0 ||
               strncmp(key, "floppy_image_", 13u) == 0 ||
               strncmp(key, "hard_drive_", 11u) == 0 ||
               strcmp(key, "bsdsocket_library") == 0 ||
               strcmp(key, "deterministic") == 0 ||
               strcmp(key, "jit_compiler") == 0) {
        ++r->supported;
        return 0;
    } else {
        ++r->unsupported;
        return 0;
    }
    if (cpu_name != NULL) {
        const struct amivm_cpu_profile *p = amivm_cpu_profile_by_name(cpu_name);
        if (p == NULL) { ++r->unsupported; return 1; }
        config->cpu_profile = p;
        ++r->supported;
    }
    return 0;
}

int amivm_fsuae_load_config(const char *path, struct amivm_config *config,
                            struct amivm_fsuae_report *report, bool strict)
{
    FILE *f;
    char line[1024];
    if (!path || !config || !report) return -1;
    memset(report, 0, sizeof *report);
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line), *eq, *comment;
        int rc;
        if (*s == '\0' || *s == '#') continue;
        comment = strchr(s, '#');
        if (comment) *comment = '\0';
        eq = strchr(s, '=');
        if (!eq) { ++report->malformed; continue; }
        *eq++ = '\0';
        s = trim(s); eq = trim(eq); unquote(eq);
        rc = set_option(s, eq, config, report);
        if (strict && rc != 0) { fclose(f); return -2; }
    }
    fclose(f);
    return (strict && (report->unsupported || report->malformed)) ? -2 : 0;
}

void amivm_fsuae_report(const struct amivm_fsuae_report *r, FILE *out)
{
    if (!r || !out) return;
    fprintf(out, "fsuae.supported=%u\n", r->supported);
    fprintf(out, "fsuae.ignored_speed=%u\n", r->ignored_speed);
    fprintf(out, "fsuae.ignored_host=%u\n", r->ignored_host);
    fprintf(out, "fsuae.unsupported=%u\n", r->unsupported);
    fprintf(out, "fsuae.malformed=%u\n", r->malformed);
    fprintf(out, "fsuae.speed_policy=unlimited\n");
}
