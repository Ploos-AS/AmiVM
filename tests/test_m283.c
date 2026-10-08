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

static int run_case_setup(const char *name, const uint8_t *code, size_t code_len, uint16_t ccr, int set_a0) {
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
    if (set_a0) {
        ref_cpu.a[0] = AMIVM_ROM_BASE + 0x10cu;
        opt_cpu.a[0] = AMIVM_ROM_BASE + 0x10cu;
    }
    ref_cpu.sr = (uint16_t)((ref_cpu.sr & 0xffe0u) | (ccr & 0x001fu));
    opt_cpu.sr = (uint16_t)((opt_cpu.sr & 0xffe0u) | (ccr & 0x001fu));
    amivm_exec_init(&engine,backend);
    for (i=0;i<128u;++i) {
        CHECK(amivm_cpu_step(&ref_cpu,&ref_vm,backend)==1);
        CHECK(amivm_exec_step(&engine,&opt_cpu,&opt_vm)==1);
        if (!same_cpu(&ref_cpu,&opt_cpu)) {
            fprintf(stderr,"M283 %s CPU divergence at step %u ref PC=%08x opt PC=%08x ref SR=%04x opt SR=%04x\n",
                    name,i,ref_cpu.pc,opt_cpu.pc,ref_cpu.sr,opt_cpu.sr);
            fprintf(stderr,"M285 optimized path: JIT blocks=%llu IR blocks=%llu fallbacks=%llu A7 ref=%08x opt=%08x\\n",
                    (unsigned long long)engine.stats.jit_blocks,
                    (unsigned long long)engine.stats.ir_blocks,
                    (unsigned long long)engine.stats.fallbacks,ref_cpu.a[7],opt_cpu.a[7]);
            {
                size_t slot=(size_t)(((AMIVM_ROM_BASE+0x100u)>>1u)&(AMIVM_EXEC_CACHE_ENTRIES-1u));
                const struct amivm_exec_cache_entry *entry=&engine.cache[slot];
                if (entry->ir_valid && entry->block.op_count) {
                    const struct amivm_ir_op *op=&entry->block.ops[entry->block.op_count-1u];
                    fprintf(stderr,"M285 decoded: opcode=%u guest_pc=%08x bytes=%u disp=%d JIT=%d\\n",
                        (unsigned)op->opcode,op->guest_pc,(unsigned)op->instruction_bytes,
                        op->imm,entry->jit_valid);
                    fprintf(stderr,"M285 JIT arch=%d size=%zu bytes:",(int)entry->jit.arch,entry->jit.size);
                    for (size_t k=0;k<entry->jit.size;++k) fprintf(stderr," %02x",entry->jit.bytes[k]);
                    fprintf(stderr,"\\n");
                }
            }
            return 1;
        }
        CHECK(memcmp(ref_vm.ram,opt_vm.ram,cfg.ram_size)==0);
    }
    amivm_vm_destroy(&opt_vm);
    amivm_vm_destroy(&ref_vm);
    printf("M283 %s differential smoke: PASS\\n",name);
    return 0;
}

static int run_case_sr(const char *name, const uint8_t *code, size_t code_len, uint16_t ccr) {
    return run_case_setup(name,code,code_len,ccr,0);
}

static int run_case(const char *name, const uint8_t *code, size_t code_len) {
    return run_case_sr(name,code,code_len,0u);
}

