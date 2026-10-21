#include "vm.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { THREADS = 8, INSTANCES_PER_THREAD = 32 };
struct worker_result {
    uint64_t ids[INSTANCES_PER_THREAD];
    int failed;
};

static void *worker(void *arg)
{
    struct worker_result *result = arg;
    struct amivm_config config;
    int i;
    amivm_config_init(&config);
    config.ram_size = 64u * 1024u;
    for (i = 0; i < INSTANCES_PER_THREAD; ++i) {
        struct amivm_vm *vm = malloc(sizeof(*vm));
        if (vm == NULL) { result->failed = 1; return NULL; }
        if (amivm_vm_init(vm, &config) != 0) {
            free(vm);
            result->failed = 1;
            return NULL;
        }
        result->ids[i] = vm->instance_id;
        amivm_vm_destroy(vm);
        free(vm);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[THREADS];
    struct worker_result results[THREADS] = {0};
    int i, j, k, l;
    for (i = 0; i < THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, worker, &results[i]) != 0) {
            for (j = 0; j < i; ++j) (void)pthread_join(threads[j], NULL);
            return 1;
        }
    }
    for (i = 0; i < THREADS; ++i)
        if (pthread_join(threads[i], NULL) != 0) return 1;
    for (i = 0; i < THREADS; ++i) {
        if (results[i].failed) return 1;
        for (j = 0; j < INSTANCES_PER_THREAD; ++j) {
            if (results[i].ids[j] == 0u) return 1;
            for (k = 0; k <= i; ++k) {
                int limit = k == i ? j : INSTANCES_PER_THREAD;
                for (l = 0; l < limit; ++l)
                    if (results[i].ids[j] == results[k].ids[l]) return 1;
            }
        }
    }
    puts("M336 concurrent VM instance IDs: PASS");
    return 0;
}
