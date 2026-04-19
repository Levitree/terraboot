# Changelog

All notable changes to Terraboot will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added

#### Admin Protocol Enhancements

- **ACK Response on ID Assignment** ([canserial.c:187-202](src/generic/canserial.c))
  - Device now sends acknowledgment on CAN ID 0x3F1 when short ID is assigned
  - ACK message format: `[0x21, short_id, device_type, status, 0x00, 0x00, 0x00, 0x00]`
  - Provides confirmation to host that ID assignment was successful
  - New constant: `CANBUS_RESP_SET_NODEID_ACK (0x21)`

- **Periodic Heartbeat Messages** ([canserial.c:315-341](src/generic/canserial.c))
  - Device sends heartbeat every 2.5 seconds on CAN ID `0x700 + short_id`
  - Heartbeat format: DLC=1, payload=`0x04`
  - Allows host to detect device presence and health
  - Uses timer-based scheduling with `DECL_TASK` infrastructure
  - Only active when device has assigned short ID

- **Device Type Constant** ([canserial.c:120](src/generic/canserial.c))
  - Added `TERRABOOT_DEVICE_TYPE (254)` constant
  - Replaces hardcoded device type value
  - Used in QUERY_UNASSIGNED and ACK responses

#### Metadata Structure Updates

- **Expanded Metadata to 84 bytes** ([appvalidate.h:15-22](src/appvalidate.h))
  - Added `app_version` field (4 bytes) - format: `0x00XXYYZZ`
  - Added `app_variant_name` field (32 bytes) - null-terminated string
  - Moved `meta_crc` to end of structure (was after app_hash)
  - Reserved field now 8 bytes, should be set to `0xFF`
  - Total structure size: 84 bytes (was 48 bytes)

```c
// New metadata structure
typedef struct __attribute__((packed)) {
    char magic[24];             // "TERRABOOT_RaiseTheWorld\0"
    uint32_t app_size;          // Size of app in bytes
    uint64_t app_hash;          // fasthash64 of app data
    uint32_t app_version;       // Application version (0x00XXYYZZ format)
    char app_variant_name[32];  // Variant name (null-terminated)
    uint8_t reserved[8];        // Future use, set to 0xFF
    uint32_t meta_crc;          // fasthash32 of all above fields
} app_metadata_t;
```

#### Connect Response Enhancements

- **Writable App Flash Size in CONNECT Response** ([flashcmd.c:34-45](src/flashcmd.c))
  - Added writable application flash size to connect response (word 5)
  - Calculated as: `CONFIG_FLASH_SIZE - (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START)`
  - Returns actual space available for application (excluding bootloader)
  - Example: 64KB total flash - 8KB bootloader = 56KB writable area
  - This area includes metadata page (1KB) + application code space (55KB)
  - Allows host to know available application space for validation and planning

- **App Variant Name in CONNECT Response** ([flashcmd.c:21-48](src/flashcmd.c))
  - Reads app variant name from metadata if valid
  - Includes variant name in connect response
  - Allows host to identify which firmware variant is installed
  - Example variants: "MainController", "SensorNode", "TestFirmware"

```
Connect Response Format (updated):
  Word 0-1: Header
  Word 2: Protocol version
  Word 3: App launch address
  Word 4: Block size
  Word 5: Flash size (NEW)
  Word 6+: MCU name (null-terminated)
  Word X+: Katapult version (null-terminated)
  Word Y+: App variant name (null-terminated, if available) (NEW)
```

#### Documentation

- **Admin Protocol Documentation** ([docs/admin_protocol.md](docs/admin_protocol.md))
  - Comprehensive protocol specification
  - CAN ID allocation table
  - Admin command reference (QUERY, SET, CLEAR, ACK)
  - Heartbeat protocol specification
  - Metadata structure documentation
  - Protocol flow examples
  - Error handling guidelines
  - Implementation notes for host and device

- **Changelog** ([CHANGELOG.md](CHANGELOG.md))
  - Tracking all changes to the project
  - Links to specific code changes

### Changed

- **Refactored Device Type Handling** ([canserial.c:146](src/generic/canserial.c))
  - Replaced hardcoded `254` with `TERRABOOT_DEVICE_TYPE` constant
  - Improves code maintainability

- **Enhanced Timer Includes** ([canserial.c:18](src/generic/canserial.c))
  - Added `generic/misc.h` include for timer functions
  - Enables heartbeat timing functionality

