# HANDOFF.md
Last updated: 2026-02-13
Workspace: `d:\SourceCode\python\_HidTestTool`

---

## Layout Audit 2026-10-20

- Screenshot moved to `docs/images/main_window.jpg` with no overwrite and SHA-256 verification; README uses a relative link instead of an old repository URL.
- Added compatible `scripts/build_release_mfc.bat` and `scripts/clean_mfc.bat` aliases. Original entries and application behavior remain unchanged.
- PASS: `scripts/build.ps1 -Configuration Release -Platform x64`; Release x64, 0 warnings/errors. No user instance was force-closed and no device was connected for the audit.
- PASS: actual `scripts/clean_mfc.bat`; 4 existing protected build files unchanged by SHA-256.
- PASS: isolated clean sentinels/idempotence (9 protected build files plus source).
- NOT RUN: fresh UI/device/parser functional acceptance; this is a directory/script-alias change, not a claim that every product checklist passed.
- Commit/push status is recorded in workspace `#READ_ME_FIRST#/PROJECT_LAYOUT_MIGRATION.md`.

## Current Authority
- Spec baseline: `AGENTS.md` (Current UI behavior is source of truth)
- QA checklist: `SELF_CHECK.md`
- Rule: Any feature behavior change must update both files in same round

---

## Implemented In This Session

### 1) Startup busy feeling mitigation
- Added lightweight pre-window splash (`Initializing HID Test Tool...`) to mask first-load busy time
- Removed startup overlay inside main window (to avoid post-open flash)
- Startup scan is deferred by timer after window creation
- Startup device scan path uses lighter enumeration path
- Files:
  - `src/app.cpp`
  - `src/main_frame.h`
  - `src/main_frame.cpp`
  - `src/ui/device_bar.cpp`

### 2) TX/RX log format and ordering cleanup
- Unified log format:
  - Timestamp ON: `[HH:MM:SS.mmm] [TX] [<hex>]` / `[HH:MM:SS.mmm] [RX] [<hex>]`
  - Timestamp OFF: `[TX] [<hex>]` / `[RX] [<hex>]`
- RX is logged at receive-time (worker path), not delayed by UI flush
- Logger now mutex-protected for TX/RX concurrent writes
- Files:
  - `src/ui/rx_panel.h`
  - `src/ui/rx_panel.cpp`
  - `src/log/logger.h`
  - `src/log/logger.cpp`
  - `src/core/scheduler.h`
  - `src/core/scheduler.cpp`
  - `src/main_frame.cpp`

### 3) Command INI default behavior
- First launch behavior:
  - Runtime command file: `HidTestTool_commands.ini`
  - If missing, it is created from current panel values
  - Default snapshot file `HidTestTool_commands_default.ini` is created only if missing
- `Load` button remains file-picker based (no forced template load)
- Files:
  - `src/config/command_config_manager.h`
  - `src/config/command_config_manager.cpp`
  - `src/main_frame.cpp`

---

## Build Verification
- Command: `scripts\build_release.bat`
- Result: success (`0 warning`, `0 error`) on 2026-02-13
- Output:
  - `build\HidTestTool.exe`
  - `build\obj\x64\Release\HidTestTool.pdb`
- Script behavior verified: rebuild + relaunch exe

---

## Document Sync Status
- `AGENTS.md`: updated for startup splash + deferred startup scan + new log format
- `SELF_CHECK.md`: updated with startup splash checks and new log format checks
- `README.md`: updated to match current UI usage and logging style

---

## Next Chat Quick Start
1. Open `AGENTS.md` and follow current baseline.
2. Validate by running `SELF_CHECK.md` cases.
3. If startup still feels slow, next step is to profile heavy control creation paths in TX/RX panel initialization.
