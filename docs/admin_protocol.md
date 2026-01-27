# Terraboot Admin Protocol

This document describes the CAN-based administrative protocol used by Terraboot for device discovery, ID assignment, heartbeat monitoring, and firmware flashing.

## CAN ID Allocation

| CAN ID Range  | Purpose         | Direction     | Format                               |
| ------------- | --------------- | ------------- | ------------------------------------ |
| `0x7DF`       | Admin broadcast | Host → Device | Discovery, reset, bootloader entry   |
| `0x3F0`       | Admin command   | Host → Device | Node ID management                   |
| `0x3F1`       | Admin response  | Device → Host | Responses to admin commands          |
| `0x700-0x77F` | Heartbeat       | Device → Host | `0x700 + short_id`                   |
| `0x100-0x1FF` | Bootloader data | Bidirectional | `0x100 + nodeid * 2` (TX), `+1` (RX) |

---

## Admin Commands (0x3F0)

All admin commands are sent on CAN ID `0x3F0`.

### Command Format

| Byte | Field      | Description                                    |
| ---- | ---------- | ---------------------------------------------- |
| 0    | Command ID | Command type identifier                        |
| 1-6  | UUID       | Device UUID (6 bytes, optional for broadcasts) |
| 7    | Parameter  | Command-specific parameter                     |

### Available Commands

#### 0x00: QUERY_UNASSIGNED

**Purpose:** Discover all devices waiting for ID assignment.

**Request:**

- DLC: 1
- Byte 0: `0x00`

**Response:** See `0x20: RESP_NEED_NODEID`

---

#### 0x11: SET_CANBOOT_NODEID

**Purpose:** Assign a short ID to a specific device.

**Request:**

- DLC: 8
- Byte 0: `0x11`
- Bytes 1-6: Target device UUID
- Byte 7: Short ID to assign (0-127)

**Response:** See `0x21: RESP_SET_NODEID_ACK`

**Notes:**

- Device will use CAN IDs: `0x100 + (short_id * 2)` for TX, `+1` for RX
- Device will send heartbeat on `0x700 + short_id`

---

#### 0x12: CLEAR_CANBOOT_NODEID

**Purpose:** Clear the assigned short ID (return device to unassigned state).

**Request:**

- DLC: 1
- Byte 0: `0x12`

**Response:** None

**Notes:**

- Broadcast command (affects all devices)
- Devices will stop sending heartbeats
- Devices will respond to QUERY_UNASSIGNED

---

## Admin Responses (0x3F1)

All admin responses are sent on CAN ID `0x3F1`.

### 0x20: RESP_NEED_NODEID

**Purpose:** Device announces it needs a short ID assignment.

**Response:**

- DLC: 8
- Byte 0: `0x20`
- Bytes 1-6: Device UUID
- Byte 7: Device type identifier

**Device Types:**

- `254` (0xFE): Terraboot

**Trigger:** Sent in response to QUERY_UNASSIGNED command

---

### 0x21: RESP_SET_NODEID_ACK

**Purpose:** Acknowledge successful short ID assignment.

**Response:**

- DLC: 8
- Byte 0: `0x21`
- Byte 1: Assigned short ID (echoed back)
- Byte 2: Device type (254 for Terraboot)
- Byte 3: Status (`0x01` = success)
- Bytes 4-7: Reserved (0x00)

**Trigger:** Sent immediately after receiving SET_CANBOOT_NODEID with matching UUID

**Notes:**

- Confirms device successfully configured with new ID
- Device immediately begins using new CAN IDs for data communication
- Device begins sending heartbeat messages

---

## Heartbeat Protocol

### Heartbeat Messages

**CAN ID:** `0x700 + short_id`

**Format:**

- DLC: 1
- Byte 0: `0x03` (heartbeat payload)

**Timing:** Sent every 2.5 seconds

**Purpose:**

- Indicates device is alive and responsive
- Allows host to detect disconnected/failed devices
- Only sent when device has assigned short ID

**Example:**

