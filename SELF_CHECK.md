Post-Generation Self-Review Checklist (CURRENT)

After completing code generation, you MUST perform a self-review
against the following checklist.

This self-review is mandatory.
Do NOT skip any item.
Report PASS / FAIL for each test case.

==================================================
Self-Review Rules
==================================================

- Evaluate behavior strictly based on the generated code.
- Do NOT assume UI behavior without explicit implementation.
- If any item FAILS, clearly state the reason.
- Do NOT silently ignore failed items.

==================================================
Section 1 — DeviceBar State Validation
==================================================

[DB-01] Initial launch state
- Device is not connected.
- Verify:
  - Device combo enabled
  - Scan Device enabled
  - Connection control uses a SINGLE toggle button
  - Button label = "Connect" and button enabled
  - Auto reconnect is UNCHECKED by default
  - Copy Device Info enabled
  - Status shows "Disconnected"

Result: PASS / FAIL

[DB-02] After successful Connect
- Verify:
  - HID handle is opened
  - Connection control uses a SINGLE toggle button
  - Button label = "Disconnect" and button enabled
  - Device combo disabled
  - Scan Device disabled
  - Status shows "Connected" or "RX Active"

Result: PASS / FAIL

[DB-03] Restart selection retention
- Close tool, reopen, and rescan.
- Verify:
  - Device combo auto-selects the last connected device
  - Match priority: PathHash > VID/PID

Result: PASS / FAIL

[DB-04] Device list format and sorting
- Verify each device label format:
  - "VID:PID  IN:xx OUT:xx  [Manufacturer - Product]"
  - If manufacturer/product missing, text is omitted
- Verify sort order:
  - VID → PID → UsagePage → Usage → Manufacturer → Product → Path

Result: PASS / FAIL

[DB-05] Copy Device Info content
- Click "Copy Device Info" for a selected device
- Verify clipboard content includes:
  - VID:PID
  - UsagePage/Usage
  - InputReport / OutputReport / FeatureReport
  - PathHash

Result: PASS / FAIL

[DB-06] Reports row includes ReportID
- Select a device and verify capability row shows:
  - `Reports        : IN=<x>  OUT=<y>  FEAT=<z>  ReportID=0xNN`
- Change ReportID (e.g., 0x00 -> 0x01)
- Verify `ReportID=0xNN` updates immediately in the Reports row
- Verify no extra Report hint row is shown below capability lines
- Verify capability text lines are fully visible (no top/bottom clipping)
- Verify capability text block width is not over-stretched (fits Reports row with small extra margin)

Result: PASS / FAIL

[DB-07] Auto reconnect behavior
- Enable Auto reconnect
- Disconnect device and reattach
- Verify tool rescans every 1s and reconnects
- Verify selection priority: PathHash > VID/PID

Result: PASS / FAIL

[DB-08] Startup deferred scan responsiveness
- Launch tool
- Verify a startup splash appears immediately after launch
- Verify no startup overlay flash appears after main window is already visible
- Verify device scan and selection restoration happen shortly after main window is shown

Result: PASS / FAIL

[DB-09] VID:PID highlight visibility
- Verify Device Capability `VID:PID` row has a contrast highlight background
- Verify highlighted row remains readable in connected/disconnected states

Result: PASS / FAIL

==================================================
Section 2 — TX Panel Enable / Disable Logic
==================================================

[TX-01] Device Disconnected
- Verify all TX controls are disabled EXCEPT:
  - Save As
  - Load

Result: PASS / FAIL

[TX-02] Device Connected, TX IDLE
- Verify:
  - Start TX enabled
  - Set interval enabled
  - Interval dropdown disabled (unchecked)
  - ReportID input enabled
  - Retry count enabled
  - Echo Mode / Handshake Mode enabled
  - Limit TX controls enabled
  - Single-shot enabled ONLY when Limit TX is unchecked

Result: PASS / FAIL

[TX-03] Set interval checkbox behavior
- When checked:
  - Interval dropdown enabled
  - Dropdown options include:
    50 ms / 100 ms / 250 ms / 500 ms / 1000 ms / 2000 ms / 5000 ms
- When unchecked:
  - Dropdown disabled
- Selected value preserved

Result: PASS / FAIL

[TX-10] TX header spacing
- Verify the Command Send header stays at the top
- Verify the scheduler row is shifted downward with visible top padding

