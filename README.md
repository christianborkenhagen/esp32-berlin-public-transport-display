# Berlin Public Transport Display

PlatformIO / ESP-IDF project for the **Elecrow CrowPanel Advance 5.0 V1.1**
(PCB SKU **DIS02050A**, ESP32-S3-WROOM-1-N16R8).

## LVGL widgets demo

The firmware configures the board for 16 MB QIO flash and octal PSRAM, uses the
project partition table, turns on the V1.1 backlight controller, and initializes
the 800×480 RGB panel. It then starts LVGL 9.4.0's widgets demo using two
full-screen RGB565 framebuffers in PSRAM and the RGB driver's 20-line bounce
buffers in internal RAM. LVGL renders into the inactive framebuffer in direct
mode. After the last dirty area, a worker on the LCD interrupt core switches
framebuffers and waits for the next bounce-frame completion before LVGL reuses
the previous buffer. Sharing a core prevents a completion for the old frame
from racing with the switch. When the GT911
responds at I²C address `0x5D`, the firmware polls it for touch coordinates so
the demo controls can be used. The LVGL worker runs on core 1; touch is polled at
30 ms over 400 kHz I²C, with short transaction timeouts so a missing touch
response cannot stall the UI for a long period.

The default build compiles the LVGL component with `-O2` and places LVGL's marked
performance-sensitive functions in internal instruction RAM; the rest of the
firmware keeps the configured ESP-IDF compiler optimization level. This uses
internal RAM and may make LVGL code harder to step through in a debugger.
LVGL uses two FreeRTOS software-rendering workers. On this ESP-IDF Xtensa
port `StackType_t` is one byte, so the default configuration allocates
32 KiB of stack per worker.
An additional 48 KiB LVGL allocation pool in PSRAM supplements the 64 KiB
internal pool for temporary rendering allocations. A startup message reports
whether the extra pool was added successfully.

The demo's Profile avatar is precomposited with LVGL onto its solid panel
background as RGB565 in PSRAM, avoiding repeated alpha blending while scrolling.
The original ARGB8888 image remains available; the firmware falls back to it
for selected incompatible background, transform, geometry, opacity, or
recolor changes. The RGB565 buffer costs about 48 KiB of PSRAM.

ESP-IDF Component Manager downloads LVGL from the manifest in
`src/idf_component.yml` on the first build. The widget demo, its fonts, and
LVGL's separate examples are enabled in `sdkconfig.defaults`. The firmware
still starts the widgets demo; enabling the examples makes them available for
use but does not run one automatically.

## Transport-screen prototype

Send `t` in the serial monitor to open a simple, scrollable departures screen.
It uses **fictional, simulated data**, not a transport API: one displayed
departure time changes every five seconds while the screen is active. The
screen uses opaque backgrounds, simple shapes, and small individual label
updates. Use its **Widgets** button or send `d` to return to the widgets demo.
The prototype is released when leaving it; the existing panel and framebuffer
synchronization are unchanged.

## Build, upload, and monitor

From this directory:

```powershell
pio run
pio run -t upload
pio device monitor
```

If more than one serial device is present, select the board's port with
`pio run -t upload --upload-port COMx` and
`pio device monitor --port COMx`.

Use the board's programming USB connection for upload. The display should show
LVGL's Profile, Analytics, and Shop tabs. Touch the screen to explore the
widgets. The serial monitor reports whether the touch controller and backlight
controller responded over I²C.

