#ifndef AMIVM_RAYTRACE_SCENE_H
#define AMIVM_RAYTRACE_SCENE_H

#include <stdint.h>

struct amivm_ray {
    int32_t ox, oy, oz;
    int32_t dx, dy, dz;
};

struct amivm_sphere {
    int32_t cx, cy, cz;
    int32_t radius;
};

struct amivm_raytrace_scene {
    const struct amivm_sphere *spheres;
    uint32_t sphere_count;
    const struct amivm_ray *rays;
    uint32_t ray_count;
};

#endif
