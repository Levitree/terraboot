// Application validation
//
// Copyright (C) 2026
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stddef.h> // offsetof
#include <string.h> // memcmp
#include "appvalidate.h"
#include "autoconf.h" // CONFIG_LAUNCH_APP_ADDRESS, CONFIG_METADATA_PAGE_SIZE
#include "fasthash.h" // fasthash64, fasthash32

// Maximum app size (total flash minus bootloader and metadata page)
#define MAX_APP_SIZE (CONFIG_FLASH_SIZE - \
    (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START) - CONFIG_METADATA_PAGE_SIZE)

// Get metadata address (at CONFIG_LAUNCH_APP_ADDRESS)
static uint32_t
get_metadata_addr(void)
{
    return CONFIG_LAUNCH_APP_ADDRESS;
}

// Get app code address (after metadata page)
static uint32_t
get_app_code_addr(void)
{
    return CONFIG_LAUNCH_APP_ADDRESS + CONFIG_METADATA_PAGE_SIZE;
}

app_metadata_t *
appvalidate_get_metadata(void)
{
    return (app_metadata_t *)get_metadata_addr();
}

app_verify_result_t
appvalidate_verify(void)
{
    app_metadata_t *meta = appvalidate_get_metadata();

    // Check magic string - distinguish erased from valid metadata
    if (memcmp(meta->magic, APP_METADATA_MAGIC, APP_METADATA_MAGIC_LEN) != 0)
        return APP_NO_METADATA;  // Magic doesn't match - no valid metadata

    // Check metadata integrity (CRC of fields before meta_crc)
    uint32_t expected_crc = fasthash32(meta,
        offsetof(app_metadata_t, meta_crc), 0);
    if (meta->meta_crc != expected_crc)
        return APP_META_CORRUPT;

    // Check size is valid
    if (meta->app_size == 0 || meta->app_size > MAX_APP_SIZE)
        return APP_SIZE_INVALID;

    // Compute hash of app region (code starts after metadata page)
    uint64_t computed = fasthash64((void*)get_app_code_addr(),
        meta->app_size, 0);
    if (computed != meta->app_hash)
        return APP_HASH_MISMATCH;

    return APP_VALID;
}
