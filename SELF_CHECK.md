# TCPTestTool Self Check

## Build

- Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Configuration Release -Platform x64
```

- Verify:
  - `build\TCPTestTool.exe` exists
  - `build\obj\x64\Release\TCPTestTool.pdb` exists

## Connection Bar

- Launch the tool
- Verify fields exist:
  - host
  - port
  - connect/disconnect
  - ping
  - auto reconnect
- Verify default values are `192.168.0.2` and `5000`
- After connecting and staying idle for more than one second, verify the connection bar still shows connected rather than RX timeout.
- Click `Ping` or `Burn Firmware` after connecting and verify the TCP socket remains open while the RX panel capture is paused.

## Config Files

- Close the tool after changing host/port/auto reconnect
- Verify these files are created next to the exe:
  - `TCPTestTool.ini`
  - `TCPTestTool_commands.ini`
  - `TCPTestTool_commands_default.ini`
- Relaunch and confirm values persist

## Console Tab

- Verify TX rows are visible
- Load a command ini file and confirm rows update
- Save and relaunch, then confirm rows persist

## RX Logging

- Enable RX log
- Receive at least one packet
- Verify a file named `YYYYMMDD_HHMMSS_tcp_rx.log` is created next to the exe

## Ping / Info

- Connect to a reachable MCU TCP endpoint
- Click `Ping`
- Verify success is shown in the UI/log
- Verify `GET_INFO` reads a valid peer mode:
  - APP
  - BOOT
- In APP mode, verify uptime increments once per second
- In BOOT mode, verify the tool does not continuously poll `GET_UPTIME`

## Function Tab

- Connect to APP mode
- Verify the Function tab has ADC / GPIO / PWM sub-tabs
- Verify each sub-tab has its own Poll checkbox
- Enable only `Poll ADC (A0)`:
  - ADC display fields update periodically
  - ADC display 1, ADC display 2, and ADC VREF use mV units
  - ADC raw TX/RX fields update periodically
  - GPIO and PWM fields remain disabled or inactive if their polling is unchecked
- Enable only `Poll GPIO (A1)`:
  - Press the EVB input button and verify GPIO input indicator changes
  - Toggle GPIO output and click `Apply GPIO`
  - Verify the GPIO Output checkbox keeps the requested state while GPIO input polling is active
  - Verify the EVB output LED changes
  - Verify the UI remains responsive while applying
- Enable only `Poll PWM (A2)`:
  - Type a non-default frequency such as `2000` and verify it is not overwritten by polling before Apply
  - Move frequency and duty sliders
  - Click `Apply PWM`
  - Verify the accepted PWM value updates after the write response
  - Verify PWM raw TX/RX fields update
  - Verify the UI remains responsive while applying
- Click `Refresh` on each sub-tab:
  - Verify one immediate RAW64 read is sent for the active sub-tab
  - Verify Refresh requires APP connection

## Firmware Tab

- Verify controls exist:
  - firmware path
  - browse
  - burn firmware
  - enter bootloader
  - reset MCU
  - progress bar

- Load a `.bin` file
- Verify the path is shown and persisted

## Firmware Update

- Start with APP firmware running
- Click `Enter Bootloader`
- Verify reconnect to bootloader succeeds
- Click `Burn Firmware`
- Verify progress moves from 0 to 100
- Verify `IAP_FINISH` completes without error
- Click `Reset MCU`
- Verify MCU boots into APP
- Verify the failure dialog, if update fails, shows:
  - failure offset
  - status
  - transport error
  - chunk size
  - image size

## Negative Checks

- Try connecting to a closed port and verify error handling
- Try burning without loading a file and verify the UI blocks the action
- Try firmware update with a corrupted file and verify bootloader rejects finish/verify
- Inject or simulate a TCP frame CRC mismatch and verify the UI reports CRC mismatch rather than only receive timeout
- Send oversized fixed-format IAP requests:
  - `IAP_START` larger than 12 bytes should be rejected
  - `IAP_FINISH` larger than 8 bytes should be rejected

## MCU Side Check

- APP target listens on TCP port `5000`
- BOOT target listens on TCP port `5000`
- Both targets respond to:
  - `PING`
  - `GET_INFO`
- APP accepts:
  - `RAW64`
  - `GET_UPTIME`
  - `RESET_TO_BOOT`
  - `RESET_MCU`
- BOOT accepts:
  - `RAW64`
  - `RESET_TO_BOOT`
  - `IAP_START`
  - `IAP_DATA`
  - `IAP_FINISH`
  - `RESET_MCU`
- BOOT rejects `GET_UPTIME` with `INVALID_STATE`
