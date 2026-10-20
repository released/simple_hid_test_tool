# AGENTS.md
## HID Test Tool — Full Tool Specification (as of 2026-02-12)

本文件為完整開發規格，已以「目前 UI / 行為」為準。
若與舊 Frozen Spec 或舊討論內容不同，以本文件為準。

---

## Workspace Layout

- Follow workspace `#READ_ME_FIRST#/PROJECT_LAYOUT.md`.
- Documentation screenshots live under `docs/images`; use relative README links.
- Canonical batch aliases: `scripts/build_release_mfc.bat` and `scripts/clean_mfc.bat`.
- Existing build/clean entry points remain compatible; only known compiler intermediates are disposable, never EXE/INI/DLL/data/log/evidence/unknown files.

## 1. Base Specification (Current UI Baseline)

### 1.1 Platform / Build Constraints
- OS: Windows 10/11 x64
- Language: C++
- UI: MFC (MFC in Shared DLL = No)
- Runtime: Static linking (/MT)
- HID API: SetupAPI + hid.dll (Windows HID class driver)
- No third-party libraries
- Build script: `scripts\build_release.bat`
- Clean script: `scripts\clean_build.bat` (removes compiler intermediates only; preserves delivery/runtime files)
- Build outputs:
  - `build\HidTestTool.exe`
  - `build\obj\x64\Release\HidTestTool.pdb`
- Build script behavior:
  - Stops any running `HidTestTool.exe` before build
  - After successful build, closes any running instance and relaunches `build\HidTestTool.exe`

### 1.2 Project Layout
- `src\` (ui, core, hid, log, config)
- `scripts\`
- `build\`

### 1.3 Main Window & Layout
- Initial size: 1024 x 768
- Start maximized
- Vertical order:
  1. DeviceBar
  2. TX Panel (Command Send)
  3. RX Panel (Monitor)
  4. Status Bar
- DeviceBar has no scroll bars
- DeviceBar height fixed; TX/RX take remaining space dynamically
- When extra vertical space is available:
  - Allocate additional height to TX panel first
  - RX panel stays at preferred height, with a small extra padding to avoid clipping
- UI theme (C): high-contrast instrument
  - Backgrounds: `#F8FAFC` → `#E2E8F0` gradient
  - Header band: `#E2E8F0`
  - Primary text: `#0F172A`
  - Accent: `#38BDF8`
  - Success: `#22C55E`
  - Warning: `#F97316`
  - Error: `#EF4444`
- Startup loading UX:
  - On startup, show a lightweight pre-window splash (`Initializing HID Test Tool...`) immediately after launch
  - Main-window overlay is not used at startup
  - Initial device scan is still deferred by timer after main window is shown

### 1.4 DeviceBar (Device selection & capability)
- Scan on startup (deferred timer after window shows); [Scan Device] triggers rescan
- Device combo format (sorted):
  `VID:PID  IN:xx OUT:xx  [Manufacturer - Product]`
  - If manufacturer/product missing, omit the text
- Device list sorting order:
  `VID` → `PID` → `UsagePage` → `Usage` → `Manufacturer` → `Product` → `Path`
- Connection control:
  - Single toggle button
  - Disconnected shows `Connect`; Connected shows `Disconnect`
  - Always enabled
- After successful connect:
  - `Scan Device` disabled
  - Device combo disabled
- Auto reconnect:
  - Checkbox `Auto reconnect`
  - When enabled and disconnected, rescan every 1s
  - Selection priority: `PathHash` → `VID/PID`
  - Uses last connected device first; falls back to saved config
- Capability summary (read-only):
  - `VID:PID`
  - `UsagePage/Usage`
  - `Reports IN/OUT/FEAT + ReportID` (payload lengths exclude ReportID byte)
  - `VID:PID` row uses a highlighted contrast background for quick visual identification
