#!/usr/bin/env python3
"""Compile and execute host-side C tests using GCC and ESP-IDF cJSON."""

import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main() -> None:
    """Build and run parser, settings, time, storage and PSRAM pool tests."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cjson-dir", type=pathlib.Path, help="ESP-IDF components/json/cJSON directory"
    )
    args = parser.parse_args()
    compiler = shutil.which("gcc")
    if not compiler:
        raise SystemExit("GCC is required on PATH")
    if args.cjson_dir is None:
        raise SystemExit("Supply --cjson-dir pointing to ESP-IDF components/json/cJSON")
    cjson_dir: pathlib.Path = args.cjson_dir
    build = ROOT / ".pio" / "host-tests"
    build.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if sys.platform == "win32" else ""
    common = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Isrc/departures",
        "-Isrc/settings",
        "-Isrc/platform",
    ]
    tests = [
        (
            "departures",
            [
                "tests/test_departures_parser.c",
                "src/departures/departures_parser.c",
                "src/departures/departures_merge.c",
                str(cjson_dir / "cJSON.c"),
            ],
            ["-DCJSON_NESTING_LIMIT=32", f"-I{cjson_dir}"],
        ),
        (
            "settings",
            [
                "tests/test_settings.c",
                "src/settings/app_settings_common.c",
                "src/settings/app_settings_desktop.c",
                "desktop/lvgl/mock_service.c",
            ],
            [],
        ),
        (
            "update_time",
            ["tests/test_update_time.c", "src/departures/update_time.c"],
            [],
        ),
    ]
    for name, sources, flags in tests:
        exe = build / f"{name}{suffix}"
        subprocess.run(
            common + flags + sources + ["-o", str(exe)], cwd=ROOT, check=True
        )
        subprocess.run([str(exe)], cwd=ROOT, check=True)
    storage = build / f"storage{suffix}"
    subprocess.run(
        common
        + [
            "-Itests/fakes",
            "tests/test_storage_esp.c",
            "src/settings/app_settings_common.c",
            "src/settings/app_settings_esp.c",
            "-o",
            str(storage),
        ],
        cwd=ROOT,
        check=True,
    )
    for case in (
        "missing-key",
        "bad-key",
        "bad-partition",
        "corrupt",
        "open-fails",
        "write-fails",
        "commit-fails",
        "success",
    ):
        subprocess.run([str(storage), case], cwd=ROOT, check=True)
    pool = build / f"lvgl_pool{suffix}"
    subprocess.run(
        common
        + [
            "-Itests/fakes",
            "tests/test_lvgl_psram_pool.c",
            "src/platform/lvgl_psram_pool.c",
            "-o",
            str(pool),
        ],
        cwd=ROOT,
        check=True,
    )
    for case in ("fail", "success"):
        subprocess.run([str(pool), case], cwd=ROOT, check=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode) from exc
