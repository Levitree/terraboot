# Terraboot Memory Layout Guide

This document explains the STM32 flash memory layout, address constants, and how to configure bootloader size.

## Quick Reference

| Term | What It Is | Example Value |
|------|------------|---------------|
| **FLASH_START** | Where flash memory begins in STM32 address space | `0x8000000` |
| **LAUNCH_APP_ADDRESS** | Where application metadata+code is placed | `0x8002000` (8KB offset) |
| **Bootloader Size** | Space used by Terraboot bootloader | 8KB (configurable: 4KB, 8KB, 16KB, 32KB) |
| **Writable App Area** | Space available for your application | Total flash - Bootloader size |

---

## Memory Layout

### Example: STM32F103 with 64KB Flash, 8KB Bootloader

```
Physical Address    Size      Contents
─────────────────────────────────────────────────────────────
0x8000000          8KB       Terraboot Bootloader
                   (0x2000)  - Entry point
                             - Admin protocol handlers
                             - Flash write routines
                             - Metadata validation
                             - CAN/USB communication

0x8002000          1KB       Application Metadata ← LAUNCH_APP_ADDRESS
                   (0x400)   - Magic string (24 bytes)
                             - App size (4 bytes)
                             - App hash (8 bytes)
                             - App version (4 bytes)
                             - Variant name (32 bytes)
                             - Reserved (8 bytes)
                             - CRC (4 bytes)
                             Total: 84 bytes
                             (rest of 1KB page is padding)

0x8002400          55KB      Application Code
                   (0xDC00)  - Vector table (first 256 bytes)
                             - Your application code
                             - .text, .rodata, .data init

0x8010000                    End of flash ← FLASH_START + FLASH_SIZE
```

**Key Points:**
- Application vector table **must** be at `LAUNCH_APP_ADDRESS + 0x400` (0x8002400)
- Bootloader validates metadata before jumping to application
- Bootloader sets VTOR register to application vector table address
- Application code hash covers only the code region (not metadata)

---

## Address Constants Explained

### CONFIG_FLASH_START (`0x8000000`)
- **What:** Physical start address of flash memory on STM32
- **Why:** Standard STM32 flash memory-mapped address
- **Used By:** Linker scripts, address calculations
- **Never Changes:** Fixed by STM32 hardware architecture

### CONFIG_FLASH_BOOT_ADDRESS (`0x8000000`)
- **What:** Where bootloader code begins
- **Why:** Bootloader always starts at the beginning of flash
- **Same As:** `FLASH_START` (they are always identical)
- **Used By:** Reset vector, startup code

### CONFIG_LAUNCH_APP_ADDRESS (e.g., `0x8002000`)
- **What:** Where application metadata and code is placed
- **Why:** Reserves space for bootloader at start of flash
- **Configurable:** Via Kconfig menu (`STM32_APP_START_xxxx` options)
- **Used By:** Flash writing, metadata validation, application jump
- **Format:** `FLASH_START + Bootloader_Size`

**Options:**
```
STM32_APP_START_1000  →  0x8001000  (4KB bootloader)
STM32_APP_START_2000  →  0x8002000  (8KB bootloader) ← Recommended
STM32_APP_START_4000  →  0x8004000  (16KB bootloader)
STM32_APP_START_8000  →  0x8008000  (32KB bootloader)
```

### CONFIG_FLASH_APPLICATION_ADDRESS (`0x8000000`)
- **What:** Legacy Katapult constant
- **Why:** Historical compatibility
- **Status:** **Not used in modern code**
- **Ignore:** Use `LAUNCH_APP_ADDRESS` instead

### CONFIG_METADATA_PAGE_SIZE (`0x400` = 1KB)
- **What:** Size of metadata page
- **Why:** Aligns with STM32 flash page size
- **Contains:** 84-byte metadata structure + padding
- **Used By:** Application jump address calculation

### CONFIG_FLASH_SIZE (e.g., `0x10000` = 64KB)
- **What:** Total flash memory size
- **Why:** Defines available flash on your specific STM32 chip
- **Varies By:** STM32 part number (e.g., F103x6=32KB, F103xB=128KB, F103xE=512KB)
- **Used By:** Bounds checking, writable area calculation

