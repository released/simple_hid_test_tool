# HANDOFF.md
Last updated: 2026-10-20
Workspace: `D:\SourceCode\python\simple_hid_test_tool`

---

## Repository Repair 2026-10-20

- Canonical destination: `https://github.com/released/simple_hid_test_tool.git`.
- The old `released/HidTestTool` URL redirects to this same repository; it is NOT a separate TCP repository.
- Remote main `4a8bb0410b559be2b5b754718273ea6f5857be67` accidentally contained TCPTestTool. Local HID layout commit `32e7ae7` was preserved, and merge `7f0d1e3` restores its HID tree while retaining the TCP commit as an ancestor. No force push/history deletion.
- Local safety branches: `backup/hid-before-repair-20261020` and `backup/accidental-tcp-before-repair-20261020`.
- Added `scripts/repository_identity.json`, `verify_repo_identity.ps1`, versioned `hooks/pre-push`, and `test_repo_identity.ps1`. Normal PowerShell/batch builds validate source and install the guard automatically; a custom existing hook is never overwritten.
- PASS: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_repo_identity.ps1`: 15 offline checks, including expected/opposite solution, merge markers, URLs, missing origin, idempotent install and custom-hook preservation.
- PASS: installed pre-push accepts the canonical HID URL and rejects the old redirected alias. This is a local accident-prevention check, not server-side security; never bypass it with `--no-verify`.
- PASS: `scripts/build.ps1 -Configuration Release -Platform x64`: 0 warnings, 0 errors; rebuilt the already tracked `build/HidTestTool.exe`.
- PASS: shared isolated startup/frame/normal-close test, exit 0. The first probe mistakenly selected the Afx splash and timed out; after requiring a captioned main frame the test passed. No application source was changed to accommodate the probe.
- PASS: shared clean-preservation verifier with `-CleanBatch scripts/clean_mfc.bat -CleanActualProject`: 9 protected fixture build files plus source retained, repeat clean passed, 4 actual protected build files unchanged (SHA-256).
- PASS: workspace static clean scan, 51 scripts with no known whole-build deletion pattern (not a universal safety proof).
- NOT RUN: physical HID/Echo/Handshake, full visual/DPI and Debug acceptance; no device commands were sent. The auto-close/relaunch batch wrapper was not executed because it force-closes existing instances.
- Publication result and exact final commit are recorded in workspace `#READ_ME_FIRST#/PROJECT_LAYOUT_MIGRATION.md`.

## Earlier Layout Audit 2026-10-20

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
