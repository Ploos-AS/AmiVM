#include "ea.h"

#include <string.h>

static int consume_displacement(const uint16_t *words, size_t word_count,
                                size_t *cursor, uint8_t size, int32_t *value)
{
    if (size == 0u) {
        *value = 0;
        return AMIVM_EA_PARSE_OK;
    }
    if (size == 2u) {
        if (*cursor >= word_count) return AMIVM_EA_PARSE_TRUNCATED;
        *value = (int16_t)words[*cursor];
        (*cursor)++;
        return AMIVM_EA_PARSE_OK;
    }
    if (size == 4u) {
        uint32_t raw;
        if (*cursor + 1u >= word_count) return AMIVM_EA_PARSE_TRUNCATED;
        raw = ((uint32_t)words[*cursor] << 16u) | words[*cursor + 1u];
        *cursor += 2u;
        *value = (int32_t)raw;
        return AMIVM_EA_PARSE_OK;
    }
    return AMIVM_EA_PARSE_RESERVED;
}

int amivm_ea_parse_index_extension(const uint16_t *words, size_t word_count,
                                   struct amivm_ea_index_extension *result)
{
    uint16_t ext;
    uint8_t bd_code;
    uint8_t iis;
    size_t cursor = 1u;
    int rc;

    if (words == NULL || result == NULL || word_count == 0u)
        return AMIVM_EA_PARSE_TRUNCATED;

    memset(result, 0, sizeof(*result));
    ext = words[0];
    result->index_is_addr = (uint8_t)((ext >> 15u) & 1u);
    result->index_reg = (uint8_t)((ext >> 12u) & 7u);
    result->index_long = (uint8_t)((ext >> 11u) & 1u);
    result->index_scale = (uint8_t)(1u << ((ext >> 9u) & 3u));

    if ((ext & 0x0100u) == 0u) {
        result->brief_displacement = (int8_t)(ext & 0xffu);
        result->words_consumed = 1u;
        return AMIVM_EA_PARSE_OK;
    }

    result->full_format = 1u;
    result->base_suppress = (uint8_t)((ext >> 7u) & 1u);
    result->index_suppress = (uint8_t)((ext >> 6u) & 1u);

    /* Bit 3 is reserved in the 68020 full extension format. */
    if ((ext & 0x0008u) != 0u) return AMIVM_EA_PARSE_RESERVED;

    bd_code = (uint8_t)((ext >> 4u) & 3u);
    if (bd_code == 0u) return AMIVM_EA_PARSE_RESERVED;
    result->base_displacement_size = bd_code == 1u ? 0u : (bd_code == 2u ? 2u : 4u);

    iis = (uint8_t)(ext & 7u);
    if (iis == 4u) return AMIVM_EA_PARSE_RESERVED;
    if (iis >= 1u && iis <= 3u)
        result->indirect_mode = AMIVM_EA_INDIRECT_PREINDEXED;
    else if (iis >= 5u)
        result->indirect_mode = AMIVM_EA_INDIRECT_POSTINDEXED;

    if (iis == 2u || iis == 6u) result->outer_displacement_size = 2u;
    else if (iis == 3u || iis == 7u) result->outer_displacement_size = 4u;

    rc = consume_displacement(words, word_count, &cursor,
                              result->base_displacement_size,
                              &result->base_displacement);
    if (rc != AMIVM_EA_PARSE_OK) return rc;

    rc = consume_displacement(words, word_count, &cursor,
                              result->outer_displacement_size,
                              &result->outer_displacement);
    if (rc != AMIVM_EA_PARSE_OK) return rc;

    result->words_consumed = cursor;
    return AMIVM_EA_PARSE_OK;
}
