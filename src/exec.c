#include "exec.h"
#include "jit_helpers.h"

#include <stdlib.h>
#include <string.h>

#define EXEC_FETCH_WORDS 64u
#define EXEC_PAGE_SHIFT 12u

static size_t cache_index(uint32_t pc)
{
    return (size_t)((pc >> 1u) % AMIVM_EXEC_CACHE_SIZE);
}

static uint32_t page_number(uint32_t address)
{
    return address >> EXEC_PAGE_SHIFT;
}

static int same_page(uint32_t a, uint32_t b)
{
    return page_number(a) == page_number(b);
}

static int context_matches(const struct amivm_exec_cache_entry *entry,
                           const struct amivm_cpu_state *cpu)
{
    uint32_t root = (cpu->sr & 0x2000u) != 0u ? cpu->srp : cpu->urp;
    return entry->context_tc == cpu->tc && entry->context_root == root &&
           entry->context_supervisor == (((cpu->sr & 0x2000u) != 0u) ? 1 : 0);
}

static int dependencies_fresh(const struct amivm_exec_cache_entry *entry,
                              const struct amivm_vm *vm)
{
    size_t i;
    for (i = 0u; i < entry->dependency_count; ++i) {
        if (amivm_page_generation(vm, entry->dependencies[i].page) !=
            entry->dependencies[i].generation) return 0;
    }
    return 1;
}

static int entry_is_fresh(const struct amivm_exec_cache_entry *entry,
                          const struct amivm_cpu_state *cpu,
                          const struct amivm_vm *vm,
                          uint32_t pc, uint64_t generation)
{
    return entry != NULL && entry->valid && entry->generation == generation &&
           entry->pc == pc && context_matches(entry, cpu) &&
           dependencies_fresh(entry, vm);
}

static void release_entry(struct amivm_exec_cache_entry *entry)
{
    if (entry == NULL) return;
    amivm_jit_runtime_release(&entry->jit_runtime);
    entry->jit_valid = 0;
    entry->chain_valid = 0;
    entry->chain_alt_valid = 0;
}

void amivm_exec_init(struct amivm_exec_engine *engine,
                     const struct amivm_cpu_backend *backend)
{
    if (engine == NULL) return;
    memset(engine, 0, sizeof(*engine));
    engine->backend = backend;
    engine->generation = 1u;
}

void amivm_exec_reset(struct amivm_exec_engine *engine)
{
    size_t i;
    const struct amivm_cpu_backend *backend;
    if (engine == NULL) return;
    backend = engine->backend;
    for (i = 0u; i < AMIVM_EXEC_CACHE_SIZE; ++i) release_entry(&engine->cache[i]);
    memset(engine, 0, sizeof(*engine));
    engine->backend = backend;
    engine->generation = 1u;
}

void amivm_exec_invalidate_all(struct amivm_exec_engine *engine)
{
    size_t i;
    if (engine == NULL) return;
    for (i = 0u; i < AMIVM_EXEC_CACHE_SIZE; ++i) release_entry(&engine->cache[i]);
    engine->generation++;
    if (engine->generation == 0u) engine->generation = 1u;
}

static void add_dependency(struct amivm_exec_cache_entry *entry,
                           struct amivm_vm *vm, uint32_t address)
{
    uint32_t page = page_number(address);
    size_t i;
    for (i = 0u; i < entry->dependency_count; ++i)
        if (entry->dependencies[i].page == page) return;
    if (entry->dependency_count >= AMIVM_EXEC_MAX_DEPENDENCIES) return;
    entry->dependencies[entry->dependency_count].page = page;
    entry->dependencies[entry->dependency_count].generation = amivm_page_generation(vm, page);
    entry->dependency_count++;
}

static int fetch_word(struct amivm_exec_cache_entry *entry,
                      struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                      uint32_t logical, uint16_t *value)
{
    uint32_t p0, p1;
    int supervisor = (cpu->sr & 0x2000u) != 0u;
    if (amivm_mmu_translate(cpu, vm, logical, 0, supervisor, &p0) != 0 ||
        amivm_mmu_translate(cpu, vm, logical + 1u, 0, supervisor, &p1) != 0)
        return -1;
    add_dependency(entry, vm, p0);
    add_dependency(entry, vm, p1);
    *value = (uint16_t)(((uint16_t)amivm_read8(vm, p0) << 8u) | amivm_read8(vm, p1));
    return 0;
}

