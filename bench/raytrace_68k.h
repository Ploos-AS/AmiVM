#ifndef AMIVM_RAYTRACE_68K_H
#define AMIVM_RAYTRACE_68K_H

#include <stdint.h>

#include "exec.h"
#include "vm.h"

struct amivm_raytrace_68k_result {
    uint32_t checksum;
    uint64_t instructions;
    uint32_t rays;
    uint32_t spheres;
};

int amivm_raytrace_68k_run(struct amivm_cpu_state *cpu,
                           struct amivm_vm *vm,
                           struct amivm_exec_engine *exec);

#endif