Result: PASS / FAIL

==================================================
Section 3 — TX Scheduler Toggle Behavior
==================================================

[TX-04] Start TX
- Verify:
  - Button label changes to "Stop TX"
  - TX State = RUNNING
  - Single-shot disabled
  - TX row controls disabled
  - Interval, ReportID, Retry, Limit, Mode toggles disabled

Result: PASS / FAIL

[TX-05] Stop TX
- Verify:
  - Button label returns to "Start TX"
  - TX State = IDLE
  - TX row controls enabled
  - Single-shot enabled ONLY when Limit TX is unchecked

Result: PASS / FAIL

[TX-06] Limit TX behavior
- Enable Limit TX and set count to a value >= 5
- Verify "Sent" starts at 0 and "Left" equals count
- Start TX and let it run
- Verify Sent increments and Left decrements per TX cycle
- When Left reaches 0:
  - Scheduler stops automatically
  - Start button returns to "Start TX"
- Verify Single-shot remains disabled while Limit TX is enabled

Result: PASS / FAIL

[TX-07] Save As / Load availability
- Verify Save As and Load are enabled even when disconnected

Result: PASS / FAIL

[TX-08] Last Echo display
- When RX data arrives, verify Last Echo shows the latest 64-byte hex payload

Result: PASS / FAIL

[TX-09] Last Echo alignment
- Verify Last Echo is rendered as a 64-byte grid (same format as TX command rows)
- Verify the grid is disabled/greyed out (display only)
- Verify the grid aligns with the 0..63 header band (same left/right)

Result: PASS / FAIL

==================================================
Section 4 — TX Command Rows
==================================================

[TXR-01] Disconnected state
- Verify all TX row controls disabled

Result: PASS / FAIL

[TXR-02] Connected + TX IDLE
- Verify all TX row controls enabled

Result: PASS / FAIL

[TXR-03] TX RUNNING
- Verify all TX row controls disabled

Result: PASS / FAIL

[TXR-04] Default button
- Click Default on any row
- Verify row bytes reset to 0xFF * 64

Result: PASS / FAIL

[TXR-05] Send Once button
- Click Send Once on a row
- Verify:
  - TX count increments
  - Status bar TX pane shows "len=.. wrote=.."

Result: PASS / FAIL

==================================================
Section 4A — TX Command Persistence & File Ops
==================================================

[TXC-01] Command INI separation
- Verify:
  - HidTestTool.ini does NOT contain RowNEnabled / RowNText
  - HidTestTool_commands.ini contains RowNEnabled / RowNText

Result: PASS / FAIL

[TXC-02] Command auto restore
- Modify TX command bytes and row enable state
- Close tool and reopen
- Verify TX commands are restored from last session

Result: PASS / FAIL

[TXC-03] Save As
- Click Save As
- Choose target path
- Verify exported ini contains RowNEnabled / RowNText

Result: PASS / FAIL

[TXC-04] Load button file selection
- Click `Load`
- Verify file picker appears
- Select a valid command INI (for example `HidTestTool_commands_default.ini`)
- Verify TX panel updates to selected file content
- Verify runtime `HidTestTool_commands.ini` is updated to the loaded content

Result: PASS / FAIL

[TXC-05] First launch command files
- On a clean machine/folder (no command ini files), launch tool once
- Verify `HidTestTool_commands.ini` is created
- Verify `HidTestTool_commands_default.ini` is created
- Verify initial `HidTestTool_commands.ini` rows are UI defaults (`0xFF * 64`)
- Verify created `HidTestTool_commands_default.ini` matches project default-template baseline:
  - `Row1Enabled=1`
  - `Row1Text` starts with `5A 5A 11 22 33 44 55 66 77 88 99 00`
  - `Row2~Row6Text` start with `5A 5A 1A` / `1B` / `1C` / `1D` / `1E`
- Modify TX rows and save/restart
- Verify `HidTestTool_commands.ini` follows latest rows
- Verify existing `HidTestTool_commands_default.ini` is not overwritten

Result: PASS / FAIL

[TXC-06] Recreate missing default template
- Keep an existing edited `HidTestTool_commands.ini`
- Delete only `HidTestTool_commands_default.ini`
- Relaunch tool
- Verify recreated `HidTestTool_commands_default.ini` uses project default-template baseline (not edited command rows)
- Verify `HidTestTool_commands.ini` still keeps edited runtime rows

