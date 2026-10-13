# TCPTestTool

Windows MFC TCP client for the M480 lwIP TCP bootloader/application project.

This project was cloned from `HidTestTool` and keeps the same general UI flow:

- Top connection bar
- Console tab for 64-byte command TX/RX
- Function tab for ADC / GPIO / PWM control
- Firmware tab for bootloader entry, binary load, burn, progress, and MCU reset

The transport layer is no longer HID. It now uses a TCP socket to talk to the MCU TCP server.

## Workspace

- PC tool: this repository.
- MCU project: M480 TCP IAP firmware repository.
- MCU project README: README in the MCU firmware repository.

## Features

- Connect to MCU by IP and port
- Ping MCU
- Read peer information
- Poll APP uptime once per second after APP mode is detected
- Send 64-byte raw command frames from the Console tab
- Receive and display RX data
- Optional RX logging to `YYYYMMDD_HHMMSS_tcp_rx.log`
- Function tab sub-pages for ADC / GPIO / PWM RAW64 packets
- Load firmware `.bin`
- Request APP to reset into bootloader
- Transfer firmware to bootloader over TCP
- Show burn progress
- Show firmware failure detail fields: offset, status, transport error, chunk size, image size
- Reset MCU after update

## Default Network

- MCU IP: `192.168.0.2`
- TCP port: `5000`
- Suggested PC IP: `192.168.0.10`
- Netmask: `255.255.255.0`

## Build

Requirements:

- Visual Studio with MFC and MSBuild
- Windows x64 or Win32 build environment

