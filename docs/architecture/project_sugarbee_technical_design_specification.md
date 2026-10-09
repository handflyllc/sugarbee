# PROJECT SUGARBEE: TECHNICAL DESIGN SPECIFICATION

**Codename:** SugarBee  
**Target Architecture:** Raspberry Pi 4 (Model B, 2GB+ RAM)  
**Primary Goal:** Lightweight, dedicated homebrew gaming appliance & engine ecosystem  
**Status:** Architecture Phase / Initial Engine Drafting

---

## 1. System Vision & Platform Overview

Project SugarBee is a dedicated homebrew gaming appliance designed for the Raspberry Pi 4 environment. It strikes a balance between low-level hardware performance and modern software safety, using a native C host engine driving an embedded Lua scripting virtual machine for homebrew game development.

### Key Hardware & Platform Targets
* **Target SBC:** Raspberry Pi 4 (Model B, 2GB+ RAM recommended)
* **Target System Architecture:** ARM64 / ARMv8-A
* **Execution Environment Progression:**
  * **Phase A (Current):** Desktop Linux environment (SDL2 + OpenGL ES 3.0 windowed)
  * **Phase B:** Headless Linux via KMS/DRM framebuffer output (Buildroot/Yocto)
  * **Phase C:** Ultra-lean custom kernel / Bare-metal execution environment
* **Output Display:** Native 1280x720 (16:9) internal viewport, hardware-scaled to 1080p/4K HDMI outputs with support for lower-resolution integer-scaled framebuffers (e.g., 320x200) and custom shaders (CRT/Scanlines).
* **Frame Rate Target:** 60 FPS V-Sync locked.

---

## 2. Architecture & Runtime Execution Model

```
+-------------------------------------------------------------------+
|                     SUGARBEE HOST ENGINE (C)                      |
|  +---------------------+  +-----------------+  +---------------+  |
|  | SDL2 (I/O, Input)   |  | OpenGL ES 3.0   |  | Audio Engine  |  |
|  +---------------------+  +-----------------+  +---------------+  |
|                                                                   |
|  +-------------------------------------------------------------+  |
|  |                 EMBEDDED LUA 5.4 / LUAJIT VM                |  |
|  |  - Sandboxed execution heap                                 |  |
|  |  - Traps null pointers, missing files, array out-of-bounds    |  |
|  +-------------------------------------------------------------+  |
+-------------------------------------------------------------------+
                                  |
                   [ FAILED SCRIPT / FATAL FAULT ]
                                  v
+-------------------------------------------------------------------+
|               GURU MEDITATION FAULT HANDLER                       |
|  - Renders human-readable error summary                            |
|  - L1+R1+SELECT -> Toggles full call stack / VM register view    |
|  - START + USB -> Flushes crash log to /guru_logs/               |
+-------------------------------------------------------------------+
```

### Core Execution Principles
* **Host Engine Core:** Written in pure C utilizing SDL2 for windowing, controller management, and event loops, and OpenGL ES 3.0 for hardware-accelerated rendering.
* **Game Logic Runtime:** Embedded **Lua 5.4 / LuaJIT** VM. Game scripts run entirely sandboxed in the Lua memory space.
* **Fault Isolation:** Memory errors, null pointers, missing assets, or bad script logic do not crash the underlying C process or OS. Instead, the Lua error hook catches the fault and redirects the runtime directly to the custom **Guru Meditation Handler**.

---

## 3. Crash Handling & Error Reporting ("Guru Meditation v2")

SugarBee features a clean, retro-inspired error handler modelled after the classic Amiga Guru Meditation screen, prioritizing plain-English error reporting for end-users while retaining deep diagnostic data for developers.

### Display Modes & Behavior
1. **Primary Screen (Player View):**
   * High-contrast retro frame with a flashing border.
   * Human-readable primary error summary (e.g., `GURU MEDITATION #00000004`).
   * Plain-English explanation: `ERROR: Missing asset 'sprites/player.png' inside cartridge.`
