# AGENTS.md

## Scope

This file applies to this repository.

## Project Summary

`TCPTestTool` is an MFC desktop TCP client derived from the older HID test tool.

Current behavior:

- transport: TCP socket
- default endpoint: `192.168.0.2:5000`
- console page sends `RAW64` frames through the shared TCP protocol
- firmware page performs bootloader entry, binary transfer, progress update, and MCU reset

## Working Rules

- Keep changes small and localized.
- Preserve existing UI flow unless the task asks for a redesign.
- Prefer compatibility over mass renaming.
- It is acceptable for some source filenames to keep old `hid_*` naming if the runtime behavior is TCP.
- Update docs when behavior changes.

## Key Files

- Solution: `TCPTestTool.sln`
- Project: `TCPTestTool.vcxproj`
- App entry: `src\main.cpp`
- Main frame: `src\main_frame.cpp`
- TCP protocol: `src\tcp\tcp_protocol.h`
- Connection UI: `src\ui\device_bar.cpp`
- Firmware UI: `src\ui\firmware_panel.cpp`
- Transport implementation: `src\hid\hid_device.cpp`

## Build

Use:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Configuration Release -Platform x64
```

Expected output:

- `build\TCPTestTool.exe`
- `build\obj\<Platform>\<Configuration>\TCPTestTool.pdb`

Clean compiler intermediates while preserving executable, INI/profile, log, and other runtime files:

```bat
scripts\clean_build.bat
```

## Runtime Files

- `TCPTestTool.ini`
- `TCPTestTool_commands.ini`
- `TCPTestTool_commands_default.ini`
- `YYYYMMDD_HHMMSS_tcp_rx.log`

## Protocol Contract

This tool must stay aligned with the MCU shared protocol at:

- `SampleCode\Template\tcp_tool_protocol.h` in the MCU firmware repository.

Do not change command IDs, header layout, or CRC behavior on one side only.