Build command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Configuration Release -Platform x64
```

Output:

- `build\TCPTestTool.exe`
- `build\obj\<Platform>\<Configuration>\TCPTestTool.pdb`

Build metadata:

- The connection summary area shows build date/time and build version/number.
- `scripts\update_build_info.ps1` runs before each build.
- The pre-build script bumps RC `FILEVERSION` / `PRODUCTVERSION`.
- The same pre-build script regenerates `src\build_info.h` for the UI build label.

Clean compiler intermediates while preserving the executable, INI/profile files, logs, and other runtime files:

```bat
scripts\clean_build.bat
```

## Runtime Files

- `TCPTestTool.ini`
  - host
  - port
  - auto reconnect
  - scheduler settings
  - RX logging settings
  - last firmware path
- `TCPTestTool_commands.ini`
  - runtime TX row definitions
- `TCPTestTool_commands_default.ini`
  - default TX row template

## UI Summary

### Connection Bar

- Host
- Port
- Connect / Disconnect
- Ping
- Auto reconnect
- Connection summary

### Console Tab

- TX rows based on the original tool flow
- Interval send and limit settings only
- RX view and optional log file
- No USB HID report / echo / handshake options remain in the active UI

Raw TX rows are packed into the TCP protocol as command `RAW64`.

### Function Tab

- ADC / GPIO / PWM are split into sub-tabs.
- Each sub-tab has its own polling checkbox.
- Polling is active only when connected to APP mode.
- Refresh sends one immediate read request for the current function sub-tab.
- Show ADC display 1 / display 2 / VREF in mV, plus band-gap raw
- Show GPIO input state
- Control GPIO output
- Control PWM frequency and duty
- Polling updates telemetry without overwriting pending GPIO output or PWM edit values.
- Show last TX / RX `RAW64` packet used by ADC / GPIO / PWM actions
- Function button actions are dispatched through the background control-poll worker, so button clicks do not block the UI thread.

### Firmware Tab

- Firmware path edit
- Browse
- Burn Firmware
- Enter Bootloader
- Reset MCU
- Progress bar
- Peer mode display

## Protocol Spec

The tool uses a framed TCP protocol defined in:

- PC: `src\tcp\tcp_protocol.h`
- MCU: `SampleCode\Template\tcp_tool_protocol.h`

### Frame Header

Each TCP frame starts with a fixed 20-byte header:

| Byte | Field | Note |
| --- | --- | --- |
| `0..3` | `magic` | ASCII `TCP1` |
| `4` | `command` | command ID |
| `5` | `status` | response status, request uses `0` |
| `6..7` | `reserved` | reserved, send `0` |
| `8..11` | `sequence` | request/response sequence number |
| `12..15` | `payload_length` | payload size in bytes |
| `16..19` | `payload_crc32` | CRC32 of payload only |

All multi-byte fields use little-endian format.
Protocol version is reported by `GET_INFO`, not stored as a separate frame-header byte.

### Common Status Codes

- `0x00` `OK`
- `0x01` `ERROR`
- `0x02` `INVALID_STATE`
- `0x03` `INVALID_PARAM`
- `0x04` `VERIFY_FAILED`
- `0x05` `BUSY`

### Peer Modes

- `0x00` `UNKNOWN`
- `0x01` `APP`
- `0x02` `BOOT`

### Command Table

- `0x01` `PING`
  - Request payload: none
  - Response payload: ASCII `PONG`
- `0x02` `GET_INFO`
  - Request payload: none
  - Response payload:

```c
typedef struct tcp_tool_get_info_payload
{
    uint8_t peer_mode;
    uint8_t protocol_version;
    uint16_t reserved;
    uint32_t app_start_addr;
    uint32_t app_size;
    uint32_t max_chunk_size;
    uint32_t uptime_seconds;
} tcp_tool_get_info_payload_t;
```

  - Wire size: `20 bytes`
- `0x03` `GET_UPTIME`
  - Request payload: none

<<<<<<< HEAD
```c
typedef struct tcp_tool_uptime_payload
{
    uint32_t uptime_seconds;
} tcp_tool_uptime_payload_t;
```

  - Wire size: `4 bytes`
- `0x10` `RAW64`
  - Request payload: fixed 64 bytes
  - APP response: echo 64 bytes, or function packet response for known APP packets
  - BOOT response: echo 64 bytes
- `0x20` `RESET_TO_BOOT`
  - Request payload: none
  - APP response: schedules checksum invalidation path and reset to bootloader
  - BOOT response: `OK` no-op
- `0x30` `IAP_START`
  - Request payload:
    - `u32 image_size`
    - `u32 image_crc32`
    - `u32 requested_chunk_size`
  - BOOT response payload:
    - `u32 accepted_chunk_size`
- `0x31` `IAP_DATA`
  - Request payload:
    - `u32 offset`
    - `u32 chunk_length`
    - `u8 data[chunk_length]`
  - BOOT response payload:
    - `u32 accepted_offset`
    - `u32 total_received`
- `0x32` `IAP_FINISH`
  - Request payload:
    - `u32 image_size`
    - `u32 image_crc32`
  - BOOT response payload: none
- `0x33` `RESET_MCU`
  - Request payload: none
  - Response payload: none
=======
`build_release.bat` behavior:
- Stops running `HidTestTool.exe` before build
- Relaunches `build\HidTestTool.exe` after successful build

## Clean Compiler Intermediates

```bat
scripts\clean_build.bat
```

Behavior:
- Removes known compiler/linker intermediates such as `build\obj` and debug symbol files
- Preserves EXE, INI/profile files, required DLLs, data, logs, and unknown delivery files
>>>>>>> 8da8a57074b3ae4bdf94a9994e982d1d1a285ef7

### APP Behavior

The APP TCP server accepts:

- `PING`
- `GET_INFO`
- `GET_UPTIME`
- `RAW64`
- `RESET_TO_BOOT`
- `RESET_MCU`

The APP TCP server rejects:

- `IAP_START`
- `IAP_DATA`
- `IAP_FINISH`

### RAW64 Function Packets

APP mode uses `RAW64` sub-packets for ADC / GPIO / PWM control.
These packets are now split by feature domain instead of keeping every function inside one shared telemetry layout.

- `0xA0` `ADC`
  - Request:
    - `Byte[0] = 0xA0`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2..63] = 0`
  - Response:
    - `Byte[0] = packet_id`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2..3] = adc_bandgap_raw`
    - `Byte[4..5] = adc_display_1_raw`
    - `Byte[6..7] = adc_display_2_raw`
    - `Byte[8..63] = reserved`
  - Voltage conversion is done in the PC tool from `adc_bandgap_raw` and the ADC raw values.
- `0xA1` `GPIO`
  - Read request:
    - `Byte[0] = 0xA1`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2..63] = 0`
  - Read response:
    - `Byte[0] = packet_id`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2] = gpio_input_status_1`
    - `Byte[3] = gpio_input_status_2`
    - `Byte[4..63] = reserved`
  - Current APP mapping:
    - `Byte[2]` = GPIO input demo status 1
    - `Byte[3]` = reserved for GPIO input demo status 2
  - Write request:
    - `Byte[0] = 0xA1`
    - `Byte[1] = 0xA5` for WRITE
    - `Byte[2] = gpio_output_write_1`
    - `Byte[3] = gpio_output_write_2`
    - `Byte[4..63] = reserved`
  - Current APP mapping:
    - `Byte[2]` controls the current GPIO output demo slot
    - `Byte[3]` is reserved
  - Write response:
    - `Byte[0] = packet_id`
    - `Byte[1] = 0xA5` for WRITE
    - `Byte[2] = gpio_output_status_1`
    - `Byte[3] = gpio_output_status_2`
    - `Byte[4..63] = reserved`
  - Current APP mapping:
    - `Byte[2]` = GPIO output demo status 1
    - `Byte[3]` = reserved for GPIO output demo status 2
- `0xA2` `PWM`
  - Read request:
    - `Byte[0] = 0xA2`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2..63] = 0`
  - Read response:
    - `Byte[0] = packet_id`
    - `Byte[1] = 0x5A` for READ
    - `Byte[2..4] = pwm_freq_1_hz_le24`
    - `Byte[5] = pwm_duty_1_percent`
    - `Byte[6..8] = pwm_freq_2_hz_le24`
    - `Byte[9] = pwm_duty_2_percent`
    - `Byte[10..63] = reserved`
  - Write request:
    - `Byte[0] = 0xA2`
    - `Byte[1] = 0xA5` for WRITE
    - `Byte[2..4] = pwm_freq_1_hz_le24`
    - `Byte[5] = pwm_duty_1_percent`
    - `Byte[6..8] = pwm_freq_2_hz_le24`
    - `Byte[9] = pwm_duty_2_percent`
    - `Byte[10..63] = reserved`
  - Current APP mapping:
    - slot 1 is the active PWM output
    - slot 2 is reserved

