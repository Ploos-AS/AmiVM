#include "raytrace_fixedpoint.h"

int amivm_raytrace_fixed_reference(
    const struct amivm_raytrace_scene *scene,
    struct amivm_raytrace_fixed_result *result)
{
    uint32_t r, s;
    uint64_t checksum = 0u;
    uint32_t hits = 0u;

    if (!scene || !result)
        return 2;

    for (r = 0u; r < scene->ray_count; ++r) {
        const struct amivm_ray *ray = &scene->rays[r];

        for (s = 0u; s < scene->sphere_count; ++s) {
            const struct amivm_sphere *sphere = &scene->spheres[s];
            int64_t ox = (int64_t)ray->ox - sphere->cx;
            int64_t oy = (int64_t)ray->oy - sphere->cy;
            int64_t oz = (int64_t)ray->oz - sphere->cz;
            int64_t dx = ray->dx;
            int64_t dy = ray->dy;
            int64_t dz = ray->dz;
            int64_t a = dx * dx + dy * dy + dz * dz;
            int64_t b = ox * dx + oy * dy + oz * dz;
            int64_t c = ox * ox + oy * oy + oz * oz -
                        (int64_t)sphere->radius * sphere->radius;
            int64_t disc = b * b - a * c;

            if (disc >= 0) {
                ++hits;
                checksum ^= (uint64_t)(disc + (int64_t)(r * 17u + s));
                checksum = (checksum << 7u) | (checksum >> 57u);
            }
        }
    }

    result->checksum = checksum;
    result->hits = hits;
    result->tests = scene->ray_count * scene->sphere_count;
    return 0;
}
