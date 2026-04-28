// Command handlers for flash requests
//
// Copyright (C) 2021 Eric Callahan <arksine.code@gmail.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h> // memmove
#include "autoconf.h" // CONFIG_BLOCK_SIZE, CONFIG_FLASH_SIZE
#include "board/flash.h" // flash_write_block
#include "board/misc.h" // application_jump
#include "byteorder.h" // cpu_to_le32
#include "canboot.h" // application_jump
#include "command.h" // command_respond_ack
#include "flashcmd.h" // flashcmd_is_in_transfer
#include "sched.h" // DECL_TASK
#include "appvalidate.h" // appvalidate_get_metadata
#include "generic/misc.h" // timer_read_time, timer_from_us

static uint8_t is_in_transfer;
static uint32_t total_bytes_written;

// Handler for "connect" commands
void
command_connect(uint32_t *data)
{
    // Clear any stale transfer state from a previous (possibly aborted)
    // session. Without this, the timeout-to-app path stays blocked forever
    // if a host crashed mid-flash and reconnects.
    is_in_transfer = 0;
    total_bytes_written = 0;

    app_metadata_t *meta = appvalidate_get_metadata();
    const char *variant_name = "";
    uint32_t variant_len = 0;
    uint32_t app_version = 0;

    // Get app version and variant name from metadata if valid
    if (memcmp(meta->magic, APP_METADATA_MAGIC, APP_METADATA_MAGIC_LEN) == 0) {
        app_version = meta->app_version;
        variant_name = meta->app_variant_name;
        variant_len = strnlen(variant_name, sizeof(meta->app_variant_name));
    }

    // Calculate writable application area (flash size minus bootloader)
    uint32_t app_flash_size = CONFIG_FLASH_SIZE -
        (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START);

    uint32_t mcuwords = DIV_ROUND_UP(strlen(CONFIG_MCU), 4);
    uint32_t version_words = DIV_ROUND_UP(strlen(CONFIG_KATAPULT_VERSION), 4);
    uint32_t variant_words = DIV_ROUND_UP(variant_len, 4);
    uint32_t out[10 + mcuwords + version_words + variant_words];
    memset(out, 0, (10 + mcuwords + version_words + variant_words) * 4);
    out[2] = cpu_to_le32(PROTO_VERSION);
    out[3] = cpu_to_le32(CONFIG_LAUNCH_APP_ADDRESS);
    out[4] = cpu_to_le32(CONFIG_BLOCK_SIZE);
    out[5] = cpu_to_le32(app_flash_size);
    out[6] = cpu_to_le32(app_version);
    memcpy(&out[7], CONFIG_MCU, strlen(CONFIG_MCU));
    memcpy(
        &out[8 + mcuwords], CONFIG_KATAPULT_VERSION,
        strlen(CONFIG_KATAPULT_VERSION)
    );
    if (variant_len > 0) {
        memcpy(&out[9 + mcuwords + version_words], variant_name, variant_len);
    }
    command_respond_ack(CMD_CONNECT, out, ARRAY_SIZE(out));
}


/****************************************************************
 * Command "complete" handling
 ****************************************************************/

static uint8_t complete;
static uint32_t complete_endtime;

void
command_complete(uint32_t *data)
{
    uint32_t out[3];
    command_respond_ack(CMD_COMPLETE, out, ARRAY_SIZE(out));
    complete = 1;
    complete_endtime = timer_read_time() + timer_from_us(100000);
}

void
complete_task(void)
{
    if (complete && timer_is_before(complete_endtime, timer_read_time()))
        system_reset();
}
DECL_TASK(complete_task);


/****************************************************************
 * Flash commands
 ****************************************************************/

int
flashcmd_is_in_transfer(void)
{
    return is_in_transfer;
}

// Reject any block_address that isn't fully inside the writable app region.
// Without this, a host can read or write past flash end (faulting on some
// backends) or below LAUNCH_APP_ADDRESS into bootloader memory.
static int
flashcmd_block_in_range(uint32_t block_address)
{
    uint32_t flash_end = CONFIG_FLASH_START + CONFIG_FLASH_SIZE;
    if (block_address < CONFIG_LAUNCH_APP_ADDRESS)
        return 0;
    if (block_address > flash_end - CONFIG_BLOCK_SIZE)
        return 0;
    return 1;
}

void
command_read_block(uint32_t *data)
{
    is_in_transfer = 1;
    uint32_t block_address = le32_to_cpu(data[1]);
    if (!flashcmd_block_in_range(block_address)) {
        command_respond_command_error();
        return;
    }
    uint32_t out[CONFIG_BLOCK_SIZE / 4 + 2 + 2];
    out[2] = cpu_to_le32(block_address);
    application_read_flash(block_address, &out[3]);
    command_respond_ack(CMD_REQ_BLOCK, out, ARRAY_SIZE(out));
}

void
command_write_block(uint32_t *data)
{
    is_in_transfer = 1;
    if (command_get_arg_count(data) != (CONFIG_BLOCK_SIZE / 4) + 1)
        goto fail;
    uint32_t block_address = le32_to_cpu(data[1]);
    if (!flashcmd_block_in_range(block_address))
        goto fail;
    // Reset byte counter at start of new transfer
    if (block_address == CONFIG_LAUNCH_APP_ADDRESS)
        total_bytes_written = 0;
    int ret = flash_write_block(block_address, &data[2]);
    if (ret < 0)
        goto fail;
    total_bytes_written += CONFIG_BLOCK_SIZE;
    uint32_t out[4];
    out[2] = cpu_to_le32(block_address);
    command_respond_ack(CMD_RX_BLOCK, out, ARRAY_SIZE(out));
    return;
fail:
    command_respond_command_error();
}

void
command_eof(uint32_t *data)
{
    is_in_transfer = 0;
    int ret = flash_complete();
    if (ret < 0) {
        command_respond_command_error();
        return;
    }
    // App metadata is now generated by the build system and included
    // in the firmware binary - no need to write it here
    uint32_t out[4];
    out[2] = cpu_to_le32(ret);
    command_respond_ack(CMD_RX_EOF, out, ARRAY_SIZE(out));
}
