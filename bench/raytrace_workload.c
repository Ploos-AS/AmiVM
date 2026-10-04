#include "raytrace_workload.h"
#include "raytrace_kernel.h"
#include "raytrace_scene.h"

int amivm_raytrace_workload_reference(
    struct amivm_raytrace_workload_result *result)
{
    struct amivm_raytrace_scene scene;

    if (!result)
        return 2;

    amivm_raytrace_scene_default(&scene);
    result->rays = scene.ray_count;
    result->spheres = scene.sphere_count;
    result->checksum = amivm_raytrace_kernel(&scene);
    result->operations =
        (uint64_t)scene.ray_count * (uint64_t)scene.sphere_count;

    return 0;
}
