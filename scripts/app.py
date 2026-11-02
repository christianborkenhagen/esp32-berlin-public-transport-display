#!/usr/bin/env python3
"""Portable firmware and desktop workflows. Run from any directory."""

import argparse
import collections.abc
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = ROOT / "desktop" / "lvgl" / "build-app"


def call(
    args: collections.abc.Sequence[str | pathlib.Path],
    env: dict[str, str] | None = None,
) -> None:
    """Run a command from the project root, displaying it first."""
    command = [str(arg) for arg in args]
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def pio() -> str:
    """Find PlatformIO Core or explain how to make it available."""
    tool = shutil.which("pio") or shutil.which("platformio")
    if not tool:
        raise SystemExit(
            "PlatformIO CLI not found on PATH. Install PlatformIO Core first."
        )
    return tool


def simulator(smoke: bool) -> None:
    """Build and run the mock-data desktop simulator."""
    env = os.environ.copy()
    if sys.platform == "win32":
        mingw = pathlib.Path("C:/msys64/mingw64/bin")
        # Prefer MSYS2 over unrelated GCC installations when it is available.
        if (mingw / "gcc.exe").exists():
            env["PATH"] = f"{mingw}{os.pathsep}{env.get('PATH', '')}"
    cmake = shutil.which("cmake", path=env.get("PATH"))
    if not cmake:
        raise SystemExit("CMake not found on PATH.")
    configure: list[str | pathlib.Path] = [
        cmake,
        "-S",
        ROOT / "desktop/lvgl",
        "-B",
        BUILD,
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    if not (BUILD / "CMakeCache.txt").exists() and shutil.which(
        "ninja", path=env.get("PATH")
    ):
        configure += ["-G", "Ninja"]
    call(configure, env)
    call([cmake, "--build", BUILD, "--config", "Release"], env)
    name = "lvgl_pc.exe" if sys.platform == "win32" else "lvgl_pc"
    exe = BUILD / name
    if not exe.exists():
        exe = BUILD / "Release" / name
    call([exe] + (["--smoke-test"] if smoke else []), env)


def main() -> None:
    """Dispatch the requested firmware or simulator workflow."""
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("build", help="Build firmware")
    run = sub.add_parser("run", help="Build and run the desktop simulator")
    run.add_argument("--smoke-test", action="store_true")
    for name in ("deploy", "monitor", "deploy-monitor"):
        item = sub.add_parser(name)
        item.add_argument(
            "--port", help="serial device, e.g. COM6 or /dev/cu.usbmodem..."
        )
    args = parser.parse_args()
    if args.command == "run":
        simulator(args.smoke_test)
        return
    tool = pio()
    if args.command == "build":
        call([tool, "run", "-e", "crowpanel_app"])
    if args.command in ("deploy", "deploy-monitor"):
        port = ["--upload-port", args.port] if args.port else []
        call([tool, "run", "-e", "crowpanel_app", "-t", "upload", *port])
    if args.command in ("monitor", "deploy-monitor"):
        port = ["--port", args.port] if args.port else []
        call([tool, "device", "monitor", "--baud", "115200", *port])


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode) from exc
