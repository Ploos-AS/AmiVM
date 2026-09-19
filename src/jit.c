#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "jit.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#include <sys/mman.h>
#endif

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_NZVC (SR_N | SR_Z | SR_V | SR_C)
#define SR_XNZVC (SR_X | SR_NZVC)

static int emit8(struct amivm_jit_code *code, uint8_t value)
{
    if (code->size >= AMIVM_JIT_CODE_CAPACITY) return AMIVM_JIT_NO_SPACE;
    code->bytes[code->size++] = value;
    return AMIVM_JIT_OK;
}

static int emit16(struct amivm_jit_code *code, uint16_t value)
{
    int rc;
    rc = emit8(code, (uint8_t)value);
    if (rc != AMIVM_JIT_OK) return rc;
    return emit8(code, (uint8_t)(value >> 8u));
}

static int emit32(struct amivm_jit_code *code, uint32_t value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        int rc = emit8(code, (uint8_t)(value >> (i * 8u)));
        if (rc != AMIVM_JIT_OK) return rc;
    }
    return AMIVM_JIT_OK;
}

static int emit_mov32_state_imm(struct amivm_jit_code *code,
                                size_t offset, uint32_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0xc7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x87u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, value);
}

static int emit_mov32_state_eax(struct amivm_jit_code *code, size_t offset)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x8bu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x87u); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, (uint32_t)offset);
}

static int emit_alu32_state_eax(struct amivm_jit_code *code,
                                size_t offset, uint8_t opcode)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, opcode); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x87u); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, (uint32_t)offset);
}

static int emit_and16_state_imm(struct amivm_jit_code *code,
                                size_t offset, uint16_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x66u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xa7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit16(code, value);
}

static int emit_or16_state_imm(struct amivm_jit_code *code,
                               size_t offset, uint16_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x66u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x8fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit16(code, value);
}

static int emit_or16_state_cx(struct amivm_jit_code *code, size_t offset)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x66u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x09u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x8fu); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, (uint32_t)offset);
}

static int emit_nz_flags(struct amivm_jit_code *code, uint32_t value)
{
    const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
    uint16_t flags = 0u;
    int rc;

    if (value == 0u) flags |= SR_Z;
    if ((value & 0x80000000u) != 0u) flags |= SR_N;
    rc = emit_and16_state_imm(code, sr_offset, (uint16_t)~SR_NZVC);
    if (rc != AMIVM_JIT_OK) return rc;
    if (flags != 0u) return emit_or16_state_imm(code, sr_offset, flags);
    return AMIVM_JIT_OK;
}

static int emit_capture_nz_flags(struct amivm_jit_code *code)
{
    const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
    int rc;

    rc = emit8(code, 0x9cu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x58u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xe9u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x04u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xe1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x0cu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit_and16_state_imm(code, sr_offset, (uint16_t)~SR_NZVC);
    if (rc != AMIVM_JIT_OK) return rc;
    return emit_or16_state_cx(code, sr_offset);
}

static int emit_capture_arithmetic_flags(struct amivm_jit_code *code, int update_x)
{
    const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
    int rc;

    rc = emit8(code, 0x9cu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x58u); if (rc != AMIVM_JIT_OK) return rc;

    rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xe1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x01u); if (rc != AMIVM_JIT_OK) return rc;
    if (update_x) {
        rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xcau); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xe2u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0x04u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0x09u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xd1u); if (rc != AMIVM_JIT_OK) return rc;
    }

    rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc2u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xeau); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x04u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xe2u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x0cu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x09u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xd1u); if (rc != AMIVM_JIT_OK) return rc;

    rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc2u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xeau); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x0au); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xe2u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x02u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x09u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xd1u); if (rc != AMIVM_JIT_OK) return rc;

    rc = emit_and16_state_imm(code, sr_offset,
                              (uint16_t)~(update_x ? SR_XNZVC : SR_NZVC));
    if (rc != AMIVM_JIT_OK) return rc;
    return emit_or16_state_cx(code, sr_offset);
}

static int emit_cmp32_state_zero(struct amivm_jit_code *code, size_t offset)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xbfu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit8(code, 0x00u);
}

