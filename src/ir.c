#include "ir.h"

#include <stdbool.h>
#include <string.h>

#include "vm.h"

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_S 0x2000u
#define SR_M 0x1000u

void amivm_ir_block_init(struct amivm_ir_block *block, uint32_t guest_pc)
{
    if (block == NULL) return;
    memset(block, 0, sizeof(*block));
    block->guest_start_pc = guest_pc;
    block->guest_end_pc = guest_pc;
}

static int emit(struct amivm_ir_block *block, enum amivm_ir_opcode opcode,
                uint8_t reg, uint8_t src_reg, uint8_t condition,
                uint8_t ea_mode, int32_t imm, uint32_t pc)
{
    struct amivm_ir_op *op;
    if (block->op_count >= AMIVM_IR_MAX_OPS) return -2;
    op = &block->ops[block->op_count++];
    memset(op, 0, sizeof(*op));
    op->opcode = opcode;
    op->reg = reg;
    op->src_reg = src_reg;
    op->condition = condition;
    op->ea_mode = ea_mode;
    op->index_scale = 1u;
    op->instruction_bytes = 2u;
    op->imm = imm;
    op->guest_pc = pc;
    return 0;
}

static void set_nz32(struct amivm_cpu_state *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x80000000u) != 0u) cpu->sr |= SR_N;
}

static void set_nz16(struct amivm_cpu_state *cpu, uint16_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x8000u) != 0u) cpu->sr |= SR_N;
}

static void set_nz8(struct amivm_cpu_state *cpu, uint8_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x80u) != 0u) cpu->sr |= SR_N;
}

static void set_logic32_flags(struct amivm_cpu_state *cpu, uint32_t value)
{
    set_nz32(cpu, value);
}

static void set_add32_flags(struct amivm_cpu_state *cpu, uint32_t lhs,
                            uint32_t rhs, uint32_t result)
{
    uint16_t flags = 0u;
    uint64_t wide = (uint64_t)lhs + (uint64_t)rhs;
    if (result == 0u) flags |= SR_Z;
    if ((result & 0x80000000u) != 0u) flags |= SR_N;
    if (((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0u) flags |= SR_V;
    if ((wide >> 32u) != 0u) flags |= (SR_C | SR_X);
    cpu->sr = (uint16_t)((cpu->sr & ~(SR_X | SR_N | SR_Z | SR_V | SR_C)) | flags);
}

static void set_sub32_flags(struct amivm_cpu_state *cpu, uint32_t lhs,
                            uint32_t rhs, uint32_t result, int update_x)
{
    uint16_t flags = 0u;
    uint16_t clear = (uint16_t)(SR_N | SR_Z | SR_V | SR_C);
    if (result == 0u) flags |= SR_Z;
    if ((result & 0x80000000u) != 0u) flags |= SR_N;
    if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) != 0u) flags |= SR_V;
    if (rhs > lhs) flags |= SR_C;
    if (update_x) {
        clear |= SR_X;
        if (rhs > lhs) flags |= SR_X;
    }
    cpu->sr = (uint16_t)((cpu->sr & ~clear) | flags);
}

static int condition_true(uint8_t condition, uint16_t sr)
{
    const int c = (sr & SR_C) != 0u;
    const int v = (sr & SR_V) != 0u;
    const int z = (sr & SR_Z) != 0u;
    const int n = (sr & SR_N) != 0u;
    switch (condition) {
    case AMIVM_IR_CC_T: return 1;
    case AMIVM_IR_CC_HI: return !c && !z;
    case AMIVM_IR_CC_LS: return c || z;
    case AMIVM_IR_CC_CC: return !c;
    case AMIVM_IR_CC_CS: return c;
    case AMIVM_IR_CC_NE: return !z;
    case AMIVM_IR_CC_EQ: return z;
    case AMIVM_IR_CC_VC: return !v;
    case AMIVM_IR_CC_VS: return v;
    case AMIVM_IR_CC_PL: return !n;
    case AMIVM_IR_CC_MI: return n;
    case AMIVM_IR_CC_GE: return n == v;
    case AMIVM_IR_CC_LT: return n != v;
    case AMIVM_IR_CC_GT: return !z && (n == v);
    case AMIVM_IR_CC_LE: return z || (n != v);
    default: return 0;
    }
}

