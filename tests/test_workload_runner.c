#include "workload_runner.h"

#include <string.h>

int main(void)
{
    static const char valid[] =
        "{\"id\":\"x\",\"platform\":\"amigaos\","
        "\"kind\":\"compiler\",\"guest\":{"
        "\"artifact\":\"a\",\"command\":\"c\","
        "\"measurement\":{},\"correctness\":{}}";
    static const char invalid[] = "{\"id\":\"x\"}";

    if (amivm_workload_validate(valid, strlen(valid)) != 0)
        return 1;
    if (amivm_workload_validate(invalid, strlen(invalid)) == 0)
        return 2;
    return 0;
}