- Device with short_id=1 sends heartbeat on `0x701`
- Device with short_id=5 sends heartbeat on `0x705`

---

## Bootloader Flash Protocol

### Connect Command (0x11)

**Purpose:** Establish connection and query bootloader capabilities.

**Request:**

- Command: `CMD_CONNECT` (0x11)
- DLC: Variable

**Response:**

- Word 0-1: Header
- Word 2: Protocol version (uint32_t)
- Word 3: App launch address (uint32_t)
- Word 4: Block size in bytes (uint32_t)
- Word 5: Writable app flash size in bytes (uint32_t)
- Word 6+: MCU name (null-terminated string)
- Word X+: Katapult version (null-terminated string)
- Word Y+: App variant name (null-terminated string, if available)

**Notes:**

- Writable app flash size = Total flash - Bootloader size
- Calculated as: `CONFIG_FLASH_SIZE - (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START)`
- Example: 64KB flash - 8KB bootloader = 56KB writable area
- This area includes metadata page (1KB) + application code space
- App variant name populated from application metadata if valid
- Added in protocol v1.1

---

### Other Flash Commands

Standard Katapult flash protocol commands:

- `0x12`: SEND_BLOCK - Write flash block
- `0x13`: SEND_EOF - End of file transfer
- `0x14`: REQUEST_BLOCK - Read flash block
- `0x15`: COMPLETE - Finalize and reboot
- `0x16`: GET_CANBUS_ID - Get device UUID

See Katapult documentation for detailed protocol specification.

---

## Application Metadata

Applications flashed to Terraboot must include metadata at `CONFIG_LAUNCH_APP_ADDRESS`.

### Metadata Structure (84 bytes)

```c
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

### Fields

#### magic (24 bytes)

- Fixed string: `"TERRABOOT_RaiseTheWorld\0"`
- Used to detect valid metadata vs erased flash

#### app_size (4 bytes)

- Size of application code in bytes
- Does not include metadata page
- Used for hash verification

#### app_hash (8 bytes)

- fasthash64 of application code region
- Computed over `app_size` bytes starting after metadata page
- Used to verify application integrity

#### app_version (4 bytes)

- Application version in format: `0x00XXYYZZ`
  - `XX`: Major version
  - `YY`: Minor version
  - `ZZ`: Patch version
- Example: `0x00010203` = v1.2.3

#### app_variant_name (32 bytes)

- Null-terminated string identifying app variant
- Examples: "MainController", "SensorNode", "TestFirmware"
- Returned in CONNECT response
- Unused bytes should be 0x00

#### reserved (8 bytes)

- Reserved for future use
- Should be set to `0xFF`

#### meta_crc (4 bytes)

- fasthash32 of all fields before this one
- Used to detect metadata corruption
- Computed over 72 bytes (all fields except meta_crc)

### Validation Process

1. Check magic string matches `"TERRABOOT_RaiseTheWorld\0"`
2. Verify `meta_crc` against fasthash32 of previous fields
3. Verify `app_size` is valid (> 0, < max flash)
4. Compute fasthash64 of application code
5. Compare computed hash with `app_hash`

### Validation Results

```c
typedef enum {
    APP_VALID = 0,          // App is valid and verified
    APP_NO_METADATA,        // Magic doesn't match (erased/never programmed)
    APP_SIZE_INVALID,       // Size is 0 or too large
    APP_HASH_MISMATCH,      // Computed hash != stored hash
    APP_META_CORRUPT,       // Magic OK but CRC fails
} app_verify_result_t;
```

---

## UUID Generation

Device UUID is a 6-byte identifier generated using fasthash64:

```c
uint64_t hash = fasthash64(chip_unique_id, chip_id_length, 0xA16231A7);
// Take lower 6 bytes
memcpy(uuid, &hash, 6);
```

**Properties:**

- Unique per device (derived from chip ID)
- Deterministic (same chip ID → same UUID)
- Used for device identification in admin protocol

---

## Protocol Flow Examples

### Device Discovery and ID Assignment

```
1. Host: QUERY_UNASSIGNED (0x3F0)
   [0x00]

