#include "raytrace_68k.h"
#include "powerload.h"

#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_powerload_result result;
    FILE *out = stdout;
    clock_t begin, end;
    double seconds;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [result.json]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        out = fopen(argv[1], "w");
        if (!out) return 2;
    }

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        goto fail;

    amivm_exec_init(&exec, amivm_cpu_reference_backend());

    begin = clock();
    if (amivm_raytrace_68k_run(&cpu, &vm, &exec) != 0)
        goto fail_vm;
    end = clock();

    seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;

    amivm_powerload_result_init(&result, "render.raytrace", "FAST");
    result.wall_clock_seconds = seconds;
    result.instructions = exec.stats.instructions;
    result.throughput = seconds > 0.0 ?
        (double)exec.stats.instructions / seconds : 0.0;
    result.throughput_unit = "instructions_per_second";
    result.vm_config = "reference-interpreter";
    result.measurement_method = "process-clock";
    result.reproducible = true;
    result.notes = "68k raytrace execution through AmiVM";

    if (amivm_powerload_result_write_json(out, &result) != 0)
        goto fail_vm;

    amivm_vm_destroy(&vm);
    if (out != stdout) fclose(out);
    return 0;

fail_vm:
    amivm_vm_destroy(&vm);
fail:
    if (out != stdout) fclose(out);
    return 1;
}
