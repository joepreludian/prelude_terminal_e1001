#!/usr/bin/env python3
"""Exercise a Prelude Terminal (reTerminal E1001) over BLE from Mac or Linux.

    python3 tools/prelude_probe.py scan
    python3 tools/prelude_probe.py info
    python3 tools/prelude_probe.py sensors
    python3 tools/prelude_probe.py listen
    python3 tools/prelude_probe.py status "Hello world"
    python3 tools/prelude_probe.py frame picture.png
    python3 tools/prelude_probe.py frame --test
    python3 tools/prelude_probe.py buzzer on|dismissable|off
    python3 tools/prelude_probe.py power saving|performance
"""
import argparse
import asyncio
import struct
import sys
import zlib

from bleak import BleakClient, BleakScanner

SVC = "7e1d0000-6b6f-4a6b-9f1a-5072656c7564"
INFO = "7e1d0001-6b6f-4a6b-9f1a-5072656c7564"
COMMAND = "7e1d0002-6b6f-4a6b-9f1a-5072656c7564"
FRAME = "7e1d0003-6b6f-4a6b-9f1a-5072656c7564"
EVENT = "7e1d0004-6b6f-4a6b-9f1a-5072656c7564"
SENSORS = "7e1d0005-6b6f-4a6b-9f1a-5072656c7564"
BATTERY_LEVEL = "00002a19-0000-1000-8000-00805f9b34fb"

OP = {"status": 0x01, "frame_begin": 0x02, "frame_end": 0x03,
      "buzzer_off": 0x10, "buzzer_on": 0x11, "buzzer_dismissable": 0x12,
      "set_power_mode": 0x20}
POWER_MODE = {0: "saving", 1: "performance"}
OP_NAME = {v: k for k, v in OP.items()}
ACK_STATUS = {0: "OK", 1: "BAD_ARG", 2: "BUSY", 3: "CRC_MISMATCH", 4: "INCOMPLETE"}
BUTTONS = {0: "LEFT", 1: "RIGHT", 2: "GREEN"}
WIDTH, HEIGHT = 800, 480
FRAME_BYTES = WIDTH * HEIGHT // 8

acks: "asyncio.Queue[tuple[int, int]]" = asyncio.Queue()


def describe_event(data: bytes) -> str:
    if not data:
        return "empty event"
    t = data[0]
    if t == 0x01:
        return f"BUTTON {BUTTONS.get(data[1], data[1])}"
    if t == 0x02:
        return "BUZZER_DISMISSED"
    if t == 0x03:
        return f"ACK {OP_NAME.get(data[1], hex(data[1]))} -> {ACK_STATUS.get(data[2], data[2])}"
    return f"unknown event {data.hex()}"


def on_event(_, data: bytearray):
    data = bytes(data)
    print(f"  <- {describe_event(data)}")
    if data and data[0] == 0x03:
        acks.put_nowait((data[1], data[2]))


def advertised_name(dev, adv) -> str:
    # macOS caches peripheral names, so prefer the name from the scan response.
    return adv.local_name or dev.name or ""


def is_prelude(dev, adv) -> bool:
    return SVC in adv.service_uuids or advertised_name(dev, adv).startswith("Prelude-")


async def scan_prelude(timeout: float = 8.0):
    found = await BleakScanner.discover(timeout=timeout, return_adv=True)
    return [(d, adv) for d, adv in found.values() if is_prelude(d, adv)]


async def find_device(name):
    print("scanning...")
    for dev, adv in await scan_prelude():
        if not name or advertised_name(dev, adv) == name:
            dev.name = advertised_name(dev, adv)
            return dev
    sys.exit("no Prelude-* device found (is it paired to another host?)")


async def send_command(client: BleakClient, payload: bytes, timeout: float = 15.0):
    while not acks.empty():
        acks.get_nowait()
    await client.write_gatt_char(COMMAND, payload, response=True)
    opcode, status = await asyncio.wait_for(acks.get(), timeout)
    return opcode, status