`Function` tab in `TCPTestTool` uses these packets and keeps `Firmware` as the last tab.

### BOOT Behavior

The bootloader TCP server accepts:

- `PING`
- `GET_INFO`
- `RAW64`
- `RESET_TO_BOOT`
- `IAP_START`
- `IAP_DATA`
- `IAP_FINISH`
- `RESET_MCU`

The bootloader returns `INVALID_STATE` for `GET_UPTIME`; uptime polling is APP-only.

### Firmware Transfer Rules

1. The PC should call `GET_INFO` after connect.
2. If the peer mode is `APP`, the PC should issue `RESET_TO_BOOT` and reconnect.
3. `IAP_START` erases the APP region and returns the accepted chunk size.
4. `IAP_DATA` must be sent sequentially with matching `offset`.
5. `IAP_FINISH` verifies final size and CRC, then checks APP validity.
6. After success, the PC may send `RESET_MCU` to boot the new APP.

Fixed-format IAP payloads are strict:

- `IAP_START` request payload must be exactly 12 bytes.
- `IAP_FINISH` request payload must be exactly 8 bytes.
- `IAP_DATA` uses an 8-byte fixed prefix plus variable-length chunk data.

### Packet Design Guidance

- Keep the framed TCP header fixed.
- Prefer one sub-packet for one feature domain instead of one ever-growing mega packet.
- Current APP examples already follow this pattern:
  - `0xA0` ADC
  - `0xA1` GPIO
  - `0xA2` PWM
- This scales better than forcing ADC / GPIO / PWM / future custom features into one packet forever.
- If future UI needs periodic updates, add per-feature polling controls or checkboxes on the PC side and keep each feature packet independent.

## Firmware Update Flow

1. Connect to APP or bootloader.
2. Run `GET_INFO`.
3. If peer mode is APP, request `RESET_TO_BOOT`.
4. Reconnect to bootloader.
5. Load `.bin`.
6. Send `IAP_START`.
7. Send `IAP_DATA` chunks.
8. Send `IAP_FINISH`.
9. Optionally click `Reset MCU`.

## Notes

- Several source filenames still keep historical `hid_*` names to avoid larger project churn.
- The actual runtime transport is TCP, not HID.
- The MCU side must expose the matching framed TCP protocol on port `5000`.
