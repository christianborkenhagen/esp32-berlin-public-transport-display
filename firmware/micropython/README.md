# MicroPython + LVGL prototype

This is a separate **Python application using LVGL**, not a different renderer.
It has two station tabs with simulated departures and eight benchmark scenes.
The native LVGL firmware and Windows simulator remain available.

## Status and requirements

The application and board adapter have host-side tests with simulated drivers.
The custom runtime image has been built in WSL, but the Python application
is uploaded separately and **has not yet been tested on the physical display**.
Do not use a stock MicroPython image: it does not include the required LVGL,
`lcd_bus.RGBBus` and `rgb_display` native drivers.

The target is a custom image of
`lvgl-micropython/lvgl_micropython` at commit
`d2d26467fa4cb9e99e569d899709043d086f7a6f`, with ESP32-S3, octal PSRAM,
16 MB flash and the `rgb_display` driver. The upstream ESP32 builder supports
Linux/macOS, **not native Windows**. WSL/Linux is therefore required to build
on a Windows machine. Follow the upstream prerequisites and do not initialize
its submodules separately; its builder fetches the versions it needs.

Example build in Linux/WSL (requires network and the upstream toolchain):

```sh
git clone https://github.com/lvgl-micropython/lvgl_micropython.git
cd lvgl_micropython
git checkout d2d26467fa4cb9e99e569d899709043d086f7a6f
python3 make.py esp32 BOARD=ESP32_GENERIC_S3 BOARD_VARIANT=SPIRAM_OCT DISPLAY=rgb_display --flash-size=16
```

On the current machine the completed build is in
`~/lvgl-micropython-esp32-build/build/lvgl_micropy_ESP32_GENERIC_S3-SPIRAM_OCT-16.bin`.
The Ubuntu build environment's CMake and Ninja are in the separate
`~/lvgl-micropython-esp32-build/.build-venv`; prepend its `bin` directory to
`PATH` before rebuilding. The project's Windows `.venv` remains separate.
If an Espressif component download returns HTTP 403, retry the build once
the registry is reachable.

The builder prints the merged `.bin` filename and the appropriate flash
command. **Flashing replaces the firmware and partition table and can erase
device data.** Save any device data first. This example has not been flashed
automatically; the currently installed firmware is left untouched.

After installing a compatible image, use the existing project `.venv` on
Windows for `mpremote`:

```powershell
uv pip install --python .venv/Scripts/python.exe mpremote
.\.venv\Scripts\python.exe -m mpremote connect COM6 cp firmware/micropython/model.py :model.py
.\.venv\Scripts\python.exe -m mpremote connect COM6 cp firmware/micropython/board.py :board.py
.\.venv\Scripts\python.exe -m mpremote connect COM6 cp firmware/micropython/ui.py :ui.py
.\.venv\Scripts\python.exe -m mpremote connect COM6 cp firmware/micropython/main.py :main.py
.\.venv\Scripts\python.exe -m mpremote connect COM6 reset
```

Use the actual serial port if it is not COM6. Reflashing the original C/LVGL
firmware is documented in the repository's main README.

## Board and measurement details

The adapter targets **CrowPanel Advance 5.0 V1.1 only**. It uses the established
800×480 RGB565 pinout and 16 MHz timing, the I²C backlight at `0x30`, and GT911
touch at `0x5d`. The upstream RGB driver uses its own PSRAM framebuffers and
partial LVGL draw buffers; its scanout/synchronization is not identical to the
native firmware's bounce-buffer implementation. Visual stability, colors and
touch still require physical verification.

Open **Benchmark** to cycle through eight 12-second scenes. The display shows
**LVGL last-flush submissions per second**, not panel scan rate or confirmed
presented frames. It is measured from the final LVGL `FLUSH_START` event in
each refresh. The copy/scanout may still be pending. If the binding does not
expose this hook, the UI reports the metric as unavailable instead of
fabricating a value. No CPU utilization is reported: this firmware has no
equivalent native per-core idle counter hook.

The static scenes do not update the status label, to avoid disturbing the
measurement; their values are printed to the serial console. Full redraw
invalidates the whole screen. The 33/25 ms values are requested timer periods,
not measured frame rates. The different MicroPython/LVGL rendering setup and
Python update overhead mean the results are not a controlled C-vs-Python
comparison.

Run the host tests from the project root:

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -p test_micropython.py -v
```

These tests use mocks and do not establish that the binding, build, display,
touch, or performance work on the target.