2. **Developer Debug Overlay:**
   * Holding **`L1 + R1 + SELECT`** toggles a scrollable, low-level debug screen showing:
     * Full Lua call stack trace with source line numbers.
     * Memory allocation stats (VM memory used vs allocated heap).
     * Render batch state and active texture bindings.
3. **External USB Debug Logging:**
   * Pressing **`START`** while a FAT32/exFAT USB drive is attached writes a timestamped crash report (`/guru_logs/crash_YYYYMMDD_HHMMSS.txt`).

### Recovery Controls
* **Soft Reboot (`A` or `X` Button):** Safely resets the Lua VM state, unmounts the active cartridge, and returns to the System Dashboard.
* **Hard Shutdown (`SELECT + START` held for 3s):** Flushes disk caches, safely syncs the Linux filesystem, and powers down the Pi to prevent SD card corruption.

---

## 4. Virtual Cartridge System (`.vcart`)

SugarBee games are distributed as single-file "Virtual Cartridge" archives containing game code, graphics, audio, and metadata.

### 64-Byte Fixed Binary Header
```
Offset   Size     Type     Field Name               Description
--------------------------------------------------------------------------------
0x00     4 B      char[4]  magic_bytes              Magic identifier: "VCAR"
0x04     2 B      uint16   abi_version              SugarBee System ABI version
0x06     2 B      uint16   min_engine_major         Minimum required engine major version
0x08     2 B      uint16   min_engine_minor         Minimum required engine minor version
0x0A     2 B      uint16   min_engine_patch         Minimum required engine patch version
0x0C     4 B      uint32   hw_flags                 Hardware flags (e.g., Pi4, Shader GLES3)
0x10     32 B     char[32] game_title               Null-terminated game title
0x30     16 B     uint8[16] reserved                Reserved for future feature flags
--------------------------------------------------------------------------------
Total: 64 Bytes
```

### Compatibility Enforcement
* If `min_engine_version > system_installed_version`, the boot loop halts prior to cartridge mounting and displays an informative OS update splash screen:  
  > *"This virtual cartridge requires SugarBee OS v1.4.0 or higher. Installed version: v1.1.2. Please update your system."*

---

## 5. Input Subsystem
* **Driver Standard:** SDL2 `GameController` API.
* **Hot-Plugging Support:** Full dynamic detection and auto-assignment for controllers connected or disconnected during gameplay.
* **On-Screen Feedback:** System overlay toast notifications display when gamepads are attached or detached.

---

## 6. Audio Architecture
* **Phase 1 (Initial Build):** Tracked / Chiptune Audio playback engine (`.mod`, `.xm`, `.s3m` formats) alongside a basic waveform synthesizer. This ensures tiny cartridge file sizes and negligible CPU load.
* **Phase 2 (Future Expansion):** Streaming OGG Vorbis background music and uncompressed WAV sound effect buffers.

---

## 7. Storage & System Layout
* **Phase 1 Baseline:** Single SD card dual-partition setup:
  * Partition 1 (FAT32, Read-Only): Bootloader, Linux kernel, KMS/DRM drivers.
  * Partition 2 (ext4/FAT32, Read-Write): `/userdata/cartridges/*.vcart`
* **Phase 2 Expansion:** Automatic detection of USB flash drives mounted at `/media/usb/cartridges/`.

---

## 8. Milestone Development Roadmap

1. **Milestone 1 (Desktop Foundation - In Progress):**
   * Draft foundational C engine host with SDL2 windowing and embedded Lua 5.4 interpreter.
   * Implement basic game loop (Update/Draw) and Guru Meditation crash handler screen.
2. **Milestone 2 (Cartridge Parser & Graphics Pipeline):**
   * Write `.vcart` 64-byte binary header parser and ZIP payload extraction.
   * Build OpenGL ES 3.0 2D sprite batching and tilemap rendering pipeline.
3. **Milestone 3 (Headless KMS/DRM Port):**
   * Port host executable to run directly on Raspberry Pi OS without X11/Wayland using KMS/DRM framebuffer output.
4. **Milestone 4 (Buildroot Appliance System):**
   * Build custom, fast-booting Buildroot Linux distribution targeting sub-5-second boot directly into the SugarBee executable.