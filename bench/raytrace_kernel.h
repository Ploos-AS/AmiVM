#ifndef AMIVM_RAYTRACE_KERNEL_H
#define AMIVM_RAYTRACE_KERNEL_H

#include "raytrace_scene.h"

#include <stdint.h>

uint64_t amivm_raytrace_kernel(const struct amivm_raytrace_scene *scene);

#endif
