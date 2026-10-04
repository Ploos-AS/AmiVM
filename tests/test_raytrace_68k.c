#include "raytrace_68k.h"

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;
    amivm_exec_init(&exec, amivm_cpu_reference_backend());

    if (amivm_raytrace_68k_run(&cpu, &vm, &exec) != 0) {
        amivm_vm_destroy(&vm);
        return 1;
    }

    if (cpu.d[0] == 0u || exec.stats.instructions == 0u) {
        amivm_vm_destroy(&vm);
        return 2;
    }

    amivm_vm_destroy(&vm);
    return 0;
}
