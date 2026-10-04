#include "powerload.h"

#include <stdio.h>

void amivm_powerload_result_init(struct amivm_powerload_result *result,
                                 const char *workload_id,
                                 const char *mode)
{
    result->schema_version = 1u;
    result->workload_id = workload_id;
    result->mode = mode;
    result->wall_clock_seconds = 0.0;
    result->throughput = 0.0;
    result->throughput_unit = "";
    result->vm_config = "";
    result->reproducible = false;
    result->notes = "";
}

int amivm_powerload_result_write_json(
    FILE *stream, const struct amivm_powerload_result *result)
{
    if (!stream || !result || !result->workload_id || !result->mode)
        return -1;

    if (fprintf(stream,
                "{\"schema_version\":%u,"
                "\"workload_id\":\"%s\","
                "\"mode\":\"%s\","
                "\"wall_clock_seconds\":%.6f,"
                "\"throughput\":%.6f,"
                "\"throughput_unit\":\"%s\","
                "\"vm_config\":\"%s\","
                "\"reproducible\":%s,"
                "\"notes\":\"%s\"}\n",
                result->schema_version,
                result->workload_id,
                result->mode,
                result->wall_clock_seconds,
                result->throughput,
                result->throughput_unit ? result->throughput_unit : "",
                result->vm_config ? result->vm_config : "",
                result->reproducible ? "true" : "false",
                result->notes ? result->notes : "") < 0)
        return -1;

    return 0;
}