static int emit_reg_alu(struct amivm_ir_block *block, uint16_t op, uint32_t pc)
{
    enum amivm_ir_opcode ir_op;
    uint8_t dst;
    uint8_t src;
    if ((op & 0xf1f8u) == 0x8080u) ir_op = AMIVM_IR_OR_L;
    else if ((op & 0xf1f8u) == 0x9080u) ir_op = AMIVM_IR_SUB_L;
    else if ((op & 0xf1f8u) == 0xb080u) ir_op = AMIVM_IR_CMP_L;
    else if ((op & 0xf1f8u) == 0xb180u) ir_op = AMIVM_IR_EOR_L;
    else if ((op & 0xf1f8u) == 0xc080u) ir_op = AMIVM_IR_AND_L;
    else if ((op & 0xf1f8u) == 0xd080u) ir_op = AMIVM_IR_ADD_L;
    else return 0;
    if (ir_op == AMIVM_IR_EOR_L) {
        src = (uint8_t)((op >> 9u) & 7u);
        dst = (uint8_t)(op & 7u);
    } else {
        dst = (uint8_t)((op >> 9u) & 7u);
        src = (uint8_t)(op & 7u);
    }
    return emit(block, ir_op, dst, src, 0u, AMIVM_IR_EA_NONE, 0, pc) == 0 ? 1 : -1;
}

static int memory_ea(uint8_t mode, uint8_t reg, int source, uint8_t *ea_mode)
{
    if (mode >= 2u && mode <= 6u) {
        *ea_mode = mode;
        return 1;
    }
    if (mode != 7u) return 0;
    if (reg == 0u) { *ea_mode = AMIVM_IR_EA_ABS_W; return 1; }
    if (reg == 1u) { *ea_mode = AMIVM_IR_EA_ABS_L; return 1; }
    if (source && reg == 2u) { *ea_mode = AMIVM_IR_EA_PC_D16; return 1; }
    if (source && reg == 3u) { *ea_mode = AMIVM_IR_EA_PC_D8_XN; return 1; }
    return 0;
}

static int control_ea(uint8_t mode, uint8_t reg, uint8_t *ea_mode)
{
    if (mode == 2u || mode == 5u || mode == 6u) {
        *ea_mode = mode;
        return 1;
    }
    if (mode != 7u) return 0;
    if (reg == 0u) { *ea_mode = AMIVM_IR_EA_ABS_W; return 1; }
    if (reg == 1u) { *ea_mode = AMIVM_IR_EA_ABS_L; return 1; }
    if (reg == 2u) { *ea_mode = AMIVM_IR_EA_PC_D16; return 1; }
    if (reg == 3u) { *ea_mode = AMIVM_IR_EA_PC_D8_XN; return 1; }
    return 0;
}

static enum amivm_ir_opcode load_opcode(unsigned width)
{
    return width == 1u ? AMIVM_IR_LOAD_B :
           width == 2u ? AMIVM_IR_LOAD_W : AMIVM_IR_LOAD_L;
}

static enum amivm_ir_opcode store_opcode(unsigned width)
{
    return width == 1u ? AMIVM_IR_STORE_B :
           width == 2u ? AMIVM_IR_STORE_W : AMIVM_IR_STORE_L;
}

static int consume_ea_extension(struct amivm_ir_op *ir, const uint16_t *words,
                                size_t word_count, size_t *index)
{
    if (ir->ea_mode == AMIVM_IR_EA_D16_AN || ir->ea_mode == AMIVM_IR_EA_ABS_W ||
        ir->ea_mode == AMIVM_IR_EA_PC_D16) {
        if (*index + 1u >= word_count) return -1;
        (*index)++;
        ir->imm = (int16_t)words[*index];
        ir->instruction_bytes = 4u;
    } else if (ir->ea_mode == AMIVM_IR_EA_ABS_L) {
        uint32_t value;
        if (*index + 2u >= word_count) return -1;
        value = ((uint32_t)words[*index + 1u] << 16u) | words[*index + 2u];
        *index += 2u;
        ir->imm = (int32_t)value;
        ir->instruction_bytes = 6u;
    } else if (ir->ea_mode == AMIVM_IR_EA_D8_AN_XN ||
               ir->ea_mode == AMIVM_IR_EA_PC_D8_XN) {
        struct amivm_ea_index_extension ext;
        int rc;
        size_t start = *index + 1u;
        if (start >= word_count) return -1;
        rc = amivm_ea_parse_index_extension(&words[start], word_count - start, &ext);
        if (rc == AMIVM_EA_PARSE_TRUNCATED) return -1;
        if (rc != AMIVM_EA_PARSE_OK) return -2;
        ir->full_format = ext.full_format;
        ir->index_is_addr = ext.index_is_addr;
        ir->index_reg = ext.index_reg;
        ir->index_long = ext.index_long;
        ir->index_scale = ext.index_scale;
        ir->base_suppress = ext.base_suppress;
        ir->index_suppress = ext.index_suppress;
        ir->indirect_mode = ext.indirect_mode;
        ir->imm = ext.full_format ? ext.base_displacement : ext.brief_displacement;
        ir->base_displacement = ext.base_displacement;
        ir->outer_displacement = ext.outer_displacement;
        ir->instruction_bytes = (uint8_t)(2u + ext.words_consumed * 2u);
        *index += ext.words_consumed;
    }
    return 0;
}

