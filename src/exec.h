#ifndef AMIVM_EXEC_H
#define AMIVM_EXEC_H

#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "ir.h"

#define AMIVM_EXEC_CACHE_ENTRIES 256u
#define AMIVM_EXEC_DECODE_WORDS 16u

struct amivm_exec_cache_entry {
    uint32_t pc;
    uint32_t generation;
    uint64_t memory_write_generation;
    int valid;
    int ir_valid;
    struct amivm_ir_block block;
};

struct amivm_exec_stats {
    uint64_t instructions;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t stale_write_misses;
    uint64_t ir_blocks;
    uint64_t ir_instructions;
    uint64_t fallbacks;
    uint64_t exits;
};

struct amivm_exec_engine {
    const struct amivm_cpu_backend *backend;
    struct amivm_exec_cache_entry cache[AMIVM_EXEC_CACHE_ENTRIES];
    struct amivm_exec_stats stats;
    uint32_t generation;
};

void amivm_exec_init(struct amivm_exec_engine *engine,
                     const struct amivm_cpu_backend *backend);
void amivm_exec_reset(struct amivm_exec_engine *engine);
void amivm_exec_invalidate_all(struct amivm_exec_engine *engine);
int amivm_exec_step(struct amivm_exec_engine *engine,
                    struct amivm_cpu_state *cpu, struct amivm_vm *vm);
int amivm_exec_run(struct amivm_exec_engine *engine,
                   struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint64_t instruction_budget);

#endif
