#include "exec.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "M283 failed: %s at line %d\n", #x, __LINE__); return 1; } } while (0)

static void put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

static int same_cpu(const struct amivm_cpu_state *a, const struct amivm_cpu_state *b) {
    return memcmp(a->d,b->d,sizeof(a->d))==0 &&
           memcmp(a->a,b->a,sizeof(a->a))==0 &&
           a->pc==b->pc && a->sr==b->sr &&
           a->usp==b->usp && a->isp==b->isp && a->msp==b->msp &&
           a->vbr==b->vbr && a->tc==b->tc && a->urp==b->urp &&
           a->srp==b->srp && a->mmusr==b->mmusr &&
           a->last_fault==b->last_fault &&
           a->last_exception_vector==b->last_exception_vector;
}

int main(void) {
    struct amivm_config cfg;
    struct amivm_vm ref_vm, opt_vm;
    struct amivm_cpu_state ref_cpu, opt_cpu;
    struct amivm_exec_engine engine;
    const struct amivm_cpu_backend *backend=amivm_cpu_reference_backend();
    unsigned i;
    amivm_config_init(&cfg);
    cfg.ram_size=1024u*1024u;
    CHECK(amivm_vm_init(&ref_vm,&cfg)==0);
    CHECK(amivm_vm_init(&opt_vm,&cfg)==0);
    put32(ref_vm.rom,AMIVM_RAM_BASE+0x1000u);
    put32(ref_vm.rom+4u,AMIVM_ROM_BASE+0x100u);
    /* BRA.S -2: a one-instruction loop with an exact instruction boundary. */
    ref_vm.rom[0x100u]=0x60u;
    ref_vm.rom[0x101u]=0xfeu;
    ref_vm.rom_used=0x102u;
    memcpy(opt_vm.rom,ref_vm.rom,ref_vm.rom_used);
    opt_vm.rom_used=ref_vm.rom_used;
    CHECK(amivm_cpu_reset(&ref_cpu,&ref_vm,backend)==0);
    CHECK(amivm_cpu_reset(&opt_cpu,&opt_vm,backend)==0);
    amivm_exec_init(&engine,backend);
    for (i=0;i<128u;++i) {
        CHECK(amivm_cpu_step(&ref_cpu,&ref_vm,backend)==1);
        CHECK(amivm_exec_step(&engine,&opt_cpu,&opt_vm)==1);
        if (!same_cpu(&ref_cpu,&opt_cpu)) {
            fprintf(stderr,"M283 CPU divergence at step %u ref PC=%08x opt PC=%08x\n",
                    i,ref_cpu.pc,opt_cpu.pc);
            return 1;
        }
        CHECK(memcmp(ref_vm.ram,opt_vm.ram,cfg.ram_size)==0);
    }
    amivm_vm_destroy(&opt_vm);
    amivm_vm_destroy(&ref_vm);
    puts("M283 reference/optimized BRA loop differential smoke: PASS");
    return 0;
}