2. Device: RESP_NEED_NODEID (0x3F1)
   [0x20, UUID[6], 0xFE]

3. Host: SET_CANBOOT_NODEID (0x3F0)
   [0x11, UUID[6], short_id]

4. Device: RESP_SET_NODEID_ACK (0x3F1)
   [0x21, short_id, 0xFE, 0x01, 0x00, 0x00, 0x00, 0x00]

5. Device: Heartbeat every 2.5s (0x700 + short_id)
   [0x04]
```

### Firmware Flashing

```
1. Host: Jump to bootloader (Klipper admin protocol)

2. Host: CONNECT (CMD_CONNECT)

3. Device: Response with capabilities
   [proto_ver, app_addr, block_size, flash_size, mcu, version, variant]

4. Host: Send firmware blocks (SEND_BLOCK)

5. Device: ACK each block

6. Host: SEND_EOF

7. Host: COMPLETE

8. Device: Reboot to new application
```

### Heartbeat Monitoring

```
Device (short_id=1):
  t=0.0s: Heartbeat on 0x701 [0x04]
  t=2.5s: Heartbeat on 0x701 [0x04]
  t=5.0s: Heartbeat on 0x701 [0x04]
  ...

Host monitors:
  - If no heartbeat for > 7.5s (3x interval), consider device offline
  - Track last heartbeat timestamp per device
```

---

## Error Handling

### ID Conflict Detection

If a device receives a SET_CANBOOT_NODEID for a different UUID but same short ID it's already using:

- Device clears its assigned ID
- Device stops sending heartbeats
- Device returns to unassigned state
- Device will respond to next QUERY_UNASSIGNED

### Metadata Validation Failures

On boot, if metadata validation fails:

- Device stays in bootloader
- Does not attempt to jump to application
- Waits for firmware flash via admin protocol

### CAN Bus Errors

- Heartbeat uses best-effort send (no retry)
- Admin ACK uses retry loop until successful
- Flash protocol includes block-level retry mechanism

---

## Version History

### Protocol v1.1 (Current)

- Added flash size to CONNECT response
- Added app variant name to CONNECT response
- Added app_variant_name to metadata structure (84 bytes)
- Added app_version to metadata structure
- Moved meta_crc to end of structure

### Protocol v1.0

- Initial release
- Basic admin protocol (query, assign, clear)
- Heartbeat support
- Original metadata structure (48 bytes)

---

## Implementation Notes

### Host Implementation

1. **Discovery:**
   - Send QUERY_UNASSIGNED on 0x3F0
   - Collect RESP_NEED_NODEID responses on 0x3F1
   - Wait 2 seconds for all responses

2. **ID Assignment:**
   - Send SET_CANBOOT_NODEID with target UUID
   - Wait for RESP_SET_NODEID_ACK
   - Verify ack.short_id matches request

3. **Heartbeat Monitoring:**
   - Track last heartbeat timestamp per device
   - Alert if no heartbeat for > 7.5 seconds
   - Use heartbeat as "alive" indicator

### Device Implementation

1. **Boot Sequence:**
   - Validate application metadata
   - If valid, jump to application
   - If invalid, stay in bootloader

2. **ID Assignment:**
   - Load UUID from chip ID
   - Wait for SET_CANBOOT_NODEID
   - Send ACK on successful assignment
   - Begin heartbeat timer

3. **Heartbeat:**
   - Use timer-based periodic task (2.5s)
   - Only send if short ID is assigned
   - Best-effort send (no retry)

---

## Security Considerations

1. **UUID Collision:** Extremely unlikely (64-bit hash of unique chip ID)
2. **ID Spoofing:** No authentication - trusted CAN bus required
3. **Flash Protection:** No encryption - physical access security model
4. **Metadata Integrity:** CRC and hash verification prevent accidental corruption

---

## References

- Katapult Documentation: https://github.com/Arksine/katapult
- fasthash Algorithm: https://github.com/ztanml/fast-hash
- CAN 2.0 Specification
