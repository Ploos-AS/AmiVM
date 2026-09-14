#include "exec.h"

#include <stdbool.h>
#include <string.h>

#include "vm.h"

#define SR_SUPERVISOR 0x2000u
#define TC_ENABLE 0x80000000u
#define PAGE_MASK 0xfffff000u
#define DESC_VALID 0x00000001u

static size_t cache_index(uint32_t pc)
{
    return (size_t)((pc >> 1u) & (AMIVM_EXEC_CACHE_ENTRIES - 1u));
}

static int read32_physical(struct amivm_vm *vm, uint32_t addr, uint32_t *value)
{
    uint8_t b0, b1, b2, b3;
    if (value == NULL || !amivm_read8(vm, addr, &b0) ||
        !amivm_read8(vm, addr + 1u, &b1) ||
        !amivm_read8(vm, addr + 2u, &b2) ||
        !amivm_read8(vm, addr + 3u, &b3)) return -1;
    *value = ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
             ((uint32_t)b2 << 8u) | (uint32_t)b3;
    return 0;
}

static int track_ram_dependency(struct amivm_exec_cache_entry *entry,
                                struct amivm_vm *vm, uint32_t physical)
{
    uint32_t page_base = physical & PAGE_MASK;
    uint64_t generation;
    size_t i;

    if (!amivm_ram_page_generation(vm, physical, &generation)) return 0;
    for (i = 0u; i < entry->dep_count; ++i) {
        if (entry->deps[i].page_base == page_base) return 0;
    }
    if (entry->dep_count >= AMIVM_EXEC_DEP_PAGES) return -1;
    entry->deps[entry->dep_count].page_base = page_base;
    entry->deps[entry->dep_count].generation = generation;
    entry->dep_count++;
    return 0;
}

static int track_mmu_dependencies(struct amivm_exec_cache_entry *entry,
                                  struct amivm_cpu_state *cpu,
                                  struct amivm_vm *vm, uint32_t logical)
{
    uint32_t root;
    uint32_t l1_addr;
    uint32_t l1_desc;
    uint32_t l2_base;
    bool supervisor;

    if ((cpu->tc & TC_ENABLE) == 0u) return 0;
    supervisor = (cpu->sr & SR_SUPERVISOR) != 0u;
    root = (supervisor ? cpu->srp : cpu->urp) & PAGE_MASK;
    if (track_ram_dependency(entry, vm, root) != 0) return -1;

    l1_addr = root + ((logical >> 22u) * 4u);
    if (read32_physical(vm, l1_addr, &l1_desc) != 0 ||
        (l1_desc & DESC_VALID) == 0u) return 0;
    l2_base = l1_desc & PAGE_MASK;
    if (track_ram_dependency(entry, vm, l2_base) != 0) return -1;
    return 0;
}

static int read8_guest(struct amivm_exec_cache_entry *entry,
                       struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                       uint32_t logical, uint8_t *value)
{
    uint32_t physical = 0u;
    bool supervisor;

    if (entry == NULL || cpu == NULL || vm == NULL || value == NULL) return -1;
    if (track_mmu_dependencies(entry, cpu, vm, logical) != 0) return -1;
    supervisor = (cpu->sr & SR_SUPERVISOR) != 0u;
    if (amivm_mmu_translate(cpu, vm, logical, false, supervisor, &physical) != AMIVM_MMU_OK)
        return -1;
    if (track_ram_dependency(entry, vm, physical) != 0) return -1;
    return amivm_read8(vm, physical, value) ? 0 : -1;
}

static int read16_guest(struct amivm_exec_cache_entry *entry,
                        struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, uint16_t *value)
{
    uint8_t hi, lo;
    if ((logical & 1u) != 0u || value == NULL) return -1;
    if (read8_guest(entry, cpu, vm, logical, &hi) != 0 ||
        read8_guest(entry, cpu, vm, logical + 1u, &lo) != 0) return -1;
    *value = (uint16_t)(((uint16_t)hi << 8u) | lo);
    return 0;
}

static uint32_t current_root(const struct amivm_cpu_state *cpu)
{
    bool supervisor = (cpu->sr & SR_SUPERVISOR) != 0u;
    if ((cpu->tc & TC_ENABLE) == 0u) return 0u;
    return (supervisor ? cpu->srp : cpu->urp) & PAGE_MASK;
}

static int context_matches(const struct amivm_exec_cache_entry *entry,
                           const struct amivm_cpu_state *cpu)
{
    uint8_t supervisor = (cpu->sr & SR_SUPERVISOR) != 0u ? 1u : 0u;
    return entry->mmu_tc == cpu->tc && entry->mmu_root == current_root(cpu) &&
           entry->supervisor == supervisor;
}

static int dependencies_fresh(const struct amivm_exec_cache_entry *entry,
                              const struct amivm_vm *vm)
{
    size_t i;
    uint64_t generation;

    for (i = 0u; i < entry->dep_count; ++i) {
        if (!amivm_ram_page_generation(vm, entry->deps[i].page_base, &generation) ||
            generation != entry->deps[i].generation) return 0;
    }
    return 1;
}

static int compile_ir_block(struct amivm_exec_cache_entry *entry,
                            struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t words[AMIVM_EXEC_DECODE_WORDS];
    size_t count = 0u;
    uint32_t pc = cpu->pc;
    int rc;

    entry->mmu_tc = cpu->tc;
    entry->mmu_root = current_root(cpu);
    entry->supervisor = (cpu->sr & SR_SUPERVISOR) != 0u ? 1u : 0u;

    while (count < AMIVM_EXEC_DECODE_WORDS) {
        if (read16_guest(entry, cpu, vm, pc + (uint32_t)(count * 2u), &words[count]) != 0) break;
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
    int base_match;
    int context_match;
    int deps_fresh;

    if (engine == NULL || cpu == NULL || vm == NULL ||
        engine->backend == NULL || engine->backend->step == NULL) return -1;

    pc = cpu->pc;
    index = cache_index(pc);
    entry = &engine->cache[index];
    base_match = entry->valid && entry->generation == engine->generation && entry->pc == pc;
    context_match = base_match && context_matches(entry, cpu);
    deps_fresh = context_match && dependencies_fresh(entry, vm);

    if (deps_fresh) {
        engine->stats.cache_hits++;
    } else {
        if (base_match && !context_match) engine->stats.context_misses++;
        else if (context_match && !deps_fresh) engine->stats.stale_page_misses++;
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
