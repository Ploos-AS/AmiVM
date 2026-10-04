#include "raytrace_scene.h"

static const struct amivm_sphere spheres[] = {
    { 0, 0, 32, 12 },
    { -18, 4, 48, 8 },
    { 18, -4, 56, 10 },
    { 0, -16, 64, 14 }
};

static const struct amivm_ray rays[] = {
    { 0, 0, 0, 0, 0, 1 },
    { -8, 0, 0, 1, 0, 4 },
    { 8, 0, 0, -1, 0, 4 },
    { 0, -8, 0, 0, 1, 4 },
    { 0, 8, 0, 0, -1, 4 },
    { -4, -4, 0, 1, 1, 6 },
    { 4, -4, 0, -1, 1, 6 },
    { -4, 4, 0, 1, -1, 6 },
    { 4, 4, 0, -1, -1, 6 }
};

void amivm_raytrace_scene_default(struct amivm_raytrace_scene *scene)
{
    if (!scene)
        return;
    scene->spheres = spheres;
    scene->sphere_count = (uint32_t)(sizeof(spheres) / sizeof(spheres[0]));
    scene->rays = rays;
    scene->ray_count = (uint32_t)(sizeof(rays) / sizeof(rays[0]));
}
