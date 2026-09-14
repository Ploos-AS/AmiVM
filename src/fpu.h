#ifndef AMIVM_FPU_H
#define AMIVM_FPU_H

#include <stdint.h>

enum amivm_fpu_op {
    AMIVM_FPU_FMOVE = 0,
    AMIVM_FPU_FADD,
    AMIVM_FPU_FSUB,
    AMIVM_FPU_FMUL,
    AMIVM_FPU_FDIV,
    AMIVM_FPU_FCMP,
    AMIVM_FPU_FTEST,
};

enum amivm_fpu_result {
    AMIVM_FPU_OK = 0,
    AMIVM_FPU_ERR_REGISTER = -1,
    AMIVM_FPU_ERR_DIVZERO = -2,
    AMIVM_FPU_ERR_OPERATION = -3,
};

#define AMIVM_FPSR_CC_N 0x08000000u
#define AMIVM_FPSR_CC_Z 0x04000000u
#define AMIVM_FPSR_CC_I 0x02000000u
#define AMIVM_FPSR_CC_NAN 0x01000000u
#define AMIVM_FPSR_EXC_DZ 0x00000400u

struct amivm_fpu_state {
    double fp[8];
    uint32_t fpcr;
    uint32_t fpsr;
    uint32_t fpiar;
};

void amivm_fpu_reset(struct amivm_fpu_state *fpu);
int amivm_fpu_execute(struct amivm_fpu_state *fpu, enum amivm_fpu_op op,
                      unsigned dst, unsigned src, uint32_t instruction_pc);

#endif
