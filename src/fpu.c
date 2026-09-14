#include "fpu.h"

#include <stddef.h>

static void set_cc(struct amivm_fpu_state *fpu, double value)
{
    fpu->fpsr &= ~(AMIVM_FPSR_CC_N | AMIVM_FPSR_CC_Z |
                   AMIVM_FPSR_CC_I | AMIVM_FPSR_CC_NAN);

    if (value != value) {
        fpu->fpsr |= AMIVM_FPSR_CC_NAN;
        return;
    }
    if (value == 0.0) {
        fpu->fpsr |= AMIVM_FPSR_CC_Z;
    } else if (value < 0.0) {
        fpu->fpsr |= AMIVM_FPSR_CC_N;
    }
}

void amivm_fpu_reset(struct amivm_fpu_state *fpu)
{
    unsigned i;

    if (fpu == NULL) {
        return;
    }
    for (i = 0u; i < 8u; ++i) {
        fpu->fp[i] = 0.0;
    }
    fpu->fpcr = 0u;
    fpu->fpsr = 0u;
    fpu->fpiar = 0u;
}

int amivm_fpu_execute(struct amivm_fpu_state *fpu, enum amivm_fpu_op op,
                      unsigned dst, unsigned src, uint32_t instruction_pc)
{
    double lhs;
    double rhs;
    double result;

    if (fpu == NULL || dst >= 8u || src >= 8u) {
        return AMIVM_FPU_ERR_REGISTER;
    }

    fpu->fpiar = instruction_pc;
    lhs = fpu->fp[dst];
    rhs = fpu->fp[src];

    switch (op) {
    case AMIVM_FPU_FMOVE:
        result = rhs;
        fpu->fp[dst] = result;
        set_cc(fpu, result);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FADD:
        result = lhs + rhs;
        fpu->fp[dst] = result;
        set_cc(fpu, result);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FSUB:
        result = lhs - rhs;
        fpu->fp[dst] = result;
        set_cc(fpu, result);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FMUL:
        result = lhs * rhs;
        fpu->fp[dst] = result;
        set_cc(fpu, result);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FDIV:
        if (rhs == 0.0) {
            fpu->fpsr |= AMIVM_FPSR_EXC_DZ;
            set_cc(fpu, lhs);
            return AMIVM_FPU_ERR_DIVZERO;
        }
        result = lhs / rhs;
        fpu->fp[dst] = result;
        set_cc(fpu, result);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FCMP:
        set_cc(fpu, lhs - rhs);
        return AMIVM_FPU_OK;
    case AMIVM_FPU_FTEST:
        set_cc(fpu, lhs);
        return AMIVM_FPU_OK;
    default:
        return AMIVM_FPU_ERR_OPERATION;
    }
}