- **Extended CONNECT Command** ([flashcmd.c:18-48](src/flashcmd.c))
  - Increased response size to accommodate flash size and variant name
  - Added metadata reading for variant name extraction
  - Response now variable-length based on variant name

- **Reworked Per-Device CAN ID Layout** ([canserial.c:151-157](src/generic/canserial.c))
  - Replaced old `(short_id << 1) + 0x100` scheme with 32-ID per-device block
  - New formula: `base_id = (short_id << 5) + 0x020`
    - `base + 0x00` CMD_RX (host → device)
    - `base + 0x01` CMD_TX responses (device → host)
    - `base + 0x02 .. base + 0x1F` DATA channels (device → host, 30 channels)
  - Valid `short_id` range narrowed to 1..29 (was 0..127)
  - Out-of-range short IDs are silently rejected (no ACK sent)
  - Reserves 0x3C0..0x3EF for future allocations
  - Admin IDs (0x3F0 / 0x3F1) and heartbeat IDs (0x700 + short_id) unchanged
  - **Breaking Change:** Host tooling must use new ID formula and enforce 1..29 range

### Technical Details

#### Memory Layout & Address Constants

Understanding the STM32 flash memory layout and address configuration:

**Address Constants (from `.config`):**

| Constant                           | Value       | Description                                   |
| ---------------------------------- | ----------- | --------------------------------------------- |
| `CONFIG_FLASH_START`               | `0x8000000` | Physical flash start address (STM32 standard) |
| `CONFIG_FLASH_BOOT_ADDRESS`        | `0x8000000` | Where bootloader code begins                  |
| `CONFIG_LAUNCH_APP_ADDRESS`        | `0x8002000` | Where app metadata + code starts (8KB offset) |
| `CONFIG_METADATA_PAGE_SIZE`        | `0x400`     | Metadata page size (1KB)                      |
| `CONFIG_FLASH_SIZE`                | `0x10000`   | Total flash size (64KB)                       |
| `CONFIG_FLASH_APPLICATION_ADDRESS` | `0x8000000` | Legacy constant (unused)                      |

**Bootloader Size Configuration:**

The bootloader size is configured via Kconfig menu options:

- `STM32_APP_START_1000`: 4KB bootloader → `LAUNCH_APP_ADDRESS = 0x8001000`
- `STM32_APP_START_2000`: 8KB bootloader → `LAUNCH_APP_ADDRESS = 0x8002000` ← **Recommended**
- `STM32_APP_START_4000`: 16KB bootloader → `LAUNCH_APP_ADDRESS = 0x8004000`
- `STM32_APP_START_8000`: 32KB bootloader → `LAUNCH_APP_ADDRESS = 0x8008000`

**Memory Map Example (64KB flash, 8KB bootloader):**

```
0x8000000  ┌─────────────────────────┐
           │   Terraboot Bootloader  │  8KB (0x2000)
0x8002000  ├─────────────────────────┤  ← LAUNCH_APP_ADDRESS
           │   App Metadata          │  1KB (0x400) - metadata structure
0x8002400  ├─────────────────────────┤
           │                         │
           │   Application Code      │  55KB (0xDC00)
           │   (Vector table at      │
           │    0x8002400)            │
           │                         │
0x8010000  └─────────────────────────┘  ← End of flash (FLASH_START + FLASH_SIZE)
```

**Writable Application Area Calculation:**

```c
Writable Area = CONFIG_FLASH_SIZE - (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START)
              = 0x10000 - (0x8002000 - 0x8000000)
              = 0x10000 - 0x2000
              = 0xE000 (56KB)
```

This 56KB is returned in the CONNECT response and represents the total space available for:

- Application metadata (1KB at 0x8002000)
- Application vector table and code (55KB starting at 0x8002400)

**Important Notes:**

- Application code vector table must be at `LAUNCH_APP_ADDRESS + METADATA_PAGE_SIZE`
- Bootloader jumps to this address after metadata validation
- Metadata CRC covers the entire metadata structure (84 bytes)
- Application hash covers only the application code region

#### CAN ID Allocation (Updated)

