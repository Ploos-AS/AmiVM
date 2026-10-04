#ifndef AMIVM_RAYTRACE_WORKLOAD_H
#define AMIVM_RAYTRACE_WORKLOAD_H

#include <stdint.h>

struct amivm_raytrace_workload_result {
    uint32_t rays;
    uint32_t spheres;
    uint64_t checksum;
    uint64_t operations;
};

int amivm_raytrace_workload_reference(
    struct amivm_raytrace_workload_result *result);

#endif
