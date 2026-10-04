#include "powerload_runner.h"

#include <string.h>

int main(void)
{
    const struct amivm_powerload_workload *workload;

    if (amivm_powerload_count() < 10u)
        return 1;

    workload = amivm_powerload_find("cpu.integer");
    if (!workload || !workload->run)
        return 1;

    if (strcmp(workload->category, "cpu") != 0)
        return 1;

    if (amivm_powerload_find("does.not.exist") != NULL)
        return 1;

    if (amivm_powerload_at(amivm_powerload_count()) != NULL)
        return 1;

    return 0;
}