static int emit_move_memory(struct amivm_ir_block *block,
                            const uint16_t *words, size_t word_count,
                            size_t *index, uint32_t pc)
{
    uint16_t op = words[*index];
    uint8_t src_mode = (uint8_t)((op >> 3u) & 7u);
    uint8_t src_reg = (uint8_t)(op & 7u);
    uint8_t dst_mode = (uint8_t)((op >> 6u) & 7u);
    uint8_t dst_reg = (uint8_t)((op >> 9u) & 7u);
    uint8_t ea_mode;
    uint8_t data_reg;
    uint8_t addr_reg = 0u;
    unsigned width;
    enum amivm_ir_opcode ir_op;
    struct amivm_ir_op *ir;
    int rc;

    switch ((op >> 12u) & 0x0fu) {
    case 1u: width = 1u; break;
    case 2u: width = 4u; break;
    case 3u: width = 2u; break;
    default: return 0;
    }

    if (dst_mode == 0u && memory_ea(src_mode, src_reg, 1, &ea_mode)) {
        data_reg = dst_reg;
        addr_reg = src_reg;
        ir_op = load_opcode(width);
    } else if (src_mode == 0u && memory_ea(dst_mode, dst_reg, 0, &ea_mode)) {
        data_reg = src_reg;
        addr_reg = dst_reg;
        ir_op = store_opcode(width);
    } else {
        return 0;
    }

    if (emit(block, ir_op, data_reg, addr_reg, 0u, ea_mode, 0, pc) != 0) return -1;
    ir = &block->ops[block->op_count - 1u];
    rc = consume_ea_extension(ir, words, word_count, index);
    if (rc != 0) {
        block->op_count--;
        return rc == -2 ? 0 : -1;
    }
    return 1;
}

static int emit_movea(struct amivm_ir_block *block, const uint16_t *words,
                      size_t word_count, size_t *index, uint32_t pc)
{
    uint16_t op = words[*index];
    uint8_t top = (uint8_t)((op >> 12u) & 0x0fu);
    uint8_t dst_mode = (uint8_t)((op >> 6u) & 7u);
    uint8_t dst_reg = (uint8_t)((op >> 9u) & 7u);
    uint8_t src_mode = (uint8_t)((op >> 3u) & 7u);
    uint8_t src_reg = (uint8_t)(op & 7u);
    uint8_t ea_mode;
    struct amivm_ir_op *ir;
    int rc;

    if ((top != 2u && top != 3u) || dst_mode != 1u) return 0;
    if (!memory_ea(src_mode, src_reg, 1, &ea_mode)) return 0;
    if (emit(block, top == 3u ? AMIVM_IR_MOVEA_W : AMIVM_IR_MOVEA_L,
             dst_reg, src_reg, 0u, ea_mode, 0, pc) != 0) return -1;
    ir = &block->ops[block->op_count - 1u];
    rc = consume_ea_extension(ir, words, word_count, index);
    if (rc != 0) {
        block->op_count--;
        return rc == -2 ? 0 : -1;
    }
    return 1;
}