---

## Choosing Bootloader Size

### How to Change

1. Run `make menuconfig`
2. Navigate to: "Application start offset"
3. Select one of:
   - `4KiB offset` (minimal, tight squeeze)
   - `8KiB offset` ← **Recommended**
   - `16KiB offset` (plenty of room for features)
   - `32KiB offset` (for large bootloaders)

### Recommendations

| Flash Size | Recommended Bootloader | Application Space | Notes |
|------------|------------------------|-------------------|-------|
| 32KB | 4KB (STM32_APP_START_1000) | 28KB | Tight - may need to disable features |
| 64KB | 8KB (STM32_APP_START_2000) | 56KB | **Good balance** |
| 128KB | 8KB (STM32_APP_START_2000) | 120KB | Plenty of app space |
| 512KB | 16KB (STM32_APP_START_4000) | 496KB | Room for advanced bootloader features |

### Size Considerations

**Bootloader Features by Size:**

**4KB (Minimal):**
- ✅ Basic admin protocol
- ✅ Flash writing
- ✅ Metadata validation
- ❌ Heartbeat (may not fit)
- ❌ Advanced debugging

**8KB (Recommended):**
- ✅ All basic features
- ✅ Heartbeat support
- ✅ ACK responses
- ✅ Full admin protocol
- ✅ Error handling
- ⚠️ Limited debug features

**16KB+ (Spacious):**
- ✅ All features
- ✅ Extensive error handling
- ✅ Debug logging
- ✅ Future expansion

---

## Writable Application Area

### What Is It?

The writable application area is the flash space available for your application after subtracting the bootloader.

### Calculation

```c
Writable Area = CONFIG_FLASH_SIZE - (CONFIG_LAUNCH_APP_ADDRESS - CONFIG_FLASH_START)
```

**Example with 64KB flash, 8KB bootloader:**
```
= 0x10000 - (0x8002000 - 0x8000000)
= 0x10000 - 0x2000
= 0xE000 (56KB decimal = 57,344 bytes)
```

### What's Included?

The writable area includes:
1. **Metadata page (1KB):** Your app's metadata structure
2. **Application code (remaining):** Vector table, code, data

**Example breakdown (64KB flash, 8KB bootloader):**
- Total writable: 56KB
- Metadata: 1KB (0x400)
- **Application code: 55KB (0xDC00)** ← This is what your app uses

### Returned in CONNECT Response

The CONNECT command (word 5) returns the **writable application area** (56KB in our example).

**Why not just application code space (55KB)?**
- Host needs to know the full writable area including metadata
- Allows host to validate firmware size before flashing
- Metadata is part of the application binary

---

## Common Configuration Issues

### Issue: Binary too large for LAUNCH_APP_ADDRESS

**Error:**
```
The Katapult binary is too large for the configured LAUNCH_APP_ADDRESS.
Maximum size 4096. Current size 6288.
```

**Solution:**
1. Run `make menuconfig`
2. Select larger bootloader offset:
   - Change from `4KiB offset` to `8KiB offset`
3. Rebuild: `make clean && make`

### Issue: Config shows STM32_APP_START_1000 but LAUNCH_APP_ADDRESS is 0x8002000

**Problem:**
```config
CONFIG_STM32_APP_START_1000=y          ← Says 4KB
CONFIG_LAUNCH_APP_ADDRESS=0x8002000    ← Actually 8KB
```

**This is inconsistent!** You should fix this:

**Solution:**
```config
# CONFIG_STM32_APP_START_1000 is not set
CONFIG_STM32_APP_START_2000=y           ← Set this
CONFIG_LAUNCH_APP_ADDRESS=0x8002000     ← Matches
```

Or run `make menuconfig` and select "8KiB offset" to automatically set both correctly.

### Issue: Application won't boot after flashing

**Check:**
1. Metadata magic string is correct: `"TERRABOOT_RaiseTheWorld\0"`
2. Metadata CRC is valid
3. Application hash matches
4. Application size is within writable area
5. Application vector table is at `LAUNCH_APP_ADDRESS + 0x400`

