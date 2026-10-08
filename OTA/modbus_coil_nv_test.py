#!/usr/bin/env python3
"""
Modbus coil EEPROM persistence test (USB CDC RTU).

Uses the board's USB virtual COM port (Modbus slave id=1, coils 00000..00009).
Optional USART1 log port can confirm [MB-NV] save/load lines.

Typical usage:
  python modbus_coil_nv_test.py --list-ports
  python modbus_coil_nv_test.py --modbus-port COM7
  python modbus_coil_nv_test.py --modbus-port COM7 --log-port COM3 --reset

Requirements:
  pip install pyserial
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Iterable, Optional

COIL_COUNT = 10
SLAVE_ID = 1
DEFAULT_BAUD = 115200
USB_PRODUCT_HINT = "STM32 Virtual ComPort"

FC_READ_COILS = 0x01
FC_WRITE_SINGLE_COIL = 0x05
FC_WRITE_MULTIPLE_COILS = 0x0F

ROOT = Path(__file__).resolve().parents[1]


def log(msg: str, level: str = "INFO") -> None:
    ts = time.strftime("%H:%M:%S")
    print(f"[{ts}] [{level}] {msg}", flush=True)


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def pack_coils(values: Iterable[int]) -> bytes:
    bits = [1 if v else 0 for v in values]
    out = bytearray((len(bits) + 7) // 8)
    for idx, bit in enumerate(bits):
        if bit:
            out[idx // 8] |= 1 << (idx % 8)
    return bytes(out)


def unpack_coils(data: bytes, count: int) -> list[int]:
    values: list[int] = []
    for idx in range(count):
        values.append(1 if (data[idx // 8] >> (idx % 8)) & 1 else 0)
    return values


def coils_to_str(values: list[int]) -> str:
    return "".join("1" if v else "0" for v in values)


def parse_pattern(text: str) -> list[int]:
    cleaned = text.strip().replace(" ", "").replace(",", "")
    if len(cleaned) != COIL_COUNT or any(ch not in "01" for ch in cleaned):
        raise ValueError(f"pattern must be exactly {COIL_COUNT} bits of 0/1")
    return [1 if ch == "1" else 0 for ch in cleaned]


def invert_pattern(values: list[int]) -> list[int]:
    return [0 if v else 1 for v in values]


class ModbusRtuClient:
    def __init__(self, port: str, baud: int = DEFAULT_BAUD, slave: int = SLAVE_ID, timeout: float = 1.5):
        try:
            import serial  # type: ignore
        except ImportError as exc:
            raise SystemExit("pyserial is required: pip install pyserial") from exc

        self._serial = serial.Serial()
        self._serial.port = port
        self._serial.baudrate = baud
        self._serial.bytesize = serial.EIGHTBITS
        self._serial.parity = serial.PARITY_NONE
        self._serial.stopbits = serial.STOPBITS_ONE
        self._serial.timeout = 0.05
        self._serial.write_timeout = 2.0
        self._slave = slave
        self._timeout = timeout

    def open(self) -> None:
        if not self._serial.is_open:
            self._serial.open()
        self._serial.reset_input_buffer()
        self._serial.reset_output_buffer()
        time.sleep(0.05)

    def close(self) -> None:
        if self._serial.is_open:
            self._serial.close()

    def transact(self, pdu: bytes, expected_fc: int) -> bytes:
        frame = bytes([self._slave]) + pdu
        frame += struct.pack("<H", crc16_modbus(frame))

        self._serial.reset_input_buffer()
        self._serial.write(frame)
        self._serial.flush()
        time.sleep(0.02)

        deadline = time.monotonic() + self._timeout
        rx = bytearray()
        min_len = 5

        while time.monotonic() < deadline:
            chunk = self._serial.read(256)
            if chunk:
                rx.extend(chunk)
                if len(rx) >= min_len:
                    fc = rx[1]
                    if fc & 0x80:
                        min_len = 5
                    elif fc == FC_READ_COILS:
                        if len(rx) >= 3:
                            min_len = 3 + rx[2] + 2
                    elif fc in (FC_WRITE_SINGLE_COIL, FC_WRITE_MULTIPLE_COILS):
                        min_len = 8
                    if len(rx) >= min_len:
                        break
            else:
                time.sleep(0.01)

        if len(rx) < 5:
            raise TimeoutError(f"no Modbus response (got {len(rx)} bytes)")

        if rx[0] != self._slave:
            raise RuntimeError(f"unexpected slave id {rx[0]} (want {self._slave})")

        if (rx[1] & 0x80) != 0:
            raise RuntimeError(f"Modbus exception 0x{rx[2]:02X} for function 0x{rx[1] & 0x7F:02X}")

        if rx[1] != expected_fc:
            raise RuntimeError(f"unexpected function 0x{rx[1]:02X} (want 0x{expected_fc:02X})")

        payload = bytes(rx[:-2])
        recv_crc = struct.unpack("<H", rx[-2:])[0]
        if crc16_modbus(payload) != recv_crc:
            raise RuntimeError("Modbus CRC mismatch")

        return bytes(rx)

    def read_coils(self, start: int, count: int) -> list[int]:
        pdu = struct.pack(">BHH", FC_READ_COILS, start, count)
        resp = self.transact(pdu, FC_READ_COILS)
        byte_count = resp[2]
        data = resp[3 : 3 + byte_count]
        return unpack_coils(data, count)

    def write_multiple_coils(self, start: int, values: list[int]) -> None:
        packed = pack_coils(values)
        pdu = struct.pack(">BHHB", FC_WRITE_MULTIPLE_COILS, start, len(values), len(packed)) + packed
        self.transact(pdu, FC_WRITE_MULTIPLE_COILS)


def list_serial_ports() -> None:
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        raise SystemExit("pyserial is required: pip install pyserial")

    print("Available COM ports:")
    found = False
    for info in list_ports.comports():
        found = True
        hint = "  <-- likely Modbus USB CDC" if USB_PRODUCT_HINT.lower() in (info.description or "").lower() else ""
        print(f"  {info.device:8} {info.description}{hint}")
    if not found:
        print("  (none)")


def guess_modbus_port() -> Optional[str]:
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        return None

    for info in list_ports.comports():
        if USB_PRODUCT_HINT.lower() in (info.description or "").lower():
            return info.device
    return None


def wait_for_modbus_ready(client: ModbusRtuClient, retries: int = 20, interval: float = 0.5) -> list[int]:
    last_err: Optional[Exception] = None
    for attempt in range(1, retries + 1):
        try:
            values = client.read_coils(0, COIL_COUNT)
            log(f"Modbus ready (attempt {attempt}/{retries})")
            return values
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            time.sleep(interval)
    raise RuntimeError(f"Modbus not responding after {retries} tries: {last_err}")


def openocd_reset() -> bool:
    ocd = Path.home() / (
        "AppData/Local/Microsoft/WinGet/Packages/"
        "xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe/"
        "xpack-openocd-0.12.0-7/bin/openocd.exe"
    )
    scripts = ocd.parent.parent / "scripts"
    if not ocd.is_file():
        log("OpenOCD not found, skip auto reset", "WARN")
        return False

    cmd = [
        str(ocd),
        "-s",
        str(scripts),
        "-f",
        "interface/cmsis-dap.cfg",
        "-f",
        "target/stm32h7x_dual_bank.cfg",
        "-c",
        "set DUAL_CORE 1",
        "-c",
        "init",
        "-c",
        "reset run",
        "-c",
        "shutdown",
    ]
    log("reset MCU via OpenOCD ...")
    result = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True, check=False)
    if result.returncode != 0:
        log(f"OpenOCD reset failed: {result.stderr.strip() or result.stdout.strip()}", "WARN")
        return False
    return True


def capture_log(port: str, baud: int, stop: threading.Event, bucket: list[str]) -> None:
    try:
        import serial  # type: ignore
    except ImportError:
        return

    try:
        ser = serial.Serial(port, baud, timeout=0.2)
    except Exception as exc:  # noqa: BLE001
        log(f"log port open failed: {exc}", "WARN")
        return

    while not stop.is_set():
        try:
            data = ser.read(512)
        except Exception:  # noqa: BLE001
            break
        if data:
            bucket.append(data.decode("utf-8", errors="replace"))
    ser.close()


def wait_for_log_line(log_chunks: list[str], needle: str, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        text = "".join(log_chunks)
        if needle in text:
            return True
        time.sleep(0.1)
    return False


def reopen_after_reset(port: str, baud: int, wait_s: float) -> ModbusRtuClient:
    deadline = time.monotonic() + wait_s
    last_err: Optional[Exception] = None
    while time.monotonic() < deadline:
        try:
            client = ModbusRtuClient(port, baud=baud)
            client.open()
            values = wait_for_modbus_ready(client, retries=3, interval=0.5)
            return client
        except Exception as exc:  # noqa: BLE001
            last_err = exc
            try:
                client.close()
            except Exception:  # noqa: BLE001
                pass
            time.sleep(1.0)
    raise RuntimeError(f"could not reopen {port} after reset: {last_err}")


def run_test(args: argparse.Namespace) -> int:
    modbus_port = args.modbus_port or guess_modbus_port()
    if not modbus_port:
        log("Modbus COM port not specified and auto-detect failed", "ERROR")
        log("Use --list-ports, then pass --modbus-port COMx", "ERROR")
        return 2

    if args.pattern:
        target = parse_pattern(args.pattern)
    else:
        target = None

    log(f"Modbus port: {modbus_port}")
    if args.log_port:
        log(f"Log port:    {args.log_port} @ {args.log_baud}")

    client = ModbusRtuClient(modbus_port, baud=args.baud, timeout=args.timeout)
    client.open()

    log_chunks: list[str] = []
    stop_log = threading.Event()
    log_thread: Optional[threading.Thread] = None
    if args.log_port:
        log_thread = threading.Thread(
            target=capture_log,
            args=(args.log_port, args.log_baud, stop_log, log_chunks),
            daemon=True,
        )
        log_thread.start()

    try:
        before = wait_for_modbus_ready(client)
        log(f"current coils: {coils_to_str(before)}")

        if target is None:
            target = invert_pattern(before)
        log(f"write pattern: {coils_to_str(target)}")

        client.write_multiple_coils(0, target)
        after_write = client.read_coils(0, COIL_COUNT)
        log(f"read back:     {coils_to_str(after_write)}")
        if after_write != target:
            log("immediate read-back mismatch (RAM / Modbus path failed)", "ERROR")
            return 1
        log("step 1 OK: write + immediate read-back")

        save_ok = False
        if args.log_port:
            save_ok = wait_for_log_line(log_chunks, "[MB-NV] coil save OK", args.save_wait)
            if save_ok:
                log("step 2 OK: saw EEPROM save log")
            else:
                log("did not see [MB-NV] coil save OK on log port", "WARN")
        else:
            log(f"waiting {args.save_wait:.1f}s for async EEPROM save ...")
            time.sleep(args.save_wait)
            save_ok = True

        if args.no_reset:
            log("skip reset (--no-reset); persistence not verified", "WARN")
            return 0

        client.close()
        client = None  # type: ignore[assignment]

        if args.reset:
            if not openocd_reset():
                input("Press Enter after you manually reset/power-cycle the board ...")
        else:
            input("Press Enter after you reset/power-cycle the board ...")

        time.sleep(args.boot_wait)
        client = reopen_after_reset(modbus_port, args.baud, args.reopen_wait)
        restored = client.read_coils(0, COIL_COUNT)
        log(f"after reboot:  {coils_to_str(restored)}")

        if args.log_port and wait_for_log_line(log_chunks, "[MB-NV] coil load OK", 3.0):
            log("saw EEPROM load log after reboot")

        if restored == target:
            log("PASS: coil values persisted across reset")
            return 0

        log("FAIL: coil values changed after reset", "ERROR")
        log(f"expected: {coils_to_str(target)}", "ERROR")
        log(f"actual:   {coils_to_str(restored)}", "ERROR")
        return 1
    finally:
        stop_log.set()
        if log_thread is not None:
            log_thread.join(timeout=1.0)
        if client is not None:
            client.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Verify Modbus coil EEPROM persistence via USB CDC RTU")
    parser.add_argument("--list-ports", action="store_true", help="list COM ports and exit")
    parser.add_argument("--modbus-port", help="USB CDC COM port for Modbus RTU")
    parser.add_argument("--log-port", help="USART1 debug COM port (optional, for [MB-NV] logs)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="serial baud (default 115200)")
    parser.add_argument("--log-baud", type=int, default=DEFAULT_BAUD, help="log port baud")
    parser.add_argument("--pattern", help=f"{COIL_COUNT}-bit pattern to write, e.g. 1010101010")
    parser.add_argument("--timeout", type=float, default=1.5, help="Modbus response timeout (s)")
    parser.add_argument("--save-wait", type=float, default=2.0, help="wait for EEPROM save (s)")
    parser.add_argument("--boot-wait", type=float, default=2.0, help="wait after reset before reopen (s)")
    parser.add_argument("--reopen-wait", type=float, default=20.0, help="max wait to reopen Modbus port (s)")
    parser.add_argument("--reset", action="store_true", help="auto reset via OpenOCD instead of manual prompt")
    parser.add_argument("--no-reset", action="store_true", help="only test write/read, skip reboot check")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.list_ports:
        list_serial_ports()
        return 0

    return run_test(args)


if __name__ == "__main__":
    raise SystemExit(main())
