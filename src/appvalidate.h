// Application validation
//
// Copyright (C) 2026
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __APPVALIDATE_H
#define __APPVALIDATE_H

#include <stdint.h>

#define APP_METADATA_MAGIC "TERRABOOT_RaiseTheWorld"
#define APP_METADATA_MAGIC_LEN 24

typedef struct __attribute__((packed)) {
    char magic[24];       // APP_METADATA_MAGIC
    uint32_t app_size;    // Size of app in bytes
    uint64_t app_hash;    // fasthash64 of app data
    uint32_t meta_crc;    // fasthash32 of above fields
    uint8_t reserved[8];  // Future use
} app_metadata_t;

typedef enum {
    APP_VALID = 0,
    APP_NO_METADATA,      // Magic doesn't match (erased or never programmed)
    APP_SIZE_INVALID,     // Size is 0 or too large
    APP_HASH_MISMATCH,    // Computed hash != stored hash
    APP_META_CORRUPT,     // Magic OK but CRC fails
} app_verify_result_t;

// Get pointer to metadata (at CONFIG_LAUNCH_APP_ADDRESS)
app_metadata_t *appvalidate_get_metadata(void);

// Verify app integrity - returns APP_VALID on success
app_verify_result_t appvalidate_verify(void);

#endif // appvalidate.h