static int compile_ir_block(struct amivm_exec_cache_entry *entry,
                            struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t words[EXEC_FETCH_WORDS];
    size_t count = 0u;
    uint32_t logical = entry->pc;
    uint32_t root = (cpu->sr & 0x2000u) != 0u ? cpu->srp : cpu->urp;
    int rc;

    entry->dependency_count = 0u;
    entry->context_tc = cpu->tc;
    entry->context_root = root;
    entry->context_supervisor = (cpu->sr & 0x2000u) != 0u ? 1 : 0;

    while (count < EXEC_FETCH_WORDS) {
        if (fetch_word(entry, cpu, vm, logical, &words[count]) != 0) break;
        count++;
        logical += 2u;
    }
    if (count == 0u) return -1;

    rc = amivm_ir_decode_words(words, count, entry->pc, &entry->block);
    if (rc != 0) return rc;
    entry->ir_valid = 1;
    amivm_jit_code_init(&entry->jit);
    rc = amivm_jit_compile(&entry->block, &entry->jit);
    if (rc == AMIVM_JIT_OK) {
        amivm_jit_runtime_init(&entry->jit_runtime);
        if (amivm_jit_runtime_prepare(&entry->jit_runtime, &entry->jit) == AMIVM_JIT_OK)
            entry->jit_valid = 1;
    }
    return 0;
}

static int fallback_step(struct amivm_exec_engine *engine,
                         struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    int rc = engine->backend->step(cpu, vm);
    if (rc > 0) {
        engine->stats.fallbacks++;
        engine->stats.instructions++;
    }
    return rc;
}

static int execute_entry_limited(struct amivm_exec_engine *engine,
                                 struct amivm_exec_cache_entry *entry,
                                 struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                                 uint64_t remaining)
{
    const struct amivm_ir_block *block = &entry->block;
    struct amivm_ir_block prefix;
    struct amivm_jit_context context;
    uint64_t executed;
    int ir_rc;

    if (!entry->ir_valid || block->op_count == 0u) return fallback_step(engine, cpu, vm);

    if (remaining < block->op_count) {
        prefix = *block;
        prefix.op_count = (size_t)remaining;
        prefix.terminates = 0;
        prefix.guest_end_pc = prefix.ops[prefix.op_count - 1u].guest_pc +
                              prefix.ops[prefix.op_count - 1u].instruction_bytes;
        ir_rc = amivm_ir_execute(&prefix, cpu, vm);
        if (ir_rc < 0) return fallback_step(engine, cpu, vm);
        executed = prefix.op_count;
        engine->stats.ir_blocks++;
        engine->stats.ir_instructions += executed;
        engine->stats.instructions += executed;
        return 1;
    }

    if (entry->jit_valid) {
        amivm_jit_context_init(&context, cpu, vm);
        ir_rc = amivm_jit_runtime_execute_context(&entry->jit_runtime, cpu, &context);
        if (ir_rc > 0) {
            executed = entry->jit.guest_instructions;
            engine->stats.jit_blocks++;
            engine->stats.jit_instructions += executed;
            engine->stats.instructions += executed;
            return 1;
        }
        engine->stats.jit_fallbacks++;
    }

    ir_rc = amivm_ir_execute(block, cpu, vm);
    if (ir_rc < 0) return fallback_step(engine, cpu, vm);

    if (ir_rc == 2) {
        size_t prefix_count = block->op_count > 0u ? block->op_count - 1u : 0u;
        if (prefix_count != 0u) {
            engine->stats.ir_blocks++;
            engine->stats.ir_instructions += prefix_count;
            engine->stats.instructions += prefix_count;
        }
        return fallback_step(engine, cpu, vm);
    }

    executed = block->op_count;
    engine->stats.ir_blocks++;
    engine->stats.ir_instructions += executed;
    engine->stats.instructions += executed;
    return 1;
}

static int execute_entry(struct amivm_exec_engine *engine,
                         struct amivm_exec_cache_entry *entry,
                         struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    return execute_entry_limited(engine, entry, cpu, vm, (uint64_t)-1);
}

static struct amivm_exec_cache_entry *prepare_entry(struct amivm_exec_engine *engine,
                                                     struct amivm_cpu_state *cpu,
                                                     struct amivm_vm *vm)
{
    struct amivm_exec_cache_entry *entry;
    uint32_t pc = cpu->pc;
    size_t index = cache_index(pc);
    int base_match;
    int context_match;
    int deps_fresh;

    entry = &engine->cache[index];
    base_match = entry->valid && entry->generation == engine->generation && entry->pc == pc;
    context_match = base_match && context_matches(entry, cpu);
    deps_fresh = context_match && dependencies_fresh(entry, vm);

    if (deps_fresh) {
        engine->stats.cache_hits++;
        return entry;
    }

    if (base_match && !context_match) engine->stats.context_misses++;
    else if (context_match && !deps_fresh) engine->stats.stale_page_misses++;
    engine->stats.cache_misses++;
    release_entry(entry);
    memset(entry, 0, sizeof(*entry));
    entry->pc = pc;
    entry->generation = engine->generation;
    entry->valid = 1;
    (void)compile_ir_block(entry, cpu, vm);
    if (entry->jit_valid) engine->stats.jit_prepares++;
    return entry;
}

static int exec_step_internal(struct amivm_exec_engine *engine,
                              struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                              struct amivm_exec_cache_entry **executed)
{
    struct amivm_exec_cache_entry *entry;

    engine->stats.dispatches++;
    entry = prepare_entry(engine, cpu, vm);
    if (executed != NULL) *executed = entry;
    return execute_entry(engine, entry, cpu, vm);
}