static int emit_addsub32_state_imm(struct amivm_jit_code *code,
                                   size_t offset, uint32_t value, int subtract)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, subtract ? 0xafu : 0x87u);
    if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, value);
}

static int emit_movzx16_state_eax(struct amivm_jit_code *code, size_t offset)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    rc = emit8(code, 0x0fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xb7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x87u); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, (uint32_t)offset);
}

static int emit_test_eax_imm(struct amivm_jit_code *code, uint32_t mask)
{
    int rc = emit8(code, 0xa9u);
    if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, mask);
}

static int emit_setcc_eax(struct amivm_jit_code *code, int set_if_zero)
{
    int rc;
    rc = emit8(code, 0x0fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, set_if_zero ? 0x94u : 0x95u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc0u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x0fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xb6u); if (rc != AMIVM_JIT_OK) return rc;
    return emit8(code, 0xc0u);
}

static int emit_condition_eax(struct amivm_jit_code *code, uint8_t condition)
{
    const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
    uint32_t mask = 0u;
    int set_if_zero = 0;
    int rc;

    if (condition == AMIVM_IR_CC_T) {
        rc = emit8(code, 0xb8u); if (rc != AMIVM_JIT_OK) return rc;
        return emit32(code, 1u);
    }

    rc = emit_movzx16_state_eax(code, sr_offset);
    if (rc != AMIVM_JIT_OK) return rc;

    switch (condition) {
    case AMIVM_IR_CC_HI: mask = SR_C | SR_Z; set_if_zero = 1; break;
    case AMIVM_IR_CC_LS: mask = SR_C | SR_Z; set_if_zero = 0; break;
    case AMIVM_IR_CC_CC: mask = SR_C; set_if_zero = 1; break;
    case AMIVM_IR_CC_CS: mask = SR_C; set_if_zero = 0; break;
    case AMIVM_IR_CC_NE: mask = SR_Z; set_if_zero = 1; break;
    case AMIVM_IR_CC_EQ: mask = SR_Z; set_if_zero = 0; break;
    case AMIVM_IR_CC_VC: mask = SR_V; set_if_zero = 1; break;
    case AMIVM_IR_CC_VS: mask = SR_V; set_if_zero = 0; break;
    case AMIVM_IR_CC_PL: mask = SR_N; set_if_zero = 1; break;
    case AMIVM_IR_CC_MI: mask = SR_N; set_if_zero = 0; break;
    case AMIVM_IR_CC_GE:
    case AMIVM_IR_CC_LT:
    case AMIVM_IR_CC_GT:
    case AMIVM_IR_CC_LE:
        rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xe8u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0x02u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0x31u); if (rc != AMIVM_JIT_OK) return rc;
        rc = emit8(code, 0xc8u); if (rc != AMIVM_JIT_OK) return rc;
        if (condition == AMIVM_IR_CC_GT || condition == AMIVM_IR_CC_LE) {
            rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0xe1u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, SR_Z); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0xc1u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0xe9u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0x01u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0x09u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit8(code, 0xc8u); if (rc != AMIVM_JIT_OK) return rc;
        }
        mask = SR_V;
        set_if_zero = condition == AMIVM_IR_CC_GE || condition == AMIVM_IR_CC_GT;
        break;
    default:
        return AMIVM_JIT_UNSUPPORTED;
    }

    rc = emit_test_eax_imm(code, mask);
    if (rc != AMIVM_JIT_OK) return rc;
    return emit_setcc_eax(code, set_if_zero);
}

static int emit_select_pc(struct amivm_jit_code *code,
                          uint32_t taken, uint32_t fallthrough)
{
    const size_t pc_offset = offsetof(struct amivm_cpu_state, pc);
    int rc;
    if (pc_offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;

    rc = emit8(code, 0xb9u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, fallthrough); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xbau); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, taken); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x85u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc0u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x0fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x45u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xcau); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x8fu); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, (uint32_t)pc_offset);
}

int amivm_jit_host_arch(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return AMIVM_JIT_ARCH_X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return AMIVM_JIT_ARCH_AARCH64;
#else
    return AMIVM_JIT_ARCH_NONE;
#endif
}

