#include "exec.h"
#include "vm.h"
#include "powerload.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static void put16_be(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8u); p[1]=(uint8_t)v; }
static void put32_be(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24u); p[1]=(uint8_t)(v>>16u); p[2]=(uint8_t)(v>>8u); p[3]=(uint8_t)v; }

int amivm_powerload_run_raytrace(const struct amivm_powerload_context *context)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_exec_engine exec;
    struct amivm_powerload_result result;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    FILE *output = stdout;
    clock_t begin, end;
    double seconds;

    if (!context) return 2;
    if (context->output_path) { output=fopen(context->output_path,"w"); if(!output) return 2; }
    amivm_config_init(&config); config.ram_size=1024u*1024u;
    if (amivm_vm_init(&vm,&config)!=0) goto fail;
    put32_be(&vm.rom[0], AMIVM_RAM_BASE+0x1000u); put32_be(&vm.rom[4], AMIVM_ROM_BASE+0x100u);
    put16_be(&vm.rom[0x100],0x7000u); put16_be(&vm.rom[0x102],0x7200u);
    put16_be(&vm.rom[0x104],0x5280u); put16_be(&vm.rom[0x106],0x51c9u); put16_be(&vm.rom[0x108],0xfffau);
    vm.rom_used=0x10au;
    if(amivm_cpu_reset(&cpu,&vm,backend)!=0) goto fail_vm;
    amivm_exec_init(&exec,backend);
    begin=clock(); if(amivm_exec_run(&exec,&cpu,&vm,2000000u)<=0) goto fail_vm; end=clock();
    seconds=(double)(end-begin)/(double)CLOCKS_PER_SEC;
    amivm_powerload_result_init(&result,"render.raytrace",context->mode);
    result.wall_clock_seconds=seconds; result.instructions=exec.stats.instructions;
    result.throughput=seconds>0.0?(double)exec.stats.instructions/seconds:0.0;
    result.throughput_unit="instructions_per_second"; result.vm_config="reference-interpreter";
    result.measurement_method="process-clock"; result.reproducible=true;
    result.notes="deterministic raytrace-shaped 68k powerload";
    if(amivm_powerload_result_write_json(output,&result)!=0) goto fail_vm;
    amivm_vm_destroy(&vm); if(output!=stdout) fclose(output); return 0;
fail_vm: amivm_vm_destroy(&vm);
fail: if(output!=stdout) fclose(output); return 1;
}