Result: PASS / FAIL

==================================================
Section 5 — Handshake Controls
==================================================

[HS-01] Enable conditions
- Verify Handshake Mode checkbox enabled only when:
  - Device Connected
  - TX State = IDLE
  - Echo Mode is NOT enabled

Result: PASS / FAIL

[HS-02] TX RUNNING
- Verify Handshake Mode checkbox disabled
- Handshake status remains visible

Result: PASS / FAIL

[HS-03] Handshake effective enable
- Verify handshake is effective ONLY when:
  - Handshake Mode is checked

Result: PASS / FAIL

[HS-04] Handshake indicator states
- When handshake starts: indicator shows WAITING
- On success: indicator shows OK
- On failure: indicator shows ERROR and status bar shows "communication error (hand-shaking)"

Result: PASS / FAIL

[HS-05] Retry count minimum
- Set Retry to a value < 5 and leave field
- Verify value clamps back to 5

Result: PASS / FAIL

[HS-06] Handshake Send 1 lock behavior
- Check `Handshake Mode`
- Verify TX Row 1 controls are auto-locked and greyed out:
  - `Send 1` checkbox auto-checked and not directly editable
  - 64-byte data cells disabled
  - `Default` / `Send Once` disabled
- Uncheck `Handshake Mode`
- Verify TX Row 1 controls return to normal editable state

Result: PASS / FAIL

==================================================
Section 5A — TX Mode Toggles
==================================================

[MODE-01] Mutual exclusion
- If Echo Mode is checked:
  - Handshake Mode is disabled
- If Handshake Mode is checked:
  - Echo Mode is disabled

Result: PASS / FAIL

[MODE-02] Mode toggle state while running
- Start TX
- Verify Echo Mode and Handshake Mode are disabled

Result: PASS / FAIL

[MODE-03] Echo Mode auto command
- With device connected and TX idle:
  - Check Echo Mode
  - Verify one TX write occurs (TX count increments and len/wrote shown)
  - Verify payload starts with: `5A 5A EE 01` (remaining bytes `00`)
  - MCU should echo the same payload as confirmation
- Uncheck Echo Mode
  - Verify one TX write occurs (TX count increments)
  - Verify payload starts with: `5A 5A EE 00` (remaining bytes `00`)

Result: PASS / FAIL

[MODE-04] Echo Mode auto command on connect
- Enable Echo Mode while disconnected (state retained)
- Connect to device
- Verify one TX write occurs immediately after connect
- Verify payload starts with: `5A 5A EE 01` (remaining bytes `00`)

Result: PASS / FAIL

[MODE-05] Mode help dialogs
- Click `?` next to Echo Mode
  - Dialog appears with PC→MCU and MCU→PC examples
  - Echo enable/disable command (`5A 5A EE 01/00`) is shown
- Click `?` next to Handshake Mode
  - Dialog appears with PC→MCU and MCU→PC examples
  - Describes Send 1 payload + exact echo match

Result: PASS / FAIL

==================================================
Section 6 — RX Monitor Controls
==================================================

[RX-01] Clear Data enable rules
- Verify:
  - Enabled only when Device Disconnected
  - Disabled when Device Connected

Result: PASS / FAIL

[RX-02] Clear Data action
- When clicked:
  - RX grid is fully cleared
  - RX status becomes "RX: Idle (cleared)"

Result: PASS / FAIL

[RX-03] Log controls
- Save to log and Add timestamp remain enabled in all states

Result: PASS / FAIL

[RX-04] RX order
- Verify RX rows order:
  - Oldest data at top
  - Newest data at bottom

Result: PASS / FAIL

[RX-07] RX grid format and alignment
- Verify RX grid uses 10x64 disabled cells (same style as TX Last Echo)
- Verify RX rows use the same row height and gap as TX rows
- Verify RX data grid aligns on X-axis with TX panel data grid (63rd column matches)

Result: PASS / FAIL

[RX-08] RX grid live update
- Run continuous TX/RX
- Verify RX rows 1–10 update continuously without stalling
- Stopping TX should not be required to see new RX values

Result: PASS / FAIL

[RX-05] Timestamp UI isolation
- When "Add timestamp" is checked:
  - RX data grid UI layout does NOT change
  - Only log output gains timestamp prefix

