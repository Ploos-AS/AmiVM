#ifndef AMIVM_RAYTRACE_68K_RESULT_H
#define AMIVM_RAYTRACE_68K_RESULT_H

#include <stdint.h>

struct amivm_raytrace_68k_result {
    uint32_t hit_count;
    uint64_t checksum;
};

#endif
