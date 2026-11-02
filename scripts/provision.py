#!/usr/bin/env python3
"""One-time ESP32-S3 HMAC key provisioning. This permanently burns an eFuse."""

import argparse
import importlib
import os
import pathlib
import secrets
import subprocess
import sys
import tempfile


def check_espefuse() -> None:
    """Check provisioning dependencies before communicating with the device."""
    try:
        importlib.import_module("espefuse")
    except ImportError as exc:
        requirements = (
            pathlib.Path(__file__).resolve().with_name("requirements-provision.txt")
        )
        raise SystemExit(
            f"Cannot load espefuse: {exc}\n"
            f'Install compatible dependencies with: "{sys.executable}" '
            f'-m pip install -r "{requirements}"'
        ) from exc


def main() -> None:
    """Inspect the device and explicitly confirm permanent key provisioning."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="serial port of the ESP32-S3")
    args = parser.parse_args()
    check_espefuse()
    tool = [sys.executable, "-m", "espefuse", "--chip", "esp32s3", "--port", args.port]
    print("Inspecting eFuses (requires the esptool Python package).", flush=True)
    subprocess.run(tool + ["summary"], check=True)
    print("\nWARNING: Burning BLOCK_KEY0 is permanent. The key will be read-protected.")
    print(
        "Confirm BLOCK_KEY0 is unused and this is the intended device before continuing."
    )
    print("A failed or interrupted burn may permanently consume the block.")
    if input("Type BURN KEY0 to continue: ").strip() != "BURN KEY0":
        print("Cancelled; no eFuse was written.")
        return
    fd, name = tempfile.mkstemp(prefix="transport-hmac-", suffix=".bin")
    try:
        with os.fdopen(fd, "wb") as file:
            file.write(secrets.token_bytes(32))
        # espefuse checks block availability and asks for its own confirmation;
        # never force an already-used block or disable read protection.
        subprocess.run(tool + ["burn_key", "BLOCK_KEY0", name, "HMAC_UP"], check=True)
    finally:
        pathlib.Path(name).unlink(missing_ok=True)
    print("Provisioned. Build/deploy the app normally; never share the temporary key.")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode) from exc
