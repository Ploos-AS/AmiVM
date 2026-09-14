#include "exec.h"

#include <string.h>

static size_t cache_index(uint32_t pc)
{
    return (size_t)((pc >> 1u) & (AMIVM_EXEC_CACHE_ENTRIES - 1u));
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

int amivm_exec_step(struct amivm_exec_engine *engine,
                    struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    struct amivm_exec_cache_entry *entry;
    uint32_t pc;
    size_t index;
    int rc;

    if (engine == NULL || cpu == NULL || vm == NULL ||
        engine->backend == NULL || engine->backend->step == NULL) return -1;

    pc = cpu->pc;
    index = cache_index(pc);
    entry = &engine->cache[index];
    if (entry->valid && entry->generation == engine->generation && entry->pc == pc) {
        engine->stats.cache_hits++;
    } else {
        engine->stats.cache_misses++;
        entry->pc = pc;
        entry->generation = engine->generation;
        entry->valid = 1;
    }

    rc = amivm_cpu_step(cpu, vm, engine->backend);
    if (rc > 0) engine->stats.instructions++;
    if (rc <= 0 || cpu->stopped) engine->stats.exits++;
    return rc;
}

int amivm_exec_run(struct amivm_exec_engine *engine,
                   struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint64_t instruction_budget)
{
    uint64_t i;
    int rc = 0;

    if (instruction_budget == 0u) return 0;
    for (i = 0u; i < instruction_budget; ++i) {
        rc = amivm_exec_step(engine, cpu, vm);
        if (rc <= 0 || cpu->stopped) return rc;
    }
    return rc;
}
