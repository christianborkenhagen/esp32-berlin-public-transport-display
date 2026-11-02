# Berlin Public Transport Display

ESP-IDF / PlatformIO firmware for the Elecrow CrowPanel Advance 5.0 V1.1
(ESP32-S3, 800 × 480). The app shows chronologically ordered BVG departures,
with one touch-selectable tab per station and up to 32 departures per station.
It uses LVGL 9.4 and the community `v6.bvg.transport.rest` API.

## Prerequisites

- Python 3.10 or newer, PlatformIO Core (`pio`), CMake, Git, and a C/C++ toolchain.
- Windows simulator: MSYS2 MinGW-w64 GCC and Ninja (the scripts look for
  `C:\msys64\mingw64\bin`).
- macOS simulator: Xcode command-line tools and SDL2 (`brew install sdl2 cmake`).
- First builds may download PlatformIO packages or LVGL.

Run these commands from the project directory on either OS:

```sh
python scripts/app.py build
python scripts/app.py run                 # desktop simulator, mock data only
python scripts/app.py run --smoke-test
python scripts/app.py deploy --port COM6  # on macOS, use /dev/cu.usbmodem…
python scripts/app.py monitor --port COM6
python scripts/app.py deploy-monitor --port COM6
```

The port argument may be omitted for PlatformIO auto-detection. Exit the
serial monitor before deploying. On Windows use `py` if `python` is unavailable.
The simulator's Wi-Fi, search, and settings are mocked and do not persist.

## Encrypted settings: one-time provisioning

The board has 16 MiB flash. The partition table preserves the default NVS and
SPIFFS locations and reserves 64 KiB at `0xF00000` for encrypted app settings.
The factory app partition is reduced by 64 KiB. Before deploying a new partition
table, back up any important flash contents. The application and scripts do not
explicitly erase the settings partition or automatically provision keys.
ESP-IDF NVS can modify flash during initialization, recovery, and normal writes,
including erasing individual pages.

**Provisioning irreversibly burns ESP32-S3 eFuse BLOCK_KEY0.** Confirm the
correct device and an unused block before continuing; an interrupted burn may
consume the block permanently. The script displays the eFuse summary, requires
`BURN KEY0`, and leaves `espefuse`'s own confirmation enabled. It refuses to
force a used block. Install the pinned provisioning dependencies in the same
Python environment as the script. In particular, `esptool` 4.11.0 cannot use
`bitstring` 5.0.0:

```sh
python -m pip install -r scripts/requirements-provision.txt
python scripts/provision.py --port COM6
```

The script generates a random key in a temporary host file, passes it to
Espressif's `espefuse` with `HMAC_UP` purpose and default read protection, and
deletes the file afterward. Host storage may retain recoverable remnants; use
a trusted machine and temporary directory. Never commit or share key material.
Build and deploy the app afterward. An unprovisioned or unusable key leaves
settings read-only and displays an error; there is no unencrypted fallback.

The app uses ESP-IDF encrypted NVS with keys derived from the HMAC peripheral.
The default NVS partition remains unencrypted and Wi-Fi uses RAM-only driver
storage; app credentials are stored only in the encrypted settings partition.
The firmware uses mbedTLS software AES for NVS and TLS to avoid hardware AES
DMA allocation failures under display and Wi-Fi memory pressure. AES-XTS remains
enabled for app settings, but TLS performance may differ. PlatformIO updates
the AES option in an existing generated `sdkconfig` on build; other settings
are preserved.
Secure boot and full flash encryption are **not** enabled: someone with
physical access who can replace firmware may be able to access stored secrets.
Old firmware images with previously compiled credentials remain sensitive.

### Settings storage troubleshooting

Provision KEY0 only once. If `espefuse` reports that BLOCK_KEY0 is
read-protected, do not force a write or attempt to burn it again. Protection
alone does not verify that the key has the required HMAC purpose.

`ESP_ERR_NVS_INVALID_STATE` indicates an NVS state problem; it does not by
itself identify a key problem or the original cause. Capture the complete boot
log and the **first** failed save, including earlier errors, using
`python scripts/app.py monitor --port COM6`. Redact credentials before sharing
logs. The firmware logs the operation that failed and disables further settings
writes for that session. A failed save may leave the on-flash state uncertain.
Do not erase the partition or reprovision to troubleshoot without first
considering a backup and the consequences; NVS can also perform recovery writes
when initialized after a reboot.
If the log instead shows `esp-aes: Failed to allocate memory` followed by
`ESP_ERR_NVS_XTS_ENCR_FAILED`, rebuild with the software AES configuration.
This firmware change does not provision the key or explicitly erase the
settings partition.

### Wi-Fi initialization troubleshooting

The firmware keeps LVGL's primary and extra memory pools in PSRAM to leave
internal RAM available for the network and DMA. If Wi-Fi initialization still
fails, the serial log identifies the failing step and reports internal and
DMA-capable heap availability. Capture the complete boot log for diagnosis;
the desktop simulator cannot reproduce hardware memory pressure.

## On the panel

Open **Settings** and use the **Wi-Fi** tab to scan for a 2.4 GHz network or
enter an SSID manually, enter the password, and save. Tap a text field to open
the on-screen keyboard; use its confirm key to dismiss it. On the **Stations**
tab, search for stations after Wi-Fi connects,
select a result, and add it. Select saved stations to remove or reorder them;
up to four are supported. The two original Berlin stations are the defaults.
Settings are saved immediately. Tabs show `~` for scheduled-only times and
label cancelled departures. Errors preserve the last results as stale and retry.
The header shows the time of the most recent successful station update in Berlin
local time; a partial update may still leave some stations stale.
API availability, response sizes, and freshness are controlled upstream.

## UI development

The 800 × 480 layout and its callbacks are written directly in
`src/ui/transport_screen.c`, shared by the firmware and desktop simulator.
UI changes require rebuilding and flashing the firmware; no editor or export
step is needed. Validate display stability on the physical panel.
The built-in LVGL font lacks German umlauts, so station and destination names
containing them may show placeholders. A font with extended Latin glyphs is
needed to display those names correctly.

Other source directories separate departure fetching and parsing (`departures/`),
settings and persistence (`settings/`), and hardware-specific LVGL memory support
(`platform/`). The panel entry point remains in `src/main.c`.

## Tests

```sh
python scripts/test.py --cjson-dir /path/to/esp-idf/components/json/cJSON
python -m unittest discover -s tests -p test_pio_sdkconfig.py
python scripts/app.py run --smoke-test
```

The C tests cover departure parsing and ordering, settings validation, the
PSRAM pool allocator, and the mock service. The simulator does not reproduce
panel scanout, touch hardware, PSRAM pressure, encrypted flash, or network/TLS behavior. On real hardware,
verify provisioning, power-cycle persistence, Wi-Fi and station setup, stale
and error behavior, display stability, and internal heap margins.
