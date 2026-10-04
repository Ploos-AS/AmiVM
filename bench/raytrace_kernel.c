#include "raytrace_kernel.h"

uint64_t amivm_raytrace_kernel(const struct amivm_raytrace_scene *scene)
{
    uint64_t checksum = 0u;
    uint32_t r, s;

    if (!scene)
        return 0u;

    for (r = 0u; r < scene->ray_count; ++r) {
        const struct amivm_ray *ray = &scene->rays[r];
        uint64_t best = UINT64_MAX;

        for (s = 0u; s < scene->sphere_count; ++s) {
            const struct amivm_sphere *sphere = &scene->spheres[s];
            int64_t ox = (int64_t)ray->ox - sphere->cx;
            int64_t oy = (int64_t)ray->oy - sphere->cy;
            int64_t oz = (int64_t)ray->oz - sphere->cz;
            int64_t dx = ray->dx;
            int64_t dy = ray->dy;
            int64_t dz = ray->dz;
            int64_t b = ox * dx + oy * dy + oz * dz;
            int64_t c = ox * ox + oy * oy + oz * oz -
                        (int64_t)sphere->radius * sphere->radius;
            int64_t disc = b * b - c * (dx * dx + dy * dy + dz * dz);

            if (disc >= 0 && (uint64_t)(b < 0 ? -b : b) < best)
                best = (uint64_t)(b < 0 ? -b : b);
        }

        checksum ^= (best == UINT64_MAX ? 0xfeedu : best + r);
        checksum = (checksum << 5u) | (checksum >> 59u);
    }

    return checksum;
}