int main(void) {
    static const uint8_t bra_short[] = {0x60u,0xfeu};
    static const uint8_t bra_word[] = {0x60u,0x00u,0xffu,0xfcu};
    CHECK(run_case("BRA.S",bra_short,sizeof(bra_short))==0);
    /* Reference 68040 uses the post-extension PC as the word-branch base. */
    CHECK(run_case("BRA.W",bra_word,sizeof(bra_word))==0);
    /* BSR pushes return PC to RAM stack; RTS restores SP and resumes
     * at the BRA.S loop. Differential RAM comparison checks stack writes. */
    static const uint8_t bsr_rts[] = {0x61u,0x02u,0x60u,0xfeu,0x4eu,0x75u};
    CHECK(run_case("BSR.S / RTS stack roundtrip",bsr_rts,sizeof(bsr_rts))==0);
    static const uint8_t bsr_word_rts[] = {0x61u,0x00u,0x00u,0x02u,0x60u,0xfeu,0x4eu,0x75u};
    CHECK(run_case("BSR.W / RTS stack roundtrip",bsr_word_rts,sizeof(bsr_word_rts))==0);
    /* M286: two nested BSR.S calls, then two RTS returns. Both stack
     * frames and the restored stack pointer must match the reference. */
    static const uint8_t nested_bsr_short[] = {
        0x61u,0x04u, /* 0: BSR.S -> 6 */
        0x60u,0xfeu, /* 2: BRA.S -> 2 */
        0x4eu,0x71u, /* 4: NOP padding */
        0x61u,0x04u, /* 6: BSR.S -> 12 */
        0x4eu,0x75u, /* 8: RTS (outer) */
        0x4eu,0x71u, /* 10: NOP padding */
        0x4eu,0x75u  /* 12: RTS (inner) */
    };
    CHECK(run_case("nested BSR.S / RTS stack roundtrip",nested_bsr_short,sizeof(nested_bsr_short))==0);
    /* Mixed BSR.W outer call and BSR.S inner call exercises both
     * instruction lengths and two distinct stacked return addresses. */
    static const uint8_t nested_bsr_mixed[] = {
        0x61u,0x00u,0x00u,0x04u, /* 0: BSR.W -> 8 */
        0x60u,0xfeu,             /* 4: BRA.S -> 4 */
        0x4eu,0x71u,             /* 6: NOP padding */
        0x61u,0x04u,             /* 8: BSR.S -> 14 */
        0x4eu,0x75u,             /* 10: RTS (outer) */
        0x4eu,0x71u,             /* 12: NOP padding */
        0x4eu,0x75u              /* 14: RTS (inner) */
    };
    CHECK(run_case("nested BSR.W + BSR.S / RTS stack roundtrip",nested_bsr_mixed,sizeof(nested_bsr_mixed))==0);
    /* M287: JSR absolute long (0x4eb9) enters an RTS subroutine.
     * The absolute destination is ROM_BASE + 0x10c; the pushed return
     * address is ROM_BASE + 0x106, followed by a BRA.S self-loop. */
    static const uint8_t jsr_abs_long_rts[] = {
        0x4eu,0xb9u,0x00u,0xf0u,0x01u,0x0cu, /* JSR $00f0010c */
        0x60u,0xfeu,                         /* BRA.S -> self */
        0x4eu,0x71u,0x4eu,0x71u,             /* NOP padding */
        0x4eu,0x75u                          /* RTS */
    };
    CHECK(run_case("JSR absolute long / RTS stack roundtrip",
                   jsr_abs_long_rts,sizeof(jsr_abs_long_rts))==0);
    /* M288: PC-relative JSR d16(PC) into an RTS subroutine.
     * Displacement is relative to the extension word at +2. */
    static const uint8_t jsr_pc_relative_rts[] = {
        0x4eu,0xbau,0x00u,0x0au, /* JSR 10(PC) -> +12 */
        0x60u,0xfeu,             /* BRA.S -> self */
        0x4eu,0x71u,0x4eu,0x71u, /* NOP padding */
        0x4eu,0x71u,             /* NOP padding */
        0x4eu,0x75u              /* RTS */
    };
    CHECK(run_case("JSR PC-relative / RTS stack roundtrip",
                   jsr_pc_relative_rts,sizeof(jsr_pc_relative_rts))==0);
    /* M289: JSR (A0) with A0 pointing at the RTS subroutine. */
    static const uint8_t jsr_a0_rts[] = {
        0x4eu,0x90u,             /* JSR (A0) -> +12 */
        0x60u,0xfeu,             /* BRA.S -> self */
        0x4eu,0x71u,0x4eu,0x71u, /* NOP padding */
        0x4eu,0x71u,0x4eu,0x71u, /* NOP padding */
        0x4eu,0x75u              /* RTS */
    };
    CHECK(run_case_setup("JSR (A0) / RTS stack roundtrip",
                         jsr_a0_rts,sizeof(jsr_a0_rts),0u,1)==0);
    /* M290: nested JSR absolute-long -> BSR.S -> RTS -> RTS.
     * Outer JSR at +0 targets +12; inner BSR at +12 targets +18.
     * Each RTS unwinds one return address, ending at the BRA loop. */
    static const uint8_t nested_jsr_bsr_rts[] = {
        0x4eu,0xb9u,0x00u,0xf0u,0x01u,0x0cu, /* +0: JSR $00f0010c */
        0x60u,0xfeu,                         /* +6: BRA.S -> self */
        0x4eu,0x71u,0x4eu,0x71u,             /* +8,+10: padding */
        0x61u,0x04u,                         /* +12: BSR.S -> +18 */
        0x4eu,0x75u,                         /* +14: outer RTS */
        0x4eu,0x71u,                         /* +16: padding */
        0x4eu,0x75u                          /* +18: inner RTS */
    };
    CHECK(run_case("nested JSR absolute long / BSR.S / RTS stack roundtrip",
                   nested_jsr_bsr_rts,sizeof(nested_jsr_bsr_rts))==0);
    /* Exercise data-register and condition-code changes before a BRA loop. */
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
    /* Exercise untaken Bcc by branching forward to a stable BRA.S loop. */
    static const uint8_t beq_short_not_taken[] = {0x67u,0x02u,0x60u,0xfeu};
    static const uint8_t bmi_short_not_taken[] = {0x6bu,0x02u,0x60u,0xfeu};
    static const uint8_t beq_word_not_taken[] = {0x67u,0x00u,0x00u,0x02u,0x60u,0xfeu};
    CHECK(run_case("BEQ.S not taken",beq_short_not_taken,sizeof(beq_short_not_taken))==0);
    CHECK(run_case("BMI.S not taken",bmi_short_not_taken,sizeof(bmi_short_not_taken))==0);
    CHECK(run_case("BEQ.W not taken",beq_word_not_taken,sizeof(beq_word_not_taken))==0);
    /* Cover every condition code using reset CCR (N=Z=V=C=0).
     * Taken branches loop on themselves; untaken branches reach BRA.S. */
    static const unsigned taken_cc[] = {2u,4u,6u,8u,10u,12u,14u};
    static const unsigned not_taken_cc[] = {3u,5u,7u,9u,11u,13u,15u};
    unsigned k;
    for (k=0u;k<sizeof(taken_cc)/sizeof(taken_cc[0]);++k) {
        uint8_t insn[] = {(uint8_t)(0x60u+taken_cc[k]),0xfeu};
        CHECK(run_case("Bcc.S taken matrix",insn,sizeof(insn))==0);
    }
    for (k=0u;k<sizeof(not_taken_cc)/sizeof(not_taken_cc[0]);++k) {
        uint8_t insn[] = {(uint8_t)(0x60u+not_taken_cc[k]),0x02u,0x60u,0xfeu};
        CHECK(run_case("Bcc.S not-taken matrix",insn,sizeof(insn))==0);
    }
    /* Each CCR combination must agree for every conditional branch.
     * Both paths converge to a BRA.S self-loop, avoiding ROM overrun. */
    for (unsigned ccr=0u;ccr<16u;++ccr) {
        for (unsigned cc=2u;cc<16u;++cc) {
            uint8_t insn[] = {(uint8_t)(0x60u+cc),0x02u,0x60u,0xfeu,0x60u,0xfeu};
            CHECK(run_case_sr("Bcc.S CCR matrix",insn,sizeof(insn),(uint16_t)ccr)==0);
        }
    }
    return 0;
}
