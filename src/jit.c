#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "jit.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__x86_64__) && defined(__linux__)
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
#if defined(__x86_64__) && defined(__linux__)
    typedef int (*jit_fn)(struct amivm_cpu_state *);
    void *mapping;
    void *entry;
    jit_fn fn;
    int rc;

    if (code == NULL || cpu == NULL || code->size == 0u ||
        code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_INVALID;
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