static int emit_lea(struct amivm_ir_block *block, const uint16_t *words,
                    size_t word_count, size_t *index, uint32_t pc)
{
    uint16_t op = words[*index];
    uint8_t src_mode;
    uint8_t src_reg;
    uint8_t dst_reg;
    uint8_t ea_mode;
    struct amivm_ir_op *ir;
    int rc;

    if ((op & 0xf1c0u) != 0x41c0u) return 0;
    src_mode = (uint8_t)((op >> 3u) & 7u);
    src_reg = (uint8_t)(op & 7u);
    dst_reg = (uint8_t)((op >> 9u) & 7u);
    if (!control_ea(src_mode, src_reg, &ea_mode)) return 0;
    if (emit(block, AMIVM_IR_LEA, dst_reg, src_reg, 0u, ea_mode, 0, pc) != 0) return -1;
    ir = &block->ops[block->op_count - 1u];
    rc = consume_ea_extension(ir, words, word_count, index);
    if (rc != 0) {
        block->op_count--;
        return rc == -2 ? 0 : -1;
    }
    return 1;
}

int amivm_ir_decode_words(struct amivm_ir_block *block,
                          const uint16_t *words, size_t word_count)
{
    size_t i;
    uint32_t pc;
    if (block == NULL || words == NULL || word_count == 0u) return -1;
    pc = block->guest_start_pc;

    for (i = 0u; i < word_count; ++i) {
        uint16_t op = words[i];
        int rc;

        rc = emit_lea(block, words, word_count, &i, pc);
        if (rc < 0) return -3;
        if (rc > 0) {
            pc += block->ops[block->op_count - 1u].instruction_bytes;
            continue;
        }
        rc = emit_movea(block, words, word_count, &i, pc);
        if (rc < 0) return -3;
        if (rc > 0) {
            pc += block->ops[block->op_count - 1u].instruction_bytes;
            continue;
        }
        rc = emit_move_memory(block, words, word_count, &i, pc);
        if (rc < 0) return -3;
        if (rc > 0) {
            pc += block->ops[block->op_count - 1u].instruction_bytes;
            continue;
        }

        if (op == 0x4e71u) {
            if (emit(block, AMIVM_IR_NOP, 0u, 0u, 0u, AMIVM_IR_EA_NONE, 0, pc) != 0) return -2;
            pc += 2u;
            continue;
        }
        if ((op & 0xf100u) == 0x7000u) {
            uint8_t reg = (uint8_t)((op >> 9u) & 7u);
            int8_t imm8 = (int8_t)(op & 0xffu);
            if (emit(block, AMIVM_IR_MOVEQ, reg, 0u, 0u, AMIVM_IR_EA_NONE,
                     (int32_t)imm8, pc) != 0) return -2;
            pc += 2u;
            continue;
        }
        if ((op & 0xfff8u) == 0x4280u) {
            if (emit(block, AMIVM_IR_CLR_L, (uint8_t)(op & 7u), 0u, 0u,
                     AMIVM_IR_EA_NONE, 0, pc) != 0) return -2;
            pc += 2u;
            continue;
        }
        if ((op & 0xfff8u) == 0x4a80u) {
            if (emit(block, AMIVM_IR_TST_L, (uint8_t)(op & 7u), 0u, 0u,
                     AMIVM_IR_EA_NONE, 0, pc) != 0) return -2;
            pc += 2u;
            continue;
        }
        if ((op & 0xf1f8u) == 0x5080u || (op & 0xf1f8u) == 0x5180u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t quick = (uint8_t)((op >> 9u) & 7u);
            if (quick == 0u) quick = 8u;
            if (emit(block, (op & 0x0100u) != 0u ? AMIVM_IR_SUBQ_L : AMIVM_IR_ADDQ_L,
                     reg, 0u, 0u, AMIVM_IR_EA_NONE, (int32_t)quick, pc) != 0) return -2;
            pc += 2u;
            continue;
        }
        rc = emit_reg_alu(block, op, pc);
        if (rc < 0) return -2;
        if (rc > 0) {
            pc += 2u;
            continue;
        }
        if ((op & 0xf000u) == 0x6000u) {
            uint8_t condition = (uint8_t)((op >> 8u) & 0x0fu);
            int8_t disp8 = (int8_t)(op & 0xffu);
            if (disp8 == 0 || condition == 1u) goto unsupported;
            if (condition == 0u) {
                if (emit(block, AMIVM_IR_BRANCH, 0u, 0u, AMIVM_IR_CC_T,
                         AMIVM_IR_EA_NONE, (int32_t)disp8, pc) != 0) return -2;
            } else {
                if (emit(block, AMIVM_IR_BRANCH_CC, 0u, 0u, condition,
                         AMIVM_IR_EA_NONE, (int32_t)disp8, pc) != 0) return -2;
            }
            block->terminates = 1;
            pc += 2u;
            block->guest_end_pc = pc;
            return 0;
        }

unsupported:
        if (emit(block, AMIVM_IR_EXIT, 0u, 0u, 0u, AMIVM_IR_EA_NONE,
                 (int32_t)op, pc) != 0) return -2;
        block->terminates = 1;
        pc += 2u;
        block->guest_end_pc = pc;
        return 1;
    }
    block->guest_end_pc = pc;
    return 0;
}

