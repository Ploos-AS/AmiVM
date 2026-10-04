#include "raytrace_workload.h"

#include <stdint.h>

int main(void)
{
    struct amivm_raytrace_workload_result result;

    if (amivm_raytrace_workload_reference(&result) != 0)
        return 1;

    if (result.rays != 9u || result.spheres != 4u)
        return 1;

    if (result.operations != 36u)
        return 1;

    /*
     * This checksum is the semantic oracle for the fixed scene.
     * It must remain stable across optimized execution backends.
     */
    if (result.checksum != 0x00048d73d4b42600ULL)
        return 2;

    return 0;
}