- Report hint row is removed
- Capability text rows use larger line spacing to avoid top/bottom clipping
- Capability text block width is auto-sized to fit the Reports row text + 10px margin
- `Copy Device Info` button:
  - Located on the right side of the capability block
  - Clipboard content:
    - VID:PID
    - UsagePage/Usage
    - InputReport / OutputReport / FeatureReport
    - PathHash
- Status indicator (top-right):
  - Colored dot + text, text right-aligned, dot on the left
  - States: Disconnected / Connected / RX Active / RX Timeout

### 1.5 TX Panel (Command Send)
- Header:
  - Title: `Command Send`
  - Handshake state indicator with dot: OFF / WAITING / OK / ERROR
- Layout spacing:
  - Scheduler row starts below header with extra top padding
- Global controls:
  - `Start TX` / `Stop TX` toggle
  - `Single-shot` (send one TX cycle)
  - `Set interval` checkbox + dropdown:
    - Options: 50 / 100 / 250 / 500 / 1000 / 2000 / 5000 ms
    - Dropdown disabled when unchecked
    - When unchecked, scheduler uses fixed 100 ms
    - Selection is preserved when toggling checkbox
  - ReportID input (0x00–0xFF), hex text
  - Handshake retry count: `Retry` with spin (min 5)
  - `Echo Mode` / `Handshake Mode` (mutually exclusive)
  - Help buttons:
    - `?` next to `Echo Mode` and `Handshake Mode`
    - Opens a dialog with PC→MCU and MCU→PC example payloads
  - Echo Mode auto command:
    - When Echo Mode is toggled, tool sends a 64-byte payload
    - Enable: `5A 5A EE 01` (remaining bytes `00`)
    - Disable: `5A 5A EE 00` (remaining bytes `00`)
    - Uses current ReportID and updates TX count/len/wrote status
  - On successful connect:
    - If Echo Mode is already checked, tool sends the Enable command once
  - `Save As` / `Load` for TX command INI
  - `Limit TX` checkbox + count (min 5) + status `Sent / Left`
  - `Last Echo` display:
    - 64-byte grid in the same format as TX command rows
    - Disabled/greyed out (display only)
    - Aligned with the 0..63 header band (same left/right)
  - `Last Echo` alignment:
    - Edit field aligns with the 0..63 header band (same left/right as header band)
- Enable/disable rules:
  - When disconnected: all TX controls disabled except `Save As` / `Load`
  - When connected & idle: controls enabled per mode rules
  - When TX running: editing controls disabled; Start button shows `Stop TX`
  - `Single-shot` disabled when `Limit TX` is enabled
- TX command grid:
  - 6 rows
  - Each row:
    - `Send N` checkbox
    - 64 byte hex cells (2 hex digits each, uppercase)
    - `Default` resets row to 0xFF * 64
    - `Send Once` sends only this row immediately
  - `Sel. all` toggles all row send checkboxes
- Input rules:
  - Only hex digits allowed in cells
  - Empty cell becomes `00` on focus loss
  - Single hex digit is padded (e.g., `A` → `0A`)
  - Paste supports `FF` / `0xFF` tokens separated by space/comma/semicolon
- Handshake row behavior:
  - `Handshake Mode` is the single effective handshake switch
  - When `Handshake Mode` is checked, Row 1 (`Send 1`) is the mandatory handshake key command
  - While `Handshake Mode` is checked, Row 1 controls are greyed out (locked):
    - `Send 1` checkbox
    - 64-byte data cells
    - `Default` / `Send Once`
  - Unchecking `Handshake Mode` restores Row 1 controls to normal editable state
  - `Echo Mode` and `Handshake Mode` remain mutually exclusive

### 1.6 Scheduler & Handshake Behavior
- TX cycle:
  - If handshake disabled: send enabled rows 1–6 in order
  - If no row enabled: error `No TX enabled.`
- Handshake flow (when enabled):
  - Send Row 1 payload as handshake key command
  - Wait for MCU echo (must match 64-byte payload)
  - On success: send enabled rows 2–6 in order
  - On failure: stop current cycle and emit `communication error (hand-shaking)`
