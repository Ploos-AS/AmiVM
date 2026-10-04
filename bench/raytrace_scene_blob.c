#include "raytrace_scene_blob.h"

static const uint8_t scene[] = {
    /* 4 spheres: cx,cy,cz,radius as signed 32-bit big-endian */
    0,0,0,0, 0,0,0,0, 0,0,0,32, 0,0,0,12,
    0xFF,0xFF,0xFF,0xEE, 0,0,0,4, 0,0,0,48, 0,0,0,8,
    0,0,0,18, 0xFF,0xFF,0xFF,0xFC, 0,0,0,56, 0,0,0,10,
    0,0,0,0, 0xFF,0xFF,0xFF,0xF0, 0,0,0,64, 0,0,0,14,

    /* 9 rays: ox,oy,oz,dx,dy,dz */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1,
    0xFF,0xFF,0xFF,0xF8, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,4,
    0,0,0,8, 0,0,0,0, 0,0,0,0, 0xFF,0xFF,0xFF,0xFF, 0,0,0,0, 0,0,0,4
};

const uint8_t *amivm_raytrace_scene_blob(size_t *size)
{
    if (size) *size = sizeof(scene);
    return scene;
}
