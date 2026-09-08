# Prelude Terminal (reTerminal E1001 firmware)

BLE-controlled e-paper terminal for the Seeed reTerminal E1001 (ESP32-S3,
7.5" 800×480 1-bit panel, three buttons, buzzer, SHT4x, 2000 mAh battery).

The device is a dumb display: a server on a Mac or Linux box connects over
Bluetooth Low Energy, pushes full frames and short status messages, and
controls the buzzer. The device reports button presses, buzzer dismissal,
battery and sensor readings.

## Build / flash / monitor

    pio run -e reterminal_e1001
    pio run -e reterminal_e1001 -t upload
    pio device monitor -e reterminal_e1001

Logs go to the USB-C port at 115200. On the E1001 that port is a CH340
USB-serial bridge wired to UART0 (GPIO 43/44), not the ESP32-S3's native USB,
so the firmware logs on `Serial0` (see `src/log.h`). Uploads run at 115200
baud; faster rates make the chip stop responding through the bridge.

If the Homebrew `pio` fails generating the bootloader with
`No module named 'intelhex'`, use PlatformIO's own interpreter:
`~/.platformio/penv/bin/pio run ...`.

## Host tests

Pure logic (protocol codec, frame assembler, policies, SHT4x decoding, word
wrap) lives in `lib/protocol` and is tested on the host with Unity:

    pio test -e native

## Pairing

- Fresh device: the pairing page shows `Bluetooth Pairing...` and the
  advertised name `Prelude-XXXX`. The first host to connect bonds with
  Just Works (no PIN) and becomes the only host allowed to connect.
- Bonded device: the status line reads `Paired with <addr>, waiting for
  connection...`. Advertising uses the controller whitelist; other hosts
  cannot connect. (Build flag `PRELUDE_CONTROLLER_WHITELIST=0` disables the
  whitelist and relies on the application-level check only.)
- Unpair: hold the GREEN button while powering on. The page shows
  `Pairing cleared` and returns to open pairing.
- Connected: status line `Connected! Waiting for data...`. Once the server
  has sent a frame, the device stops drawing its own pages; the server owns
  the screen until reboot.

## BLE protocol

Service UUID `7e1d0000-6b6f-4a6b-9f1a-5072656c7564`. All characteristics
require an encrypted (bonded) link. Multi-byte integers are little-endian.

| Characteristic | UUID | Props | Purpose |
|---|---|---|---|
| info | `7e1d0001-…` | read | firmware/protocol info + battery |
| command | `7e1d0002-…` | write with response | one command per write |
| frame | `7e1d0003-…` | write without response | image chunks |
| event | `7e1d0004-…` | notify | device → host events |
| sensors | `7e1d0005-…` | read | TLV sensor snapshot |

Plus the standard Battery Service `0x180F` / Battery Level `0x2A19`
(read + notify).

### info (11 bytes)

```
u8  protocol_version   (1)
u8  fw_major, fw_minor, fw_patch
u16 width  (800)
u16 height (480)
u8  battery_percent
u16 battery_mv
```

### sensors

Concatenated records `[u8 type][u8 len][value]`. Records for hardware not
detected at boot are omitted. Refreshed every 30 s.

| Type | Sensor | len | Value |
|---|---|---|---|
| 0x01 | SHT4x temperature | 2 | `int16` centi-°C |
| 0x02 | SHT4x humidity | 2 | `uint16` centi-%RH |
| 0x03 | Battery | 3 | `u8 percent`, `u16 mV` |

### Commands (first byte = opcode)

| Opcode | Name | Payload | ACK sent |
|---|---|---|---|
| 0x01 | DISPLAY_STATUS | UTF-8 text, 1..500 bytes | after the refresh |
| 0x02 | FRAME_BEGIN | `u32 length` (48000), `u32 crc32` | immediately |
| 0x03 | FRAME_END | none | after the refresh |
| 0x10 | BUZZER_OFF | none | immediately |
| 0x11 | BUZZER_ON | none | immediately |
| 0x12 | BUZZER_ON_DISMISSABLE | none | immediately |

Every command gets exactly one ACK event. Unknown opcodes or bad payloads
get `ACK(opcode, BAD_ARG)`. If the device's command queue is full the ACK
status is `BUSY`; retry.

DISPLAY_STATUS draws a centered 560×240 box over whatever is on screen,
word-wrapped, up to 6 lines. BUZZER_ON beeps 300 ms on / 300 ms off until
BUZZER_OFF or disconnect. BUZZER_ON_DISMISSABLE does the same but a GREEN
press silences it and sends BUZZER_DISMISSED instead of BUTTON.

### Frame transfer

Image format: 800×480, 1 bit per pixel, row-major, 100 bytes per row, MSB is
the leftmost pixel, **1 = black**. Total 48,000 bytes. CRC32 is IEEE
(`zlib.crc32`).

1. Write `FRAME_BEGIN(length=48000, crc32)` → wait for `ACK(0x02, OK)`.
   `BUSY` means a frame is still open or being rendered; `BAD_ARG` means a
   wrong length.
2. Write chunks to `frame` without response: `u16 offset` followed by data.
   Any order; keep each write ≤ MTU − 3 bytes (up to 514 with MTU 517).
3. Write `FRAME_END` → the device checks that exactly 48,000 bytes arrived
   and the CRC matches, refreshes the panel (2–5 s), then sends
   `ACK(0x03, OK)`. Otherwise `INCOMPLETE` or `CRC_MISMATCH`.
4. Wait for that ACK before the next FRAME_BEGIN.

A frame left open for more than 10 s is discarded.

**macOS note for server authors:** CoreBluetooth silently discards
write-without-response packets queued while `canSendWriteWithoutResponse`
is false, and bleak does not wait for it. Wait for that flag (or the
`peripheralIsReadyToSendWriteWithoutResponse` callback) before each chunk,
as `tools/prelude_probe.py` does. Without it roughly half the chunks are
lost and the device answers `INCOMPLETE`. With it a full frame takes about
2–3 s to transfer plus 3.5 s to refresh.

### Events (first byte = type)

| Type | Name | Payload |
|---|---|---|
| 0x01 | BUTTON | `u8 id`: 0 LEFT, 1 RIGHT, 2 GREEN |
| 0x02 | BUZZER_DISMISSED | none |
| 0x03 | ACK | `u8 opcode`, `u8 status` |

ACK status: 0 OK, 1 BAD_ARG, 2 BUSY, 3 CRC_MISMATCH, 4 INCOMPLETE. Button
presses while no host is connected are dropped.

## Probe tool

`tools/prelude_probe.py` exercises the device from Mac or Linux and is the
reference for the server side.

    python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt
    .venv/bin/python tools/prelude_probe.py scan
    .venv/bin/python tools/prelude_probe.py info
    .venv/bin/python tools/prelude_probe.py sensors
    .venv/bin/python tools/prelude_probe.py listen
    .venv/bin/python tools/prelude_probe.py status "Hello world"
    .venv/bin/python tools/prelude_probe.py frame --test
    .venv/bin/python tools/prelude_probe.py frame picture.png
    .venv/bin/python tools/prelude_probe.py buzzer dismissable

On macOS use Python 3.11 or newer (the system 3.9 cannot build bleak's
dependencies). macOS caches peripheral names, so a board that previously ran
other firmware may show its old name in Bluetooth tools; the probe matches on
the advertised name and service UUID instead.

## Manual on-device checklist

1. Fresh flash → pairing page with header, hints, `Bluetooth Pairing...`
   and the battery percentage.
2. `prelude_probe.py info` → macOS shows a pairing prompt once; the page
   flips to `Connected! Waiting for data...` and info prints firmware 0.1.0.
3. `status "Hello"` → overlay box. `frame --test` → checkerboard with a
   solid black band across the top 20 rows (1 = black on the wire; verified
   on hardware, the inversion in `display::blitFrame` is correct).
4. `listen` then press LEFT/RIGHT/GREEN → BUTTON events.
5. `buzzer dismissable` → beeping; GREEN → BUZZER_DISMISSED and silence.
   `buzzer on` → GREEN sends BUTTON GREEN and keeps beeping; `buzzer off`.
6. Disconnect (Ctrl-C) → buzzer stops, screen unchanged, device advertises
   again; a second computer cannot connect.
7. Power-cycle holding GREEN → `Pairing cleared` overlay then
   `Bluetooth Pairing...`.

## Verified on hardware (2026-09-07)

Checklist items 1–6 pass on a reTerminal E1001 paired to a Mac: pairing and
bonding, whitelisted reconnect, status overlay, full test frame (48,000 bytes
in 2.2 s, refresh 3.4 s), button events, dismissable buzzer with GREEN, buzzer
stopping on disconnect. Item 7 (GREEN at power-on) and the second-computer
rejection have not been exercised yet.

## Layout

```
lib/board/board_pins.h     pin map
lib/protocol/prelude/      pure C++ protocol + policies (host-tested)
src/app.*                  state machine, command dispatch (owns display/buzzer)
src/ble_link.*             NimBLE service, bonding, whitelist, frame staging
src/display.*              EPaper pages, overlay box, raw blit
src/buttons.* buzzer.* battery.* sensors.*   hardware wrappers
test/                      Unity suites for lib/protocol
tools/prelude_probe.py     bleak-based dev client
```