- Handshake timeout:
  - Configurable via INI
  - Clamped to 200–500 ms (default 300 ms)
- Retry count:
  - Minimum 5
- Limit TX:
  - Counts TX cycles (not per-row)
  - When remaining reaches 0, scheduler stops automatically

### 1.7 HID Transport Rules
- TX uses Output Report; RX uses Input Report
- Logical payload size: 64 bytes
- Tool builds `[ReportID][64-byte payload]` (65 bytes)
- `HidDevice::WriteReport` adapts:
  - If device OutputReport length == 64, drop ReportID
  - If device OutputReport length == 65 and payload is 64, insert ReportID
- RX payload extraction:
  - If input report length == 65, ignore first byte (ReportID)

### 1.8 RX Panel (Monitor)
- Displays last 10 RX packets
- Oldest at top, newest at bottom
- Header states:
  - `RX: Idle`
  - `RX: Receiving` (RX within 100 ms)
  - `RX: Timeout` (no RX > 1 s)
  - `RX: Idle (cleared)` after Clear Data
- RX grid format:
  - 10 rows x 64 bytes
  - Same cell format as TX Last Echo (disabled/grey display)
  - X-axis aligned with TX panel data grid
- Controls:
  - `Save to log` (always enabled)
  - `Add timestamp` (always enabled; affects log only)
  - `Clear Data` (enabled only when disconnected)
- Log:
  - File name: `YYYYMMDD_HHMMSS_hid_rx.log`
  - Location: same folder as executable
  - Contains both TX and RX lines for correlation
  - RX lines are written at receive time (not UI batch flush) for better chronological order
  - Line format:
    - RX without timestamp: `[RX] [<hex bytes>]`
    - RX with timestamp: `[HH:MM:SS.mmm] [RX] [<hex bytes>]`
    - TX without timestamp: `[TX] [<hex bytes>]`
    - TX with timestamp: `[HH:MM:SS.mmm] [TX] [<hex bytes>]`

### 1.9 Status Bar
- Panes:
  - `Conn: ...`
  - `Err: ...` (only real error messages)
  - `TX: n (len=.. wrote=..)`
  - `RX: n`
  - `HS: OFF/WAITING/OK/ERROR`
  - Hover info: `TX Row N Byte[i] = 0xXX`
- Status bar pane widths adjust to window width to prevent overlap
- TX pane width must fit len/wrote text
- Err pane width must be large enough to avoid being covered by TX pane

### 1.10 Config Persistence
- Main config: `HidTestTool.ini` (same folder as exe)
- Command config: `HidTestTool_commands.ini`
- Default command template: `HidTestTool_commands_default.ini`
- First launch behavior:
  - If `HidTestTool_commands.ini` does not exist, TX rows stay at UI default (`0xFF * 64` per row) and are saved to `HidTestTool_commands.ini`
  - If `HidTestTool_commands_default.ini` does not exist, create it using the current project default-template content
  - Current default-template baseline is aligned to `HidTestTool_commands.ini` with `Row1Text` prefix: `5A 5A 11 22 33 44 55 66 77 88 99 00`
- `Load` button behavior:
  - Opens file picker for selecting a command INI file (e.g. `HidTestTool_commands.ini` or `HidTestTool_commands_default.ini`)
  - Loads selected file into current TX rows, then syncs current runtime copy back to `HidTestTool_commands.ini`
  - Does not replace existing `HidTestTool_commands_default.ini`
- Saved/loaded fields (HidTestTool.ini):
  - Device Path / VID / PID / PathHash
  - Interval enabled + interval ms
  - ReportID
  - Handshake mode enabled + timeout + retry count
  - Echo Mode enabled
  - Limit TX enabled + limit count
  - RX log enabled + timestamp enabled
- TX commands (HidTestTool_commands.ini):
  - RowNEnabled
  - RowNText
- Save on settings change and on app exit; load on startup

---

## 2. Active QA / Self-Check
請以 `SELF_CHECK.md` 為當前驗收清單。