For a repeatable performance comparison, send `b` through the serial monitor
and don't touch the display until the benchmark finishes. It selects Profile
and performs 40 fixed scrolling steps down and back up, logging the scroll
range, elapsed time, and average refresh timings. Compare runs with the same
scroll range; this synthetic test complements rather than replaces a visual
check for smoothness and flicker. Alternatively, with the monitor closed, run
`python tools/run_scroll_benchmark.py COMx` using an environment that has
`pyserial` installed (such as PlatformIO's Python environment).
Optional `--tiles 2|4`, `--avatar normal|hidden`,
`--avatar-format original|rgb565`, and
`--effects normal|simple` arguments select A/B test settings before the run.
Use `--screen transport` (or send `c` in the monitor) to run the same 40-step
test on the prototype; its simulated updates pause during the test. Compare
repeated runs of the *same* screen and scroll range: Profile and the prototype
have different content, dimensions, and ranges, so their times are not a
controlled widget-for-widget comparison.
Hiding the Profile avatar preserves its layout; simple effects override
main-part theme radius and shadow on Profile objects (local styles and other
parts can still take precedence). These settings persist until changed or the
board resets. Restore the normal appearance with `--avatar normal`,
`--avatar-format rgb565`, and `--effects normal`. Always compare results with
identical scroll ranges.

For a touch-responsiveness experiment, send `i` for a 10 ms polling period or
`j` to restore the 30 ms default. The script also accepts `--touch 10|30`.
The diagnostics report observed intervals between reads; these include delays
when the LVGL task is busy and are **not** input-to-photon latency. Compare
manual touch behavior as well as the logs; faster polling does not guarantee
faster rendering.

The separate `crowpanel_advance_5_v1_1_o3` PlatformIO environment builds the
LVGL component at `-O3` instead of `-O2`. For example:

```powershell
pio run -e crowpanel_advance_5_v1_1_o3
pio run -e crowpanel_advance_5_v1_1_o3 -t upload --upload-port COM6
```

The regular environment remains `-O2`; the startup log identifies the chosen
level. The `-O3` environment uses `sdkconfig.o3.defaults` to set each worker
stack to 16 KiB on a fresh configuration. The larger `-O3` instruction-RAM
image leaves insufficient internal memory for both 32 KiB worker stacks on
this board; a missing worker can make the first screen hang.

Generated `sdkconfig` files take precedence over defaults. If an existing
`-O3` configuration has the old 32 KiB setting, set
`CONFIG_LV_DRAW_THREAD_STACK_SIZE=16384` using
`pio run -e crowpanel_advance_5_v1_1_o3 -t menuconfig`. The build rejects the
known-problematic 32 KiB-or-larger setting in this environment.

For a comparison, keep the other settings equivalent, run the same benchmark
several times, and check memory use, touch behavior, tab switching, and flicker.
The stack sizes differ, so this is not a pure compiler-flag comparison.
On the transport scroll benchmark, the tested `-O3` build was not
meaningfully faster than `-O2`; `-O3` remains experimental.

## VS Code note

PlatformIO builds this ESP-IDF project using CMake internally. The root
`CMakeLists.txt` is required by that build, but the separate CMake Tools
extension may also try to configure it and ask you to select a compiler kit.
That extra kit is not needed when building through PlatformIO.

The workspace disables CMake Tools' automatic configure-on-open and
configure-on-edit behavior in `.vscode/settings.json`. Use the PlatformIO
**Build** command for builds. PlatformIO also generates
`.vscode/c_cpp_properties.json` for IntelliSense; avoid editing that generated
file directly.

## CAD design setup

For CAD work with build123d in VS Code:

1. Create a Python virtual environment for OCP with `uv venv --python 3.14.6`.
2. Install the `ocp_vscode` library from the VS Code Command Palette.
3. Install pip in the environment with `uv run python -m ensurepip`, then
   install build123d from its GitHub repository:

   ```sh
   python3 -m pip install git+https://github.com/gumyr/build123d
   ```

### build123d MCP server for Codex

The MCP server runs as a local process that Codex starts when needed. It has
its own uv-managed environment, separate from the project's `.venv`. Verify it
can start, then register it in Codex's user configuration:

```sh
uv tool run --python 3.14.6 build123d-mcp@latest --version
codex mcp add build123d -- uv tool run --python 3.14.6 build123d-mcp@latest
codex mcp list
```

Codex CLI and the Codex VS Code extension share this MCP configuration. Reload
or restart VS Code after adding the server so the extension picks it up. If
Codex cannot find `uv`, use its absolute executable path in the `codex mcp add`
command (find it with `command -v uv`). In a new Codex session, verify the
connection by asking it to use the build123d MCP tools to create and render a
20 mm cube, then report its dimensions.

## Hardware notes

- Display: 5-inch, 800×480 IPS panel with an ST7262 driver, connected over a
  16-bit RGB565 bus. The RGB signals are wired to the ESP32-S3 as follows:
  PCLK GPIO 39, HSYNC GPIO 40, VSYNC GPIO 41, DE GPIO 42; B0–B4 GPIOs
  21/47/48/45/38, G0–G5 GPIOs 9/10/11/12/13/14, and R0–R4 GPIOs
  7/17/18/3/46.
- Touch and backlight share I²C: SDA GPIO 15 and SCL GPIO 16. Touch is a
  capacitive, single-touch GT911 at address `0x5D`. On V1.1, backlight control
  is handled by the STC8H1K28 at address `0x30`.
- The configured pixel clock is 16 MHz, matching Elecrow's V1.1 ESP-IDF RGB
  example, including its active-negative PCLK setting. With the current porches,
  the raster rate is `16,000,000 / ((800 + 4 + 8 + 8) × (480 + 4 + 8 + 8))`,
  or about 39.0 frames per second. This
  is the panel scan rate; LVGL can render fewer frames if widget drawing becomes
  the limit. The ST7262 datasheet's generic RGB timing table lists 23–27 MHz,
  while Elecrow's board-specific V1.1 ESP-IDF example configures 16 MHz.
- The RGB panel uses two 750 KiB framebuffers in PSRAM and two 20-line ESP-IDF
  bounce buffers in internal RAM. LVGL renders dirty areas into the inactive
  framebuffer; the RGB driver copies the displayed framebuffer into bounce
  buffers for DMA scanout. At 39.0 fps, active 16-bit pixels amount to about
  30 MB/s of framebuffer reads. The ESP-IDF driver is configured to restart
  scanout automatically when it detects a missed bounce-buffer completion,
  rather than unconditionally restarting at every VSYNC. A delayed restart can
  itself cause a transient shift.
- The CPU runs at 240 MHz and the data cache uses 64-byte lines, matching
  Elecrow's V1.1 example. Espressif recommends 64-byte cache lines and larger
  bounce buffers in this mode to reduce RGB scanout drift. Every five seconds,
  serial diagnostics report VSYNC events, completed bounce frames, and LVGL
  updates. For intervals with updates, they also report average per-update
  preparation (including layout and synchronization of dirty areas between
  buffers), drawing/other work, framebuffer-switch wait, and total refresh
  time, plus the maximum refresh time. The draw/other figure includes refresh
  bookkeeping and excludes the measured switch wait; it is not a pure CPU
  drawing benchmark. Diagnostics also report free internal heap and the
  largest available internal block, to help assess the RAM cost of IRAM code
  and bounce buffers. These counters can help identify bottlenecks or missed
  frame completions but do not prove the image is free of visual artifacts.
- Area diagnostics sum the clipped regions passed to LVGL's flush callback
  and report their average size/count per update and the largest rectangle
  during the interval. This is a coarse measure of redraw work, not a
  per-widget time profile; overlapping regions can be counted more than once.
- The double-buffer approach follows the V1.1
  [Elecrow LVGL reference](https://github.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/master/example/V1.1/ESP_IDF/Lesson03-LVGL_Control_LED)
  and Espressif's
  [LVGL RGB display port](https://github.com/espressif/esp-bsp/blob/bba9438/components/esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c).
  Tearing during updates, RGB data starvation, panel timing, and backlight
  behavior are distinct possible causes of apparent flicker; verify changes
  on the physical display rather than assuming they have the same cause.
- The GPIO map and V1.1 I²C controller details follow [Elecrow's board
  documentation](https://elecrow.com/wiki/CrowPanel_Advance_5.0-HMI_ESP32_AI_Display.html).
  The pixel clock range follows the [ST7262 datasheet](https://www.orientdisplay.com/wp-content/uploads/2020/11/ST7262.pdf).
  Confirm signal stability on the actual board when changing timing or clock
  settings.
- Do not commit Wi-Fi credentials; they are not needed for this hardware test.