---

## Build System Details

### How LAUNCH_APP_ADDRESS is Set

The Kconfig system automatically sets `LAUNCH_APP_ADDRESS` based on your menu selection:

**File:** `src/stm32/Kconfig`
```kconfig
config LAUNCH_APP_ADDRESS
    hex
    default 0x8020000 if STM32_APP_START_20000
    default 0x8008000 if STM32_APP_START_8000
    default 0x8004000 if STM32_APP_START_4000
    default 0x8002000 if STM32_APP_START_2000
    default 0x8001000 if STM32_APP_START_1000
    default 0x8008000
```

### Generated autoconf.h

During build, these configs become C macros in `out/autoconf.h`:

```c
#define CONFIG_FLASH_START 0x8000000
#define CONFIG_LAUNCH_APP_ADDRESS 0x8002000
#define CONFIG_FLASH_SIZE 0x10000
#define CONFIG_METADATA_PAGE_SIZE 0x400
// ... etc
```

These are then used throughout the codebase.

---

## Application Integration

### Linker Script Setup

Your application linker script must:

1. **Place vector table at correct address:**
   ```ld
   FLASH_START = 0x8002000;  /* LAUNCH_APP_ADDRESS */
   METADATA_SIZE = 0x400;     /* 1KB */

   MEMORY {
       FLASH (rx) : ORIGIN = FLASH_START + METADATA_SIZE, LENGTH = 55K
   }
   ```

2. **Reserve metadata space:**
   ```ld
   .metadata FLASH_START : {
       KEEP(*(.metadata))
   }

   .text (FLASH_START + METADATA_SIZE) : {
       KEEP(*(.vectors))  /* Vector table first */
       *(.text*)
   }
   ```

### Metadata Generation

**In your application build system:**

```python
# Generate metadata (Python example)
metadata = struct.pack(
    '<24sIQI32s8sI',
    b'TERRABOOT_RaiseTheWorld\0',  # magic (24 bytes)
    app_size,                       # app_size (4 bytes)
    fasthash64(app_data),           # app_hash (8 bytes)
    0x00010000,                     # app_version v1.0.0 (4 bytes)
    b'MyApp\0' + b'\0' * 26,        # variant_name (32 bytes)
    b'\xFF' * 8,                    # reserved (8 bytes)
    fasthash32(metadata_without_crc) # meta_crc (4 bytes)
)
```

---

## FAQ

**Q: Why is FLASH_APPLICATION_ADDRESS unused?**
A: It's a legacy Katapult constant. Modern Terraboot uses `LAUNCH_APP_ADDRESS` which is more accurate since it points to where the app actually launches from.

**Q: Can I change bootloader size after deploying devices?**
A: No - changing bootloader size requires re-flashing both bootloader and application with matching addresses.

**Q: What happens if metadata is corrupted?**
A: Bootloader stays in bootloader mode, waiting for firmware via admin protocol. Application won't run.

**Q: Why is metadata 1KB when structure is only 84 bytes?**
A: Aligns with STM32 flash page size for easier erasing/writing. Unused bytes are padding.

**Q: Can I put data after the application code?**
A: Yes, any space from `LAUNCH_APP_ADDRESS + METADATA_SIZE + app_code_size` to end of flash is available for application data/parameters.

---

## Summary

| You Want To... | Use This Constant | Value (Example) |
|----------------|-------------------|-----------------|
| Know where flash starts | `CONFIG_FLASH_START` | `0x8000000` |
| Know where app metadata is | `CONFIG_LAUNCH_APP_ADDRESS` | `0x8002000` |
| Know where app code starts | `LAUNCH_APP_ADDRESS + METADATA_PAGE_SIZE` | `0x8002400` |
| Calculate bootloader size | `LAUNCH_APP_ADDRESS - FLASH_START` | `0x2000` (8KB) |
| Calculate writable app space | `FLASH_SIZE - bootloader_size` | `0xE000` (56KB) |
| Know total flash | `CONFIG_FLASH_SIZE` | `0x10000` (64KB) |

**Always remember:** Application vector table must be at `LAUNCH_APP_ADDRESS + 0x400`!
