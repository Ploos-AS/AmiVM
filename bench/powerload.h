#ifndef AMIVM_POWERLOAD_H
#define AMIVM_POWERLOAD_H

#include <stdbool.h>
#include <stdio.h>

struct amivm_powerload_result {
    unsigned schema_version;
    const char *workload_id;
    const char *mode;
    double wall_clock_seconds;
    double throughput;
    const char *throughput_unit;
    const char *vm_config;
    bool reproducible;
    const char *notes;
};

void amivm_powerload_result_init(struct amivm_powerload_result *result,
                                 const char *workload_id,
                                 const char *mode);

int amivm_powerload_result_write_json(
    FILE *stream, const struct amivm_powerload_result *result);

#endif
