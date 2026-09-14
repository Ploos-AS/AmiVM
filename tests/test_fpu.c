#include "fpu.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct amivm_fpu_state fpu;

    amivm_fpu_reset(&fpu);
    CHECK(fpu.fpcr == 0u);
    CHECK(fpu.fpsr == 0u);
    CHECK(fpu.fpiar == 0u);
    CHECK(fpu.fp[0] == 0.0);

    fpu.fp[0] = 3.5;
    fpu.fp[1] = 2.0;

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FADD, 0u, 1u, 0x1000u) == AMIVM_FPU_OK);
    CHECK(fpu.fp[0] == 5.5);
    CHECK(fpu.fpiar == 0x1000u);

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FSUB, 0u, 1u, 0x1002u) == AMIVM_FPU_OK);
    CHECK(fpu.fp[0] == 3.5);

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FMUL, 0u, 1u, 0x1004u) == AMIVM_FPU_OK);
    CHECK(fpu.fp[0] == 7.0);

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FDIV, 0u, 1u, 0x1006u) == AMIVM_FPU_OK);
    CHECK(fpu.fp[0] == 3.5);

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FMOVE, 2u, 0u, 0x1008u) == AMIVM_FPU_OK);
    CHECK(fpu.fp[2] == 3.5);

    fpu.fp[3] = 3.5;
    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FCMP, 2u, 3u, 0x100au) == AMIVM_FPU_OK);
    CHECK((fpu.fpsr & AMIVM_FPSR_CC_Z) != 0u);

    fpu.fp[4] = -1.0;
    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FTEST, 4u, 4u, 0x100cu) == AMIVM_FPU_OK);
    CHECK((fpu.fpsr & AMIVM_FPSR_CC_N) != 0u);

    fpu.fp[5] = 0.0;
    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FDIV, 0u, 5u, 0x100eu) == AMIVM_FPU_ERR_DIVZERO);
    CHECK((fpu.fpsr & AMIVM_FPSR_EXC_DZ) != 0u);
    CHECK(fpu.fpiar == 0x100eu);

    CHECK(amivm_fpu_execute(&fpu, AMIVM_FPU_FMOVE, 8u, 0u, 0x1010u) == AMIVM_FPU_ERR_REGISTER);

    puts("AmiVM M2.8 FPU foundation tests: PASS");
    return 0;
}
