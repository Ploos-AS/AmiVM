#include "raytrace_fixedpoint.h"
#include "raytrace_scene.h"

int main(void)
{
    struct amivm_raytrace_scene scene;
    struct amivm_raytrace_fixed_result result;

    amivm_raytrace_scene_default(&scene);
    if (amivm_raytrace_fixed_reference(&scene, &result) != 0)
        return 1;

    if (result.tests != 36u)
        return 2;

    /* Stable semantic oracle for the fixed-point scene. */
    if (result.checksum != 0x46C27F643F2959E8ULL)
        return 3;

    return 0;
}