void amivm_jit_code_init(struct amivm_jit_code *code)
{
    if (code == NULL) return;
    memset(code, 0, sizeof(*code));
    code->arch = (enum amivm_jit_arch)amivm_jit_host_arch();
}

int amivm_jit_compile(const struct amivm_ir_block *block,
                      struct amivm_jit_code *code)
{
    size_t i;
    uint32_t final_pc;
    int terminal_pc = 0;
    int rc;

    if (block == NULL || code == NULL || block->op_count == 0u)
        return AMIVM_JIT_INVALID;
    amivm_jit_code_init(code);
    if (code->arch == AMIVM_JIT_ARCH_AARCH64) {
        const struct amivm_ir_op *op;
        size_t d_offset;
        uint32_t value;
        uint32_t insn;

        if (block->op_count != 1u) return AMIVM_JIT_UNSUPPORTED;
        op = &block->ops[0];
        if (op->opcode == AMIVM_IR_MOVEQ) {
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            value = (uint32_t)op->imm;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || d_offset > (4095u * 4u))
                return AMIVM_JIT_UNSUPPORTED;
            /* MOVZ/MOVK w9 materialize the 32-bit MOVEQ result. */
            insn = 0x52800009u | ((value & 0xffffu) << 5u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x72a00009u | (((value >> 16u) & 0xffffu) << 5u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            /* STR w9, [x0, #d_offset]. */
            insn = 0xb9000009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            /* MOVEQ updates N/Z and clears V/C while preserving X.
               Load/store SR as a halfword; immediates are known at compile time. */
            {
                const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
                uint16_t flags = 0u;
                uint16_t sr_clear = (uint16_t)~SR_NZVC;
                if ((value & 0x80000000u) != 0u) flags |= SR_N;
                if (value == 0u) flags |= SR_Z;
                if ((sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                    return AMIVM_JIT_UNSUPPORTED;
                /* LDRH w10, [x0, #sr_offset] */
                insn = 0x7940000au | (((uint32_t)sr_offset / 2u) << 10u);
                rc = emit32(code, insn);
                if (rc != AMIVM_JIT_OK) return rc;
                /* AND w10, w10, #0xfff0 (logical immediate). */
                insn = 0x121c2d4au;
                rc = emit32(code, insn);
                if (rc != AMIVM_JIT_OK) return rc;
                (void)sr_clear;
                if (flags != 0u) {
                    /* MOVZ w11, #flags; ORR w10,w10,w11. */
                    insn = 0x5280000bu | ((uint32_t)flags << 5u);
                    rc = emit32(code, insn);
                    if (rc != AMIVM_JIT_OK) return rc;
                    rc = emit32(code, 0x2a0b014au);
                    if (rc != AMIVM_JIT_OK) return rc;
                }
                /* STRH w10, [x0, #sr_offset] */
                insn = 0x7900000au | (((uint32_t)sr_offset / 2u) << 10u);
                rc = emit32(code, insn);
                if (rc != AMIVM_JIT_OK) return rc;
            }
        } else if (op->opcode == AMIVM_IR_CLR_L) {
            const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || d_offset > (4095u * 4u) ||
                (sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                return AMIVM_JIT_UNSUPPORTED;
            /* STR wzr, [x0, #d_offset]. */
            insn = 0xb900001fu | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            /* CLR.L clears N/V/C, sets Z and preserves X. */
            insn = 0x7940000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x121c2d4au); /* and w10,w10,#0xfff0 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x321e014au); /* orr w10,w10,#0x4 */
            if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7900000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
        } else if (op->opcode == AMIVM_IR_TST_L) {
            const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || d_offset > (4095u * 4u) ||
                (sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                return AMIVM_JIT_UNSUPPORTED;
            /* LDR w9, [x0,#d_offset]; derive 68k N/Z in w11. */
            insn = 0xb9400009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x7100013fu); /* cmp w9,#0 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f17ebu); /* cset w11,eq */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531f7d29u); /* lsr w9,w9,#31 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531e756bu); /* lsl w11,w11,#2 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531d7129u); /* lsl w9,w9,#3 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a09016bu); /* orr w11,w11,w9 */
            if (rc != AMIVM_JIT_OK) return rc;
            /* Preserve X and unrelated SR bits, clear NZVC, merge N/Z. */
            insn = 0x7940000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x121c2d4au); /* and w10,w10,#0xfff0 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); /* orr w10,w10,w11 */
            if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7900000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn);
            if (rc != AMIVM_JIT_OK) return rc;
        } else if (op->opcode == AMIVM_IR_ADDQ_L ||
                   op->opcode == AMIVM_IR_SUBQ_L) {
            const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
            uint32_t quick = (uint32_t)op->imm;
            if (op->reg >= 8u || quick == 0u || quick > 8u)
                return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || d_offset > (4095u * 4u) ||
                (sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                return AMIVM_JIT_UNSUPPORTED;
            /* LDR w9,[x0,#d]; ADDS/SUBS w9,w9,#quick; STR w9. */
            insn = 0xb9400009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            insn = (op->opcode == AMIVM_IR_ADDQ_L ? 0x31000129u : 0x71000129u) |
                   (quick << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0xb9000009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            /* Build 68k XNZVC from AArch64 NZCV. C is inverted for SUBS. */
            rc = emit32(code, 0x1a9f57eau); /* cset w10,mi (N) */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531d714au); /* lsl w10,w10,#3 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f17ebu); /* cset w11,eq (Z) */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531e756bu); /* lsl w11,w11,#2 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); /* orr w10,w10,w11 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f77ebu); /* cset w11,vs (V) */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531f796bu); /* lsl w11,w11,#1 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, op->opcode == AMIVM_IR_ADDQ_L ?
                        0x1a9f37ebu : 0x1a9f27ebu); /* cset cs / cc */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); /* merge C */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531c716bu); /* lsl w11,w11,#4 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); /* merge X */
            if (rc != AMIVM_JIT_OK) return rc;
            /* Preserve upper SR bits; replace XNZVC. */
            insn = 0x7940000bu | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x121b296bu); /* and w11,w11,#0xffe0 */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0a016bu); /* orr w11,w11,w10 */
            if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7900000bu | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
        } else if (op->opcode == AMIVM_IR_ADD_L ||
                   op->opcode == AMIVM_IR_SUB_L ||
                   op->opcode == AMIVM_IR_CMP_L) {
            const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
            size_t src_offset;
            int update_x = op->opcode != AMIVM_IR_CMP_L;
            if (op->reg >= 8u || op->src_reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            src_offset = offsetof(struct amivm_cpu_state, d) +
                         ((size_t)op->src_reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || (src_offset & 3u) != 0u ||
                d_offset > (4095u * 4u) || src_offset > (4095u * 4u) ||
                (sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                return AMIVM_JIT_UNSUPPORTED;
            insn = 0xb9400009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0xb940000au | (((uint32_t)src_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, op->opcode == AMIVM_IR_ADD_L ?
                        0x2b0a0129u : 0x6b0a0129u); /* adds/subs w9,w9,w10 */
            if (rc != AMIVM_JIT_OK) return rc;
            if (op->opcode != AMIVM_IR_CMP_L) {
                insn = 0xb9000009u | (((uint32_t)d_offset / 4u) << 10u);
                rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            }
            /* Convert host NZCV to 68k NZVC. SUB/CMP invert host C for borrow. */
            rc = emit32(code, 0x1a9f57eau); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531d714au); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f17ebu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531e756bu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f77ebu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531f796bu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, op->opcode == AMIVM_IR_ADD_L ?
                        0x1a9f37ebu : 0x1a9f27ebu); /* cs for add, cc for borrow */
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); if (rc != AMIVM_JIT_OK) return rc;
            if (update_x) {
                rc = emit32(code, 0x531c716bu); if (rc != AMIVM_JIT_OK) return rc;
                rc = emit32(code, 0x2a0b014au); if (rc != AMIVM_JIT_OK) return rc;
            }
            insn = 0x7940000bu | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            if (update_x) {
                rc = emit32(code, 0x121b296bu); /* clear XNZVC */
                if (rc != AMIVM_JIT_OK) return rc;
            } else {
                rc = emit32(code, 0x121c2d6bu); /* clear NZVC, preserve X */
                if (rc != AMIVM_JIT_OK) return rc;
            }
            rc = emit32(code, 0x2a0a016bu); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7900000bu | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
        } else if (op->opcode == AMIVM_IR_AND_L ||
                   op->opcode == AMIVM_IR_OR_L ||
                   op->opcode == AMIVM_IR_EOR_L) {
            const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
            size_t src_offset;
            if (op->reg >= 8u || op->src_reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            src_offset = offsetof(struct amivm_cpu_state, d) +
                         ((size_t)op->src_reg * sizeof(uint32_t));
            if ((d_offset & 3u) != 0u || (src_offset & 3u) != 0u ||
                d_offset > (4095u * 4u) || src_offset > (4095u * 4u) ||
                (sr_offset & 1u) != 0u || sr_offset > (4095u * 2u))
                return AMIVM_JIT_UNSUPPORTED;
            insn = 0xb9400009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0xb940000au | (((uint32_t)src_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            if (op->opcode == AMIVM_IR_AND_L) insn = 0x0a0a0129u;
            else if (op->opcode == AMIVM_IR_OR_L) insn = 0x2a0a0129u;
            else insn = 0x4a0a0129u;
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0xb9000009u | (((uint32_t)d_offset / 4u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            /* Logical ops set N/Z, clear V/C and preserve X. */
            rc = emit32(code, 0x7100013fu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x1a9f17ebu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531e756bu); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531f7d29u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x531d7129u); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a09016bu); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7940000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x121c2d4au); if (rc != AMIVM_JIT_OK) return rc;
            rc = emit32(code, 0x2a0b014au); if (rc != AMIVM_JIT_OK) return rc;
            insn = 0x7900000au | (((uint32_t)sr_offset / 2u) << 10u);
            rc = emit32(code, insn); if (rc != AMIVM_JIT_OK) return rc;
        } else if (op->opcode != AMIVM_IR_NOP) {
            return AMIVM_JIT_UNSUPPORTED;
        }
        rc = emit32(code, 0x52800020u); /* mov w0, #1 */
        if (rc != AMIVM_JIT_OK) return rc;
        rc = emit32(code, 0xd65f03c0u); /* ret */
        if (rc != AMIVM_JIT_OK) return rc;
        code->guest_instructions = 1u;
        code->guest_start_pc = block->guest_start_pc;
        code->guest_end_pc = block->guest_end_pc;
        return AMIVM_JIT_OK;
    }
    if (code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_UNSUPPORTED;

    for (i = 0u; i < block->op_count; ++i) {
        const struct amivm_ir_op *op = &block->ops[i];
        uint32_t value;
        size_t d_offset;
        size_t src_offset;

        switch (op->opcode) {
        case AMIVM_IR_NOP:
            break;
        case AMIVM_IR_MOVEQ:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            value = (uint32_t)op->imm;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_mov32_state_imm(code, d_offset, value);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_nz_flags(code, value);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_CLR_L:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_mov32_state_imm(code, d_offset, 0u);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_nz_flags(code, 0u);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_TST_L:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_cmp32_state_zero(code, d_offset);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_capture_nz_flags(code);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_ADDQ_L:
        case AMIVM_IR_SUBQ_L:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_addsub32_state_imm(code, d_offset, (uint32_t)op->imm,
                                         op->opcode == AMIVM_IR_SUBQ_L);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_capture_arithmetic_flags(code, 1);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_ADD_L:
        case AMIVM_IR_SUB_L:
        case AMIVM_IR_CMP_L:
        case AMIVM_IR_AND_L:
        case AMIVM_IR_OR_L:
        case AMIVM_IR_EOR_L:
            if (op->reg >= 8u || op->src_reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) +
                       ((size_t)op->reg * sizeof(uint32_t));
            src_offset = offsetof(struct amivm_cpu_state, d) +
                         ((size_t)op->src_reg * sizeof(uint32_t));
            rc = emit_mov32_state_eax(code, src_offset);
            if (rc != AMIVM_JIT_OK) return rc;
            switch (op->opcode) {
            case AMIVM_IR_ADD_L: rc = emit_alu32_state_eax(code, d_offset, 0x01u); break;
            case AMIVM_IR_SUB_L: rc = emit_alu32_state_eax(code, d_offset, 0x29u); break;
            case AMIVM_IR_CMP_L: rc = emit_alu32_state_eax(code, d_offset, 0x39u); break;
            case AMIVM_IR_AND_L: rc = emit_alu32_state_eax(code, d_offset, 0x21u); break;
            case AMIVM_IR_OR_L:  rc = emit_alu32_state_eax(code, d_offset, 0x09u); break;
            case AMIVM_IR_EOR_L: rc = emit_alu32_state_eax(code, d_offset, 0x31u); break;
            default: rc = AMIVM_JIT_INVALID; break;
            }
            if (rc != AMIVM_JIT_OK) return rc;
            if (op->opcode == AMIVM_IR_ADD_L || op->opcode == AMIVM_IR_SUB_L)
                rc = emit_capture_arithmetic_flags(code, 1);
            else if (op->opcode == AMIVM_IR_CMP_L)
                rc = emit_capture_arithmetic_flags(code, 0);
            else
                rc = emit_capture_nz_flags(code);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_BRANCH:
            if (i + 1u != block->op_count) return AMIVM_JIT_INVALID;
            value = op->guest_pc + 2u + (uint32_t)op->imm;
            rc = emit_mov32_state_imm(code, offsetof(struct amivm_cpu_state, pc), value);
            if (rc != AMIVM_JIT_OK) return rc;
            terminal_pc = 1;
            break;
        case AMIVM_IR_BRANCH_CC:
            if (i + 1u != block->op_count) return AMIVM_JIT_INVALID;
            rc = emit_condition_eax(code, op->condition);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_select_pc(code,
                                op->guest_pc + 2u + (uint32_t)op->imm,
                                op->guest_pc + 2u);
            if (rc != AMIVM_JIT_OK) return rc;
            terminal_pc = 1;
            break;
        default:
            return AMIVM_JIT_UNSUPPORTED;
        }
    }

    final_pc = block->guest_end_pc;
    if (!terminal_pc) {
        rc = emit_mov32_state_imm(code, offsetof(struct amivm_cpu_state, pc), final_pc);
        if (rc != AMIVM_JIT_OK) return rc;
    }
    rc = emit8(code, 0xb8u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, 1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc3u); if (rc != AMIVM_JIT_OK) return rc;

    code->guest_instructions = block->op_count;
    code->guest_start_pc = block->guest_start_pc;
    code->guest_end_pc = final_pc;
    return AMIVM_JIT_OK;
}

int amivm_jit_execute(const struct amivm_jit_code *code,
                      struct amivm_cpu_state *cpu)
{
#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
    typedef int (*jit_fn)(struct amivm_cpu_state *);
    void *mapping;
    void *entry;
    jit_fn fn;
    int rc;

    if (code == NULL || cpu == NULL || code->size == 0u ||
        code->arch != (enum amivm_jit_arch)amivm_jit_host_arch())
        return AMIVM_JIT_INVALID;
    mapping = mmap(NULL, code->size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return AMIVM_JIT_EXEC_UNAVAILABLE;
    memcpy(mapping, code->bytes, code->size);
    if (mprotect(mapping, code->size, PROT_READ | PROT_EXEC) != 0) {
        (void)munmap(mapping, code->size);
        return AMIVM_JIT_EXEC_UNAVAILABLE;
    }
    if (sizeof(fn) != sizeof(entry)) {
        (void)munmap(mapping, code->size);
        return AMIVM_JIT_EXEC_UNAVAILABLE;
    }
    entry = mapping;
    memcpy(&fn, &entry, sizeof(fn));
    rc = fn(cpu);
    (void)munmap(mapping, code->size);
    return rc;
#else
    (void)code;
    (void)cpu;
    return AMIVM_JIT_EXEC_UNAVAILABLE;
#endif
}