Result: PASS / FAIL

[RX-06] Log file path and format
- Enable Save to log and receive RX data
- Verify log file name matches:
  - YYYYMMDD_HHMMSS_hid_rx.log
- Verify log file is in the same folder as HidTestTool.exe
- Verify line format:
  - RX with timestamp: [HH:MM:SS.mmm] [RX] [<hex bytes>]
  - RX without timestamp: [RX] [<hex bytes>]
  - TX with timestamp: [HH:MM:SS.mmm] [TX] [<hex bytes>]
  - TX without timestamp: [TX] [<hex bytes>]

Result: PASS / FAIL

[RX-09] TX/RX correlation logging
- Enable Save to log
- Trigger at least one TX cycle with Handshake Mode enabled
- Verify log includes both TX and RX lines in chronological order
- Verify TX payload lines and RX payload lines can be paired by sequence/time
- Verify RX lines are not delayed until UI stop/flush (continuous run still updates log in time order)

Result: PASS / FAIL

==================================================
Section 7 — Status Consistency
==================================================

[ST-01] Status separation
- Verify the following states are independent:
  - Transport Status (Connected / Disconnected / RX Active / RX Timeout)
  - RX State
  - Handshake State
  - TX State

Result: PASS / FAIL

[ST-02] No implicit state inference
- RX Active does NOT imply Handshake OK
- RX Timeout does NOT disable TX automatically

Result: PASS / FAIL

==================================================
Section 8 — Status Bar Output
==================================================

[SB-01] TX pane write info
- Verify TX pane shows:
  - "TX: <count> (len=.. wrote=..)" after any write
- Err pane only shows actual error messages
- TX pane text is not overlapped by RX pane

Result: PASS / FAIL

[SB-03] Err pane visibility
- Trigger a long error message (e.g., write failure)
- Verify Err pane text is not overlapped by TX pane

Result: PASS / FAIL

[SB-02] Hover byte info
- Move mouse over TX grid bytes
- Verify hover pane shows:
  - "TX Row N Byte[i] = 0xXX"
- Move mouse away and verify hover pane clears

Result: PASS / FAIL

==================================================
Section 9 — Config Persistence
==================================================

[CFG-01] INI location
- Verify HidTestTool.ini and HidTestTool_commands.ini are created
  in the same folder as HidTestTool.exe

Result: PASS / FAIL

[CFG-02] Persisted fields
- Modify and restart; verify persistence for:
  - Device selection (PathHash / VID / PID)
  - Interval enabled + interval ms
  - ReportID
  - Echo Mode / Handshake Mode
  - Handshake retry count
  - Limit TX enabled + limit count
  - Save to log + Add timestamp

Result: PASS / FAIL

==================================================
Section 10 — Global UI Consistency
==================================================

[G-01] Enable/Disable does not modify values
- Verify no UI control value is changed
  when being enabled or disabled

Result: PASS / FAIL

[G-03] UI theme colors (C)
- Verify panels use high-contrast light gradients
- Verify header bands use `#E2E8F0`
- Verify status indicators use C palette (success/active/warning/error)

Result: PASS / FAIL

[G-02] Immediate UI update
- Verify all state changes update UI immediately
- No delayed or deferred refresh

Result: PASS / FAIL

[LAY-01] Panel height allocation
- With a tall window:
  - RX panel stays at preferred height (no large blank area)
  - Extra vertical space is given to TX panel

Result: PASS / FAIL

==================================================
Section 11 — Build Output
==================================================

[BLD-02] PDB output location
- Verify Release build writes HidTestTool.pdb to:
  - build\obj\x64\Release\
- No .pdb exists next to build\HidTestTool.exe

Result: PASS / FAIL

[BLD-03] Build script auto-restart
- Run `scripts\build_release.bat`
- Verify any running HidTestTool is closed
- Verify `build\HidTestTool.exe` launches after successful build

Result: PASS / FAIL

[BLD-04] Clean script keep exe only
- Run `scripts\clean_build.bat`
- Verify `build\HidTestTool.exe` is kept (if existed before clean)
- Verify all other files/folders under `build\` are removed

Result: PASS / FAIL

==================================================
Final Self-Review Summary
==================================================

- Total test cases:
- PASS count:
- FAIL count:

If FAIL count > 0:
- List failed items and reasons.
- State what must be fixed before completion.