static int block_has_chainable_successor(const struct amivm_exec_cache_entry *entry)
{
    const struct amivm_ir_op *op;

    if (entry == NULL || !entry->ir_valid || entry->block.op_count == 0u) return 0;
    op = &entry->block.ops[entry->block.op_count - 1u];
    if (op->opcode == AMIVM_IR_BRANCH || op->opcode == AMIVM_IR_BRANCH_CC ||
        op->opcode == AMIVM_IR_BSR || op->opcode == AMIVM_IR_RTS) return 1;

    return entry->block.terminates == 0;
}

static void record_chain_hit(struct amivm_exec_engine *engine,
                             const struct amivm_exec_cache_entry *source,
                             const struct amivm_exec_cache_entry *target)
{
    engine->stats.chain_hits++;
    engine->stats.cache_hits++;
    if (source->jit_valid && target->jit_valid) engine->stats.jit_chain_hits++;
}

static void record_chain_miss(struct amivm_exec_engine *engine,
                              const struct amivm_exec_cache_entry *source)
{
    engine->stats.chain_misses++;
    if (source != NULL && source->jit_valid) engine->stats.jit_chain_misses++;
}

static struct amivm_exec_cache_entry *lookup_chain_slot(struct amivm_exec_engine *engine,
                                                        struct amivm_exec_cache_entry *source,
                                                        struct amivm_cpu_state *cpu,
                                                        struct amivm_vm *vm,
                                                        uint32_t target_pc,
                                                        int alternate)
{
    struct amivm_exec_cache_entry *target;
    int *valid;
    uint32_t *pc;
    size_t *index;

    if (alternate) {
        valid = &source->chain_alt_valid;
        pc = &source->chain_alt_pc;
        index = &source->chain_alt_index;
    } else {
        valid = &source->chain_valid;
        pc = &source->chain_pc;
        index = &source->chain_index;
    }

    if (!*valid || *pc != target_pc) return NULL;
    target = &engine->cache[*index];
    if (entry_is_fresh(target, cpu, vm, target_pc, engine->generation)) return target;
    *valid = 0;
    return NULL;
}

static void remember_chain_slot(struct amivm_exec_cache_entry *source,
                                uint32_t target_pc, size_t target_index)
{
    if (!source->chain_valid || source->chain_pc == target_pc) {
        source->chain_valid = 1;
        source->chain_pc = target_pc;
        source->chain_index = target_index;
        return;
    }

    source->chain_alt_valid = 1;
    source->chain_alt_pc = target_pc;
    source->chain_alt_index = target_index;
}

static struct amivm_exec_cache_entry *resolve_chain(struct amivm_exec_engine *engine,
                                                    struct amivm_exec_cache_entry *source,
                                                    struct amivm_cpu_state *cpu,
                                                    struct amivm_vm *vm)
{
    struct amivm_exec_cache_entry *target;
    uint32_t target_pc = cpu->pc;
    size_t target_index;

    if (!block_has_chainable_successor(source)) return NULL;

    target = lookup_chain_slot(engine, source, cpu, vm, target_pc, 0);
    if (target == NULL)
        target = lookup_chain_slot(engine, source, cpu, vm, target_pc, 1);
    if (target != NULL) {
        record_chain_hit(engine, source, target);
        return target;
    }

    target_index = cache_index(target_pc);
    target = &engine->cache[target_index];
    if (!entry_is_fresh(target, cpu, vm, target_pc, engine->generation)) {
        record_chain_miss(engine, source);
        return NULL;
    }

    remember_chain_slot(source, target_pc, target_index);
    record_chain_hit(engine, source, target);
    return target;
}

int amivm_exec_step(struct amivm_exec_engine *engine,
                    struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    if (engine == NULL || cpu == NULL || vm == NULL ||
        engine->backend == NULL || engine->backend->step == NULL) return -1;
    return exec_step_internal(engine, cpu, vm, NULL);
}

int amivm_exec_run(struct amivm_exec_engine *engine,
                   struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint64_t instruction_budget)
{
    struct amivm_exec_cache_entry *source = NULL;
    struct amivm_exec_cache_entry *target;
    uint64_t before;
    uint64_t used;
    uint64_t remaining;
    int rc = 0;

    if (engine == NULL || cpu == NULL || vm == NULL ||
        engine->backend == NULL || engine->backend->step == NULL) return -1;
    if (instruction_budget == 0u) return 0;

    before = engine->stats.instructions;
    while ((used = engine->stats.instructions - before) < instruction_budget) {
        remaining = instruction_budget - used;
        target = resolve_chain(engine, source, cpu, vm);
        if (target != NULL) {
            rc = execute_entry_limited(engine, target, cpu, vm, remaining);
            source = target;
        } else {
            engine->stats.dispatches++;
            source = prepare_entry(engine, cpu, vm);
            rc = execute_entry_limited(engine, source, cpu, vm, remaining);
        }
        if (rc <= 0 || cpu->stopped) return rc;
    }
    return rc;
}
