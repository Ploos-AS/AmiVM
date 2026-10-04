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

static uint64_t host_integer_workload(uint64_t iterations)
{
    uint64_t value = 0u;
    uint64_t i;

    for (i = 0u; i < iterations; ++i) {
        value += 1u;
        value ^= (value << 7u);
        value += 3u;
        value ^= (value >> 3u);
    }
    return value;
}

int main(int argc, char **argv)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_powerload_result result;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t loop_pc = AMIVM_ROM_BASE + 0x100u;
    const uint64_t host_iterations = 100000000u;
    const uint64_t vm_budget = 1000000u;
    const char *output_path = NULL;
    FILE *output = stdout;
    clock_t begin;
    clock_t end;
    double host_seconds;
    double vm_seconds;
    double host_iter_per_sec;
    double vm_ips;
    const uint64_t expected_instructions = 131074u;
    uint64_t host_value;

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

    begin = clock();
    host_value = host_integer_workload(host_iterations);
    end = clock();
    host_seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    host_iter_per_sec = host_seconds > 0.0 ?
        (double)host_iterations / host_seconds : 0.0;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) {
        if (output != stdout) fclose(output);
        return 1;
    }

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], loop_pc);
    put16_be(&vm.rom[0x100], 0x7000u);
    put16_be(&vm.rom[0x102], 0x7200u);
    put16_be(&vm.rom[0x104], 0x5280u);
    put16_be(&vm.rom[0x106], 0x51c9u);
    put16_be(&vm.rom[0x108], 0xfffau);
    vm.rom_used = 0x10au;

    if (amivm_cpu_reset(&cpu, &vm, backend) != 0) {
        amivm_vm_destroy(&vm);
        if (output != stdout) fclose(output);
        return 1;
    }
    amivm_exec_init(&exec, backend);

    begin = clock();
    if (amivm_exec_run(&exec, &cpu, &vm, vm_budget) <= 0) {
        amivm_vm_destroy(&vm);
        if (output != stdout) fclose(output);
        return 1;
    }
    end = clock();

    vm_seconds = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    vm_ips = vm_seconds > 0.0 ?
        (double)exec.stats.instructions / vm_seconds : 0.0;

    amivm_powerload_result_init(&result, "cpu.integer", "FAST");
    result.wall_clock_seconds = vm_seconds;
    result.instructions = exec.stats.instructions;
    result.throughput = vm_ips;
    result.throughput_unit = "instructions_per_second";
    result.vm_config = "reference-interpreter";
    result.measurement_method = "process-clock";
    result.reproducible = true;
    result.notes = "cpu integer baseline";

    if (amivm_powerload_result_write_json(output, &result) != 0) {
        if (output != stdout) fclose(output);
        amivm_vm_destroy(&vm);
        return 1;
    }

    if (output != stdout) {
        fclose(output);
    }

    printf("AmiVM Powerload: cpu.integer\n");
    printf("mode=FAST\n");
    printf("host_iterations=%llu host_seconds=%.6f host_iter_per_sec=%.0f host_value=%llu\n",
           (unsigned long long)host_iterations, host_seconds, host_iter_per_sec,
           (unsigned long long)host_value);
    printf("vm_instructions=%llu vm_seconds=%.6f vm_ips=%.0f cache_hits=%llu cache_misses=%llu\n",
           (unsigned long long)exec.stats.instructions, vm_seconds, vm_ips,
           (unsigned long long)exec.stats.cache_hits,
           (unsigned long long)exec.stats.cache_misses);

    amivm_vm_destroy(&vm);
    (void)output_path;
    return 0;
}
