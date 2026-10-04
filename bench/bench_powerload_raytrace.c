#include "exec.h"
#include "vm.h"
#include "powerload.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static void put16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_powerload_result result;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t loop_pc = AMIVM_ROM_BASE + 0x100u;
    const uint64_t vm_budget = 2000000u;
    const char *output_path = NULL;
    FILE *output = stdout;
    clock_t begin;
    clock_t end;
    double seconds;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [result.json]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        output_path = argv[1];
        output = fopen(output_path, "w");
        if (!output) {
            perror(output_path);
            return 2;
        }
    }

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        goto fail;

    /*
     * Deterministic integer-heavy raytracing kernel placeholder.
     * The workload shape is deliberately fixed so optimized execution
     * backends can be compared without changing the architectural result.
     */
    put32_be(&vm.rom[0], AMIVM_RAM_BASE + 0x1000u);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x7000u);
    put16_be(&vm.rom[0x102], 0x7200u);
    put16_be(&vm.rom[0x104], 0x5280u);
    put16_be(&vm.rom[0x106], 0x51c9u);
    put16_be(&vm.rom[0x108], 0xfffau);
    vm.rom_used = 0x10au;

    if (amivm_cpu_reset(&cpu, &vm, backend) != 0)
        goto fail_vm;
    amivm_exec_init(&exec, backend);

    begin = clock();
    if (amivm_exec_run(&exec, &cpu, &vm, vm_budget) <= 0)
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
    result.notes = "deterministic raytrace-shaped 68k powerload";

    if (amivm_powerload_result_write_json(output, &result) != 0)
        goto fail_vm;

    amivm_vm_destroy(&vm);
    if (output != stdout)
        fclose(output);
    return 0;

fail_vm:
    amivm_vm_destroy(&vm);
fail:
    if (output != stdout)
        fclose(output);
    return 1;
}
