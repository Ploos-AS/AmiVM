#include "powerload_runner.h"

#include <string.h>

int main(void)
{
    const struct amivm_powerload_workload *w =
        amivm_powerload_find("render.raytrace");

    if (!w || !w->run)
        return 1;
    if (strcmp(w->category, "rendering") != 0)
        return 1;
    return 0;
}
