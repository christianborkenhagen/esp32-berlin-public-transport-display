"""Run the firmware's on-demand scroll benchmark without resetting the board."""

import argparse
import sys
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Board's serial port, e.g. COM6")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--tiles", type=int, choices=(2, 4))
    parser.add_argument("--avatar", choices=("normal", "hidden"))
    parser.add_argument("--avatar-format", choices=("original", "rgb565"))
    parser.add_argument("--effects", choices=("normal", "simple"))
    parser.add_argument("--screen", choices=("profile", "transport"), default="profile")
    parser.add_argument("--touch", type=int, choices=(10, 30))
    args = parser.parse_args()
    if args.screen == "transport" and (args.avatar or args.avatar_format or args.effects):
        parser.error("--avatar, --avatar-format and --effects apply only to Profile")

    # Set modem lines before opening so connecting does not reset the board.
    connection = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=2)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    try:
        connection.open()
        time.sleep(0.5)
        connection.reset_input_buffer()
        commands = bytearray()
        if args.tiles is not None:
            commands.extend(str(args.tiles).encode("ascii"))
        if args.avatar is not None:
            commands.extend(b"h" if args.avatar == "hidden" else b"s")
        if args.avatar_format is not None:
            commands.extend(b"r" if args.avatar_format == "rgb565" else b"o")
        if args.effects is not None:
            commands.extend(b"e" if args.effects == "simple" else b"n")
        if args.touch is not None:
            commands.extend(b"i" if args.touch == 10 else b"j")
        commands.extend(b"b" if args.screen == "profile" else b"c")
        connection.write(commands)
        deadline = time.monotonic() + args.timeout
        pending = b""
        while time.monotonic() < deadline:
            pending += connection.read(connection.in_waiting or 1)
            while b"\n" in pending:
                raw_line, pending = pending.split(b"\n", 1)
                line = raw_line.decode(errors="replace").strip()
                if line:
                    print(line, flush=True)
                if "Scroll benchmark config failed:" in line:
                    return 1
                if "Scroll benchmark starting:" in line:
                    expected = (
                        f"starting: {args.screen},",
                        f"tiles {args.tiles}" if args.tiles is not None else None,
                        f"avatar {args.avatar}" if args.avatar is not None else None,
                        (
                            f"format {args.avatar_format}"
                            if args.avatar_format is not None
                            else None
                        ),
                        f"effects {args.effects}" if args.effects is not None else None,
                        f"touch {args.touch} ms" if args.touch is not None else None,
                    )
                    if any(item not in line for item in expected if item is not None):
                        print("Benchmark settings did not match request.", file=sys.stderr)
                        return 1
                if "Scroll benchmark per update:" in line:
                    return 0
                if "Scroll benchmark timed out" in line:
                    return 1
            time.sleep(0.02)
        print("No completed scroll benchmark received before timeout.", file=sys.stderr)
        return 1
    finally:
        connection.close()


if __name__ == "__main__":
    sys.exit(main())