static bool supervisor_mode(const struct amivm_cpu_state *cpu)
{
    return (cpu->sr & SR_S) != 0u;
}

static void sync_a7_bank(struct amivm_cpu_state *cpu)
{
    if ((cpu->sr & SR_S) == 0u) cpu->usp = cpu->a[7];
    else if ((cpu->sr & SR_M) != 0u) cpu->msp = cpu->a[7];
    else cpu->isp = cpu->a[7];
}

static int ir_translate(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, bool write, unsigned width,
                        uint32_t physical[4])
{
    unsigned i;
    if (width > 1u && (logical & 1u) != 0u) return -1;
    for (i = 0u; i < width; ++i) {
        if (amivm_mmu_translate(cpu, vm, logical + i, write,
                                supervisor_mode(cpu), &physical[i]) != AMIVM_MMU_OK)
            return -1;
    }
    return 0;
}

static int ir_read(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint32_t logical, unsigned width, uint32_t *value)
{
    uint32_t physical[4];
    uint8_t b[4] = {0u, 0u, 0u, 0u};
    unsigned i;
    uint32_t result = 0u;
    if (value == NULL || ir_translate(cpu, vm, logical, false, width, physical) != 0) return -1;
    for (i = 0u; i < width; ++i) {
        if (!amivm_read8(vm, physical[i], &b[i])) return -1;
        result = (result << 8u) | b[i];
    }
    *value = result;
    return 0;
}

static int ir_write(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                    uint32_t logical, unsigned width, uint32_t value)
{
    uint32_t physical[4];
    unsigned i;
    if (ir_translate(cpu, vm, logical, true, width, physical) != 0) return -1;
    for (i = 0u; i < width; ++i) {
        unsigned shift = (width - 1u - i) * 8u;
        if (!amivm_write8(vm, physical[i], (uint8_t)(value >> shift))) return -1;
    }
    return 0;
}

static unsigned memory_width(enum amivm_ir_opcode opcode)
{
    switch (opcode) {
    case AMIVM_IR_LOAD_B:
    case AMIVM_IR_STORE_B: return 1u;
    case AMIVM_IR_LOAD_W:
    case AMIVM_IR_STORE_W: return 2u;
    default: return 4u;
    }
}

static unsigned ea_step(const struct amivm_ir_op *op, unsigned width)
{
    if (width == 1u && op->src_reg == 7u) return 2u;
    return width;
}

static int32_t index_value(const struct amivm_ir_op *op,
                           const struct amivm_cpu_state *cpu)
{
    uint32_t raw;
    int32_t value;
    if (op->full_format && op->index_suppress) return 0;
    raw = op->index_is_addr ? cpu->a[op->index_reg] : cpu->d[op->index_reg];
    value = op->index_long ? (int32_t)raw : (int32_t)(int16_t)(raw & 0xffffu);
    return value * (int32_t)op->index_scale;
}

static uint32_t indexed_base(const struct amivm_ir_op *op,
                             const struct amivm_cpu_state *cpu)
{
    if (op->full_format && op->base_suppress) return 0u;
    if (op->ea_mode == AMIVM_IR_EA_PC_D8_XN) return op->guest_pc + 2u;
    return cpu->a[op->src_reg];
}

