#include "exec.h"

#include <string.h>

#include "vm.h"

static size_t cache_index(uint32_t pc)
{
    return (size_t)((pc >> 1u) & (AMIVM_EXEC_CACHE_ENTRIES - 1u));
}

static int read16_physical(struct amivm_vm *vm, uint32_t addr, uint16_t *value)
{
    uint8_t hi, lo;
    if (!amivm_read8(vm, addr, &hi) || !amivm_read8(vm, addr + 1u, &lo)) return -1;
    *value = (uint16_t)(((uint16_t)hi << 8u) | lo);
    return 0;
}

static int compile_ir_block(struct amivm_exec_cache_entry *entry,
                            struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t words[AMIVM_EXEC_DECODE_WORDS];
    size_t count = 0u;
    uint32_t pc = cpu->pc;
    int rc;

    /* M2.12 IR translation is deliberately disabled while the MMU is active.
     * The reference interpreter remains authoritative for translated fetches.
     */
    if ((cpu->tc & 0x80000000u) != 0u) return 0;

    while (count < AMIVM_EXEC_DECODE_WORDS) {
        if (read16_physical(vm, pc + (uint32_t)(count * 2u), &words[count]) != 0) break;
        count++;
    }
    if (count == 0u) return 0;

    amivm_ir_block_init(&entry->block, pc);
    rc = amivm_ir_decode_words(&entry->block, words, count);
    if (rc < 0 || entry->block.op_count == 0u) return 0;
    entry->ir_valid = 1;
    return 1;
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
    const struct amivm_cpu_backend *backend;
    if (engine == NULL) return;
    backend = engine->backend;
    memset(engine, 0, sizeof(*engine));
    engine->backend = backend;
    engine->generation = 1u;
}

void amivm_exec_invalidate_all(struct amivm_exec_engine *engine)
{
    if (engine == NULL) return;
    engine->generation++;
    if (engine->generation == 0u) {
        memset(engine->cache, 0, sizeof(engine->cache));
        engine->generation = 1u;
    }
}

static int fallback_step(struct amivm_exec_engine *engine,
                         struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    int rc;
    engine->stats.fallbacks++;
    rc = amivm_cpu_step(cpu, vm, engine->backend);
    if (rc > 0) engine->stats.instructions++;
    if (rc <= 0 || cpu->stopped) engine->stats.exits++;
    return rc;
}

int amivm_exec_step(struct amivm_exec_engine *engine,
                    struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    struct amivm_exec_cache_entry *entry;
    uint32_t pc;
    size_t index;
    int ir_rc;

    if (engine == NULL || cpu == NULL || vm == NULL ||
        engine->backend == NULL || engine->backend->step == NULL) return -1;

    pc = cpu->pc;
    index = cache_index(pc);
    entry = &engine->cache[index];
    if (entry->valid && entry->generation == engine->generation && entry->pc == pc) {
        engine->stats.cache_hits++;
    } else {
        engine->stats.cache_misses++;
        memset(entry, 0, sizeof(*entry));
        entry->pc = pc;
        entry->generation = engine->generation;
        entry->valid = 1;
        (void)compile_ir_block(entry, cpu, vm);
    }

    if (!entry->ir_valid) return fallback_step(engine, cpu, vm);

    ir_rc = amivm_ir_execute(&entry->block, cpu);
    if (ir_rc < 0) return fallback_step(engine, cpu, vm);

    if (ir_rc == 2) {
        /* IR EXIT leaves PC at the unsupported guest instruction. */
        return fallback_step(engine, cpu, vm);
    }

    engine->stats.ir_blocks++;
    engine->stats.ir_instructions += entry->block.op_count;
    engine->stats.instructions += entry->block.op_count;
    return 1;
}

int amivm_exec_run(struct amivm_exec_engine *engine,
                   struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint64_t instruction_budget)
{
    uint64_t before;
    int rc = 0;

    if (instruction_budget == 0u) return 0;
    before = engine != NULL ? engine->stats.instructions : 0u;
    while (engine != NULL && engine->stats.instructions - before < instruction_budget) {
        rc = amivm_exec_step(engine, cpu, vm);
        if (rc <= 0 || cpu->stopped) return rc;
    }
    return rc;
}