| CAN ID Range  | Purpose                                                                                 |
| ------------- | --------------------------------------------------------------------------------------- |
| `0x040-0x3BF` | Per-device 32-ID block: `base = (short_id << 5) + 0x020`, short_id 1..29                |
|               | &nbsp;&nbsp;`base + 0x00` CMD_RX (host → device)                                        |
|               | &nbsp;&nbsp;`base + 0x01` CMD_TX responses (device → host)                              |
|               | &nbsp;&nbsp;`base + 0x02 .. base + 0x1F` DATA channels (device → host)                  |
| `0x3C0-0x3EF` | Reserved                                                                                |
| `0x3F0`       | Admin command (query, assign, clear)                                                    |
| `0x3F1`       | Admin response (need ID, ACK)                                                           |
| `0x700-0x77F` | Heartbeat messages (`0x700 + short_id`)                                                 |
| `0x7DF`       | Admin broadcast (discovery, reset, bootloader entry)                                    |

#### Binary Size Impact

- Heartbeat and ACK features add approximately 60-80 bytes to firmware
- Metadata structure change affects application binary layout
- Applications must be rebuilt with new metadata structure

#### Compatibility Notes

- **Breaking Change:** Metadata structure changed from 48 to 84 bytes
- Applications compiled with old metadata format will fail validation
- All applications must be rebuilt with new `app_metadata_t` structure
- Flash tools must be updated to handle new CONNECT response format
- Heartbeat monitoring is backward compatible (new feature, no breaking changes)

### Migration Guide

#### For Application Developers

1. Update application to new 84-byte metadata structure
2. Add `app_version` field (format: `0x00XXYYZZ`)
3. Add `app_variant_name` field (up to 31 characters + null terminator)
4. Set `reserved` field to `0xFF` (8 bytes)
5. Move `meta_crc` calculation to end (after all other fields)
6. Rebuild application with new structure

#### For Host Software

1. Update CONNECT response parser to read:
   - Word 5: Flash size
   - Additional words: App variant name (after MCU and version)
2. Implement ACK response handling for ID assignment (0x21)
3. Implement heartbeat monitoring (optional but recommended)
4. Update metadata validation to expect 84-byte structure

#### For Firmware Flashing

1. Ensure bootloader is updated to latest version
2. Flash applications built with new metadata structure
3. Verify CONNECT response includes flash size and variant name
4. Test heartbeat messages appear every 2.5 seconds after ID assignment

### Files Modified

| File                                               | Changes                                   | Lines                                         |
| -------------------------------------------------- | ----------------------------------------- | --------------------------------------------- |
| [src/generic/canserial.c](src/generic/canserial.c) | Admin protocol, heartbeat, constants      | 18, 42, 48-54, 115-120, 146, 177-208, 315-341 |
| [src/generic/canserial.h](src/generic/canserial.h) | (No changes)                              | -                                             |
| [src/appvalidate.h](src/appvalidate.h)             | Metadata structure (48→84 bytes)          | 15-22                                         |
| [src/appvalidate.c](src/appvalidate.c)             | (No changes - validation logic unchanged) | -                                             |
| [src/flashcmd.c](src/flashcmd.c)                   | CONNECT response enhancements             | 10-11, 18-48                                  |
| [docs/admin_protocol.md](docs/admin_protocol.md)   | New comprehensive protocol documentation  | (new file)                                    |
| [CHANGELOG.md](CHANGELOG.md)                       | This file                                 | (new file)                                    |

### Testing Recommendations

1. **ID Assignment:**
   - Send QUERY_UNASSIGNED, verify device responds
   - Send SET_CANBOOT_NODEID, verify ACK response
   - Confirm device uses correct data CAN IDs

2. **Heartbeat:**
   - Monitor CAN bus for heartbeat messages
   - Verify 2.5 second interval
   - Verify correct CAN ID (`0x700 + short_id`)
   - Verify payload is `0x04`

3. **CONNECT Response:**
   - Send CONNECT command
   - Parse flash size from word 5
   - Parse app variant name from end of response
   - Verify all fields present and correct

4. **Metadata:**
   - Flash application with new metadata
   - Verify bootloader validates metadata successfully
   - Verify variant name appears in CONNECT response
   - Test metadata corruption detection

### Known Issues

- Binary size exceeds 4096 bytes on some configurations
  - Workaround: Increase `LAUNCH_APP_ADDRESS` in menuconfig
  - Or disable optional features to reduce size

### Future Work

- [ ] Add authentication to admin protocol
- [ ] Support for encrypted firmware updates
- [ ] Extended metadata for build timestamps
- [ ] Configurable heartbeat interval
- [ ] Network-wide heartbeat aggregation

---

## [1.2.6] - 2026-01-27

### Changed

- Speed optimizations for CAN bus communication
- Updated CANBUS speed configuration

## [Earlier Versions]

See git history for changes in versions prior to 1.2.6.
