#ifndef AMIVM_RAYTRACE_FIXEDPOINT_H
#define AMIVM_RAYTRACE_FIXEDPOINT_H

#include <stdint.h>
#include "raytrace_scene.h"

struct amivm_raytrace_fixed_result {
    uint64_t checksum;
    uint32_t hits;
    uint32_t tests;
};

int amivm_raytrace_fixed_reference(
    const struct amivm_raytrace_scene *scene,
    struct amivm_raytrace_fixed_result *result);

#endif