static uint32_t ea_address_direct(const struct amivm_ir_op *op,
                                  const struct amivm_cpu_state *cpu,
                                  unsigned width)
{
    uint32_t base;
    unsigned step = ea_step(op, width);
    switch (op->ea_mode) {
    case AMIVM_IR_EA_ABS_W:
        return (uint32_t)(int32_t)(int16_t)op->imm;
    case AMIVM_IR_EA_ABS_L:
        return (uint32_t)op->imm;
    case AMIVM_IR_EA_PC_D16:
        return (uint32_t)((int64_t)(op->guest_pc + 2u) + (int64_t)(int16_t)op->imm);
    case AMIVM_IR_EA_PC_D8_XN:
        base = indexed_base(op, cpu);
        return (uint32_t)((int64_t)base +
                          (int64_t)(op->full_format ? op->base_displacement : (int8_t)op->imm) +
                          (int64_t)index_value(op, cpu));
    default:
        break;
    }
    base = cpu->a[op->src_reg];
    switch (op->ea_mode) {
    case AMIVM_IR_EA_AN:
    case AMIVM_IR_EA_POSTINC:
        return base;
    case AMIVM_IR_EA_PREDEC:
        return base - step;
    case AMIVM_IR_EA_D16_AN:
        return (uint32_t)((int64_t)base + (int64_t)(int16_t)op->imm);
    case AMIVM_IR_EA_D8_AN_XN:
        base = indexed_base(op, cpu);
        return (uint32_t)((int64_t)base +
                          (int64_t)(op->full_format ? op->base_displacement : (int8_t)op->imm) +
                          (int64_t)index_value(op, cpu));
    default:
        return base;
    }
}

static int resolve_ea(const struct amivm_ir_op *op,
                      struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                      unsigned width, uint32_t *address)
{
    uint32_t base;
    uint32_t pointer_address;
    uint32_t pointer;
    int32_t index;

    if (address == NULL) return -1;
    if (!op->full_format || op->indirect_mode == AMIVM_EA_INDIRECT_NONE) {
        *address = ea_address_direct(op, cpu, width);
        return 0;
    }
    if (vm == NULL) return -1;

    base = indexed_base(op, cpu);
    index = index_value(op, cpu);
    if (op->indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED)
        pointer_address = (uint32_t)((int64_t)base + (int64_t)op->base_displacement + index);
    else
        pointer_address = (uint32_t)((int64_t)base + (int64_t)op->base_displacement);

    if (ir_read(cpu, vm, pointer_address, 4u, &pointer) != 0) return -1;

    if (op->indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED)
        *address = (uint32_t)((int64_t)pointer + (int64_t)op->outer_displacement);
    else
        *address = (uint32_t)((int64_t)pointer + index + (int64_t)op->outer_displacement);
    return 0;
}

static void finish_ea_update(const struct amivm_ir_op *op,
                             struct amivm_cpu_state *cpu, uint32_t address,
                             unsigned width)
{
    unsigned step = ea_step(op, width);
    if (op->ea_mode == AMIVM_IR_EA_POSTINC)
        cpu->a[op->src_reg] += step;
    else if (op->ea_mode == AMIVM_IR_EA_PREDEC)
        cpu->a[op->src_reg] = address;
    if (op->src_reg == 7u &&
        (op->ea_mode == AMIVM_IR_EA_POSTINC || op->ea_mode == AMIVM_IR_EA_PREDEC))
        sync_a7_bank(cpu);
}

static int is_load(enum amivm_ir_opcode opcode)
{
    return opcode == AMIVM_IR_LOAD_B || opcode == AMIVM_IR_LOAD_W ||
           opcode == AMIVM_IR_LOAD_L;
}

static int is_store(enum amivm_ir_opcode opcode)
{
    return opcode == AMIVM_IR_STORE_B || opcode == AMIVM_IR_STORE_W ||
           opcode == AMIVM_IR_STORE_L;
}

