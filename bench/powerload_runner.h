#ifndef AMIVM_POWERLOAD_RUNNER_H
#define AMIVM_POWERLOAD_RUNNER_H

#include <stddef.h>
#include <stdio.h>

struct amivm_powerload_context {
    const char *mode;
    const char *output_path;
};

typedef int (*amivm_powerload_run_fn)(
    const struct amivm_powerload_context *context);

struct amivm_powerload_workload {
    const char *id;
    const char *category;
    const char *description;
    amivm_powerload_run_fn run;
};

const struct amivm_powerload_workload *amivm_powerload_find(
    const char *id);

size_t amivm_powerload_count(void);

const struct amivm_powerload_workload *amivm_powerload_at(size_t index);

#endif
