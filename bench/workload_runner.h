#ifndef AMIVM_WORKLOAD_RUNNER_H
#define AMIVM_WORKLOAD_RUNNER_H

#include <stddef.h>

struct amivm_workload_descriptor {
    char id[128];
    char platform[32];
    char kind[32];
    char artifact[512];
    char command[512];
    char input[512];
    char metric[64];
    char unit[64];
    char correctness_type[32];
    char correctness_expected[256];
    unsigned iterations;
};

int amivm_workload_validate(const char *json, size_t length);

#endif
