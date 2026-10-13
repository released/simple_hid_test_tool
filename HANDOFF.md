# TCPTestTool Handoff

## Current Scope

`TCPTestTool` is the PC-side client for the M480 lwIP TCP firmware update project.

It was derived from the original HID desktop tool, but the active transport is now TCP.

## Implemented

- MFC desktop application builds as `TCPTestTool.exe`
- Connection bar changed from device scan to `Host + Port + Connect + Ping`
- Existing console TX/RX workflow retained
- Legacy USB HID report / echo / handshake controls removed from the active console UI
- TCP framed protocol added in `src\tcp\tcp_protocol.*`
- `RAW64` command used for the console page
- Firmware tab added for:
  - browse firmware
  - enter bootloader
  - burn firmware
  - reset MCU
  - progress display
- Function tab added for APP RAW64 packets:
  - ADC `0xA0`
  - GPIO `0xA1`
  - PWM `0xA2`
  - per-function sub-tabs and polling checkboxes
  - per-function TX/RX raw data display
- APP uptime polling added:
  - `GET_UPTIME` is polled only in APP mode
  - bootloader `GET_UPTIME` is treated as invalid state and should not be polled
- Function tab button commands are dispatched by the background control-poll worker to avoid blocking the MFC UI thread.
- TCP receive close path was hardened:
  - socket close wakes blocked receive/poll operations
  - receive buffers are cleared only under receive lock
  - CRC mismatch is reported as CRC mismatch instead of being swallowed as timeout
- RX capture pause no longer closes the TCP socket; firmware burn, ping, and reset commands can pause the RX thread without disconnecting.
- The connection bar reports socket connection state separately from idle RX timeout so a quiet but open TCP connection still shows connected.
- Function telemetry updates no longer overwrite pending GPIO output or PWM edit values during polling; write responses still synchronize accepted values.
- ADC display fields use mV units to match ADC VREF.
- Firmware failure reporting now separates:
  - failure offset
  - status
  - transport error
  - chunk size
  - image size
- Config persistence updated to:
  - `TCPTestTool.ini`
  - `TCPTestTool_commands.ini`
  - `TCPTestTool_commands_default.ini`
- RX log filename changed to `*_tcp_rx.log`
- Runtime config no longer stores HID report / handshake / echo settings

## Important Compatibility Choice

Some file and class names still use old `hid_*` naming:

- `src\hid\hid_device.*`
- `src\hid\hid_scan.*`
- `src\TCPTestTool.rc`
- `src\res\tcp.ico`

This was kept intentionally to reduce project churn while changing the runtime behavior to TCP.

## MCU Contract

The tool expects the MCU TCP server to support the framed protocol shared by:

- PC: `src\tcp\tcp_protocol.h`
- MCU: `SampleCode\Template\tcp_tool_protocol.h`
- MCU README: README in the MCU firmware repository.

Expected default endpoint:

- `192.168.0.2:5000`

Current MCU memory split:

- BOOT: `0x00000000 - 0x0001FFFF` (128KB)
- APP: `0x00020000 - 0x0007FFFF` (384KB)
- APP CRC32: `0x0007FFFC`

Strict bootloader IAP payload checks:

- `IAP_START`: exactly 12 bytes
- `IAP_DATA`: 8-byte prefix plus chunk data
- `IAP_FINISH`: exactly 8 bytes

## Known Gaps

- No automated UI test
- No end-to-end validation against real MCU hardware in this workspace
- No Keil build verification from this tool run

## Recommended Next Validation

1. Build `TCPTestTool` in Release x64.
2. Build both MCU targets in Keil.
3. Connect PC directly to MCU with static IPs on `192.168.0.x`.
4. Verify `PING` and `GET_INFO`.
5. Verify APP -> bootloader reset.
6. Verify `.bin` burn and reboot into APP.
