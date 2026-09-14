#ifndef AMIVM_EA_H
#define AMIVM_EA_H

#include <stddef.h>
#include <stdint.h>

enum amivm_ea_indirect_mode {
    AMIVM_EA_INDIRECT_NONE = 0,
    AMIVM_EA_INDIRECT_PREINDEXED,
    AMIVM_EA_INDIRECT_POSTINDEXED,
};

enum amivm_ea_parse_result {
    AMIVM_EA_PARSE_OK = 0,
    AMIVM_EA_PARSE_TRUNCATED = -1,
    AMIVM_EA_PARSE_RESERVED = -2,
};

struct amivm_ea_index_extension {
    uint8_t full_format;
    uint8_t index_is_addr;
    uint8_t index_reg;
    uint8_t index_long;
    uint8_t index_scale;
    uint8_t base_suppress;
    uint8_t index_suppress;
    uint8_t base_displacement_size;
    uint8_t outer_displacement_size;
    enum amivm_ea_indirect_mode indirect_mode;
    int32_t brief_displacement;
    int32_t base_displacement;
    int32_t outer_displacement;
    size_t words_consumed;
};

int amivm_ea_parse_index_extension(const uint16_t *words, size_t word_count,
                                   struct amivm_ea_index_extension *result);

#endif
