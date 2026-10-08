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
           a->cacr==b->cacr && a->sfc==b->sfc && a->dfc==b->dfc &&
           a->stopped==b->stopped &&
           a->fault_address==b->fault_address &&
           a->fault_opcode==b->fault_opcode &&
           a->last_fault==b->last_fault &&
           a->last_exception_vector==b->last_exception_vector;
}

static int run_case(const char *name, const uint8_t *code, size_t code_len) {
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
    /* The fixture is a single branch instruction looping to its own PC. */
    memcpy(ref_vm.rom+0x100u,code,code_len);
    ref_vm.rom_used=0x100u+code_len;
    memcpy(opt_vm.rom,ref_vm.rom,ref_vm.rom_used);
    opt_vm.rom_used=ref_vm.rom_used;
    CHECK(amivm_cpu_reset(&ref_cpu,&ref_vm,backend)==0);
    CHECK(amivm_cpu_reset(&opt_cpu,&opt_vm,backend)==0);
    amivm_exec_init(&engine,backend);
    for (i=0;i<128u;++i) {
        CHECK(amivm_cpu_step(&ref_cpu,&ref_vm,backend)==1);
        CHECK(amivm_exec_step(&engine,&opt_cpu,&opt_vm)==1);
        if (!same_cpu(&ref_cpu,&opt_cpu)) {
            fprintf(stderr,"M283 %s CPU divergence at step %u ref PC=%08x opt PC=%08x ref SR=%04x opt SR=%04x\n",
                    name,i,ref_cpu.pc,opt_cpu.pc,ref_cpu.sr,opt_cpu.sr);
            return 1;
        }
        CHECK(memcmp(ref_vm.ram,opt_vm.ram,cfg.ram_size)==0);
    }
    amivm_vm_destroy(&opt_vm);
    amivm_vm_destroy(&ref_vm);
    printf("M283 %s differential smoke: PASS\\n",name);
    return 0;
}

int main(void) {
    static const uint8_t bra_short[] = {0x60u,0xfeu};
    static const uint8_t bra_word[] = {0x60u,0x00u,0xffu,0xfcu};
    CHECK(run_case("BRA.S",bra_short,sizeof(bra_short))==0);
    /* Reference 68040 uses the post-extension PC as the word-branch base. */
    CHECK(run_case("BRA.W",bra_word,sizeof(bra_word))==0);
    /* Reference backend currently implements BRA/BSR, not Bcc.
     * Exercise data-register and condition-code changes before a BRA loop. */
    static const uint8_t moveq_zero_bra[] = {0x70u,0x00u,0x60u,0xfeu};
    static const uint8_t moveq_neg_bra[] = {0x70u,0xffu,0x60u,0xfeu};
    CHECK(run_case("MOVEQ #0,D0; BRA.S",moveq_zero_bra,sizeof(moveq_zero_bra))==0);
    CHECK(run_case("MOVEQ #-1,D0; BRA.S",moveq_neg_bra,sizeof(moveq_neg_bra))==0);
    static const uint8_t bne_short[] = {0x66u,0xfeu};
    static const uint8_t bpl_short[] = {0x6au,0xfeu};
    static const uint8_t bne_word[] = {0x66u,0x00u,0xffu,0xfcu};
    CHECK(run_case("BNE.S taken",bne_short,sizeof(bne_short))==0);
    CHECK(run_case("BPL.S taken",bpl_short,sizeof(bpl_short))==0);
    CHECK(run_case("BNE.W taken",bne_word,sizeof(bne_word))==0);
    return 0;
}