def test_pattern():
    """Checkerboard of 40 px squares (top-left square black) with a labelled box."""
    from PIL import Image, ImageDraw, ImageFont
    img = Image.new("L", (WIDTH, HEIGHT), 255)
    d = ImageDraw.Draw(img)
    for y in range(0, HEIGHT, 40):
        for x in range(0, WIDTH, 40):
            if ((x // 40) + (y // 40)) % 2 == 0:
                d.rectangle([x, y, x + 39, y + 39], fill=0)
    d.rectangle([200, 200, 600, 280], fill=255, outline=0, width=3)
    try:
        font = ImageFont.load_default(size=22)  # Pillow >= 10.1 ships a scalable default
    except TypeError:
        font = ImageFont.load_default()
    d.text((220, 226), "Prelude Terminal test frame", fill=0, font=font)
    return img


def image_to_frame(path) -> bytes:
    from PIL import Image
    if path:
        img = Image.open(path).convert("L").resize((WIDTH, HEIGHT))
    else:
        img = test_pattern()
    bw = img.point(lambda p: 255 if p >= 128 else 0, mode="1")
    packed = bw.tobytes()                       # PIL packs 1 = white
    frame = bytes(~b & 0xFF for b in packed)   # protocol wants 1 = black
    assert len(frame) == FRAME_BYTES, len(frame)
    return frame


def _core_bluetooth_peripheral(client: BleakClient):
    """The CBPeripheral behind a bleak client on macOS, or None elsewhere."""
    backend = getattr(client, "_backend", None)
    periph = getattr(backend, "_peripheral", None)
    return periph if periph is not None and hasattr(periph, "canSendWriteWithoutResponse") else None


async def write_without_response(client: BleakClient, char: str, payload: bytes):
    """Write-without-response with back-pressure.

    CoreBluetooth silently discards writes queued while canSendWriteWithoutResponse
    is false, and bleak does not wait for it, so poll it here. BlueZ queues writes.
    """
    periph = _core_bluetooth_peripheral(client)
    if periph is not None:
        while not periph.canSendWriteWithoutResponse():
            await asyncio.sleep(0.001)
    await client.write_gatt_char(char, payload, response=False)


async def with_client(args, fn):
    dev = await find_device(args.name)
    print(f"connecting to {dev.name} ({dev.address})")
    async with BleakClient(dev, timeout=20.0) as client:
        if sys.platform.startswith("linux"):
            try:
                await client.pair()
            except Exception as e:  # already paired or agent handled it
                print(f"  pair(): {e}")
        await client.start_notify(EVENT, on_event)
        await fn(client)


async def cmd_scan(args):
    for dev, adv in await scan_prelude():
        print(f"{advertised_name(dev, adv)}  {dev.address}  rssi {adv.rssi}")


async def cmd_info(args):
    async def run(client):
        raw = bytes(await client.read_gatt_char(INFO))
        proto, major, minor, patch, w, h, pct, mv = struct.unpack("<BBBBHHBH", raw[:11])
        mode = POWER_MODE.get(raw[11], raw[11]) if len(raw) >= 12 else "n/a (protocol 1)"
        print(f"protocol {proto}, firmware {major}.{minor}.{patch}, {w}x{h}, "
              f"battery {pct}% ({mv} mV), power mode {mode}")
        level = await client.read_gatt_char(BATTERY_LEVEL)
        print(f"battery service level: {level[0]}%")
    await with_client(args, run)


async def cmd_sensors(args):
    async def run(client):
        raw = bytes(await client.read_gatt_char(SENSORS))
        i = 0
        while i + 2 <= len(raw):
            t, n = raw[i], raw[i + 1]
            v = raw[i + 2:i + 2 + n]
            i += 2 + n
            if t == 0x01:
                print(f"temperature {struct.unpack('<h', v)[0] / 100:.2f} C")
            elif t == 0x02:
                print(f"humidity {struct.unpack('<H', v)[0] / 100:.2f} %RH")
            elif t == 0x03:
                pct, mv = struct.unpack("<BH", v)
                print(f"battery {pct}% ({mv} mV)")
            else:
                print(f"sensor type {t:#x}: {v.hex()}")
    await with_client(args, run)


async def cmd_listen(args):
    async def run(client):
        print("listening for events, Ctrl-C to stop")
        while True:
            await asyncio.sleep(1)
    await with_client(args, run)


async def cmd_status(args):
    async def run(client):
        _, st = await send_command(client, bytes([OP["status"]]) + args.text.encode("utf-8"))
        print(f"status -> {ACK_STATUS.get(st)}")
    await with_client(args, run)


async def cmd_buzzer(args):
    async def run(client):
        _, st = await send_command(client, bytes([OP[f"buzzer_{args.mode}"]]))
        print(f"buzzer {args.mode} -> {ACK_STATUS.get(st)}")
        if args.mode != "off":
            print("listening 30 s for BUZZER_DISMISSED / buttons")
            await asyncio.sleep(30)
    await with_client(args, run)


async def cmd_power(args):
    async def run(client):
        mode = 0 if args.mode == "saving" else 1
        _, st = await send_command(client, bytes([OP["set_power_mode"], mode]))
        print(f"power {args.mode} -> {ACK_STATUS.get(st)}")
    await with_client(args, run)


async def cmd_frame(args):
    frame = image_to_frame(None if args.test else args.image)
    crc = zlib.crc32(frame) & 0xFFFFFFFF

    async def run(client):
        chunk = max(20, min(512, client.mtu_size - 3 - 2))
        print(f"mtu {client.mtu_size}, chunk {chunk} bytes, {-(-FRAME_BYTES // chunk)} chunks")
        _, st = await send_command(client, struct.pack("<BII", OP["frame_begin"], FRAME_BYTES, crc))
        if st != 0:
            sys.exit(f"frame_begin refused: {ACK_STATUS.get(st)}")
        loop = asyncio.get_event_loop()
        t0 = loop.time()
        for off in range(0, FRAME_BYTES, chunk):
            await write_without_response(client, FRAME, struct.pack("<H", off) + frame[off:off + chunk])
        print(f"sent {FRAME_BYTES} bytes in {loop.time() - t0:.2f} s, waiting for refresh")
        _, st = await send_command(client, bytes([OP["frame_end"]]), timeout=30.0)
        print(f"frame_end -> {ACK_STATUS.get(st)} after {loop.time() - t0:.2f} s")
    await with_client(args, run)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--name", help="exact device name, default: first Prelude-*")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("scan").set_defaults(fn=cmd_scan)
    sub.add_parser("info").set_defaults(fn=cmd_info)
    sub.add_parser("sensors").set_defaults(fn=cmd_sensors)
    sub.add_parser("listen").set_defaults(fn=cmd_listen)
    s = sub.add_parser("status")
    s.add_argument("text")
    s.set_defaults(fn=cmd_status)
    b = sub.add_parser("buzzer")
    b.add_argument("mode", choices=["on", "dismissable", "off"])
    b.set_defaults(fn=cmd_buzzer)
    pw = sub.add_parser("power")
    pw.add_argument("mode", choices=["saving", "performance"])
    pw.set_defaults(fn=cmd_power)
    f = sub.add_parser("frame")
    f.add_argument("image", nargs="?")
    f.add_argument("--test", action="store_true")
    f.set_defaults(fn=cmd_frame)
    args = p.parse_args()
    if args.cmd == "frame" and not args.test and not args.image:
        p.error("frame needs an image path or --test")
    try:
        asyncio.run(args.fn(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
