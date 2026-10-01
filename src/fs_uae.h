#ifndef AMIVM_FS_UAE_H
#define AMIVM_FS_UAE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "vm.h"

struct amivm_fsuae_report {
    unsigned supported;
    unsigned ignored_speed;
    unsigned ignored_host;
    unsigned unsupported;
    unsigned malformed;
};

int amivm_fsuae_load_config(const char *path, struct amivm_config *config,
                            struct amivm_fsuae_report *report,
                            bool strict);
void amivm_fsuae_report(const struct amivm_fsuae_report *report, FILE *out);

#endif