int amivm_ir_execute(const struct amivm_ir_block *block,
                     struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    size_t i;
    if (block == NULL || cpu == NULL) return -1;
    cpu->pc = block->guest_start_pc;

    for (i = 0u; i < block->op_count; ++i) {
        const struct amivm_ir_op *op = &block->ops[i];
        uint32_t lhs, rhs, result, address;
        unsigned width;
        uint32_t instruction_bytes = op->instruction_bytes;

        if (op->reg >= 8u || op->src_reg >= 8u || op->index_reg >= 8u) {
            if (op->opcode != AMIVM_IR_NOP && op->opcode != AMIVM_IR_BRANCH &&
                op->opcode != AMIVM_IR_BRANCH_CC && op->opcode != AMIVM_IR_EXIT)
                return -2;
        }

        if (is_load(op->opcode) || is_store(op->opcode)) {
            if (vm == NULL) return -4;
            width = memory_width(op->opcode);
            if (resolve_ea(op, cpu, vm, width, &address) != 0) return -4;
            if (is_load(op->opcode)) {
                if (ir_read(cpu, vm, address, width, &result) != 0) return -4;
                if (width == 1u) {
                    cpu->d[op->reg] = (cpu->d[op->reg] & 0xffffff00u) | (result & 0xffu);
                    set_nz8(cpu, (uint8_t)result);
                } else if (width == 2u) {
                    cpu->d[op->reg] = (cpu->d[op->reg] & 0xffff0000u) | (result & 0xffffu);
                    set_nz16(cpu, (uint16_t)result);
                } else {
                    cpu->d[op->reg] = result;
                    set_nz32(cpu, result);
                }
            } else {
                if (ir_write(cpu, vm, address, width, cpu->d[op->reg]) != 0) return -4;
            }
            finish_ea_update(op, cpu, address, width);
            cpu->pc = op->guest_pc + instruction_bytes;
            continue;
        }

        switch (op->opcode) {
        case AMIVM_IR_NOP:
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_MOVEQ:
            cpu->d[op->reg] = (uint32_t)op->imm;
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_MOVEA_W:
        case AMIVM_IR_MOVEA_L:
            if (vm == NULL) return -4;
            width = op->opcode == AMIVM_IR_MOVEA_W ? 2u : 4u;
            if (resolve_ea(op, cpu, vm, width, &address) != 0) return -4;
            if (ir_read(cpu, vm, address, width, &result) != 0) return -4;
            cpu->a[op->reg] = width == 2u ? (uint32_t)(int32_t)(int16_t)result : result;
            if (op->reg == 7u) sync_a7_bank(cpu);
            cpu->pc = op->guest_pc + instruction_bytes;
            break;
        case AMIVM_IR_LEA:
            if (resolve_ea(op, cpu, vm, 4u, &address) != 0) return -4;
            cpu->a[op->reg] = address;
            if (op->reg == 7u) sync_a7_bank(cpu);
            cpu->pc = op->guest_pc + instruction_bytes;
            break;
        case AMIVM_IR_CLR_L:
            cpu->d[op->reg] = 0u;
            set_nz32(cpu, 0u);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_TST_L:
            set_nz32(cpu, cpu->d[op->reg]);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_ADDQ_L:
            lhs = cpu->d[op->reg]; rhs = (uint32_t)op->imm; result = lhs + rhs;
            cpu->d[op->reg] = result; set_add32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_SUBQ_L:
            lhs = cpu->d[op->reg]; rhs = (uint32_t)op->imm; result = lhs - rhs;
            cpu->d[op->reg] = result; set_sub32_flags(cpu, lhs, rhs, result, 1);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_ADD_L:
            lhs = cpu->d[op->reg]; rhs = cpu->d[op->src_reg]; result = lhs + rhs;
            cpu->d[op->reg] = result; set_add32_flags(cpu, lhs, rhs, result);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_SUB_L:
            lhs = cpu->d[op->reg]; rhs = cpu->d[op->src_reg]; result = lhs - rhs;
            cpu->d[op->reg] = result; set_sub32_flags(cpu, lhs, rhs, result, 1);
            cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_CMP_L:
            lhs = cpu->d[op->reg]; rhs = cpu->d[op->src_reg]; result = lhs - rhs;
            set_sub32_flags(cpu, lhs, rhs, result, 0); cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_AND_L:
            result = cpu->d[op->reg] & cpu->d[op->src_reg]; cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result); cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_OR_L:
            result = cpu->d[op->reg] | cpu->d[op->src_reg]; cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result); cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_EOR_L:
            result = cpu->d[op->reg] ^ cpu->d[op->src_reg]; cpu->d[op->reg] = result;
            set_logic32_flags(cpu, result); cpu->pc = op->guest_pc + 2u;
            break;
        case AMIVM_IR_BRANCH:
            cpu->pc = op->guest_pc + 2u + (uint32_t)op->imm;
            return 1;
        case AMIVM_IR_BRANCH_CC:
            cpu->pc = condition_true(op->condition, cpu->sr)
                          ? op->guest_pc + 2u + (uint32_t)op->imm
                          : op->guest_pc + 2u;
            return 1;
        case AMIVM_IR_EXIT:
            cpu->pc = op->guest_pc;
            return 2;
        default:
            return -3;
        }
    }
    return 0;
}
