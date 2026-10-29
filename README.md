# Berlin Public Transport Display

PlatformIO / ESP-IDF project for the **Elecrow CrowPanel Advance 5.0 V1.1**
(PCB SKU **DIS02050A**, ESP32-S3-WROOM-1-N16R8).

## Animation benchmark

The firmware starts a continuously looping benchmark with eight 12-second scenes:
a speedometer, moving blocks, animated arcs, translucent layers, and an
automatically scrolling list, followed by three static comparison scenes. Each
eight-scene cycle alternates the LVGL display
refresh and animation timer periods between 33 ms and 25 ms; the active period
is shown on screen and logged. Animation durations and the 30 ms touch polling
configuration are unchanged. It does not start the widgets demo or transport
prototype. Leave the screen untouched when comparing runs. Each scene and its
one-second FPS/CPU samples are also reported on the serial console.

To compare refresh periods with the same firmware, use the 1000 Hz build below
and capture at least two full cycles (about three minutes and 15 seconds),
preferably more.
Compare the same scenes and discard samples near scene or period changes:
their one-second measurement windows may include the previous scene or period.
The 25 ms setting is a timer period, not a promise of 40 FPS; panel scanout
and rendering remain independent limits. Confirm both modes are visually stable.

The three static comparison scenes use the same basic layout:

- **Static idle:** no periodic changes or redraws after the initial scene draw.
- **Static 1 pixel:** a 1×1 object changes color at the selected timer period.
- **Static full redraw:** the entire screen is invalidated at the same timer
  period, forcing a redraw of the static content without changing its appearance.

These tests keep the on-screen metrics label fixed, so it does not add dirty
regions. Live FPS and CPU estimates are available on the serial console. The
five-second diagnostics report the average and largest LVGL redraw regions;
use windows wholly within a scene to check that they are approximately 1 and
384,000 pixels, respectively. Transition windows can include the previous scene.
The redraw timer and display refresh timer are separate, so updates may be
coalesced or delayed; the selected period is not a guaranteed FPS.

After the idle transition, presented FPS should approach zero because nothing
is being presented, even though the panel continues scanning at approximately
39 Hz. This idle test does not measure maximum rendering throughput. All three
tests still use the same two full-screen framebuffers and synchronized switching;
the one-pixel test is a partial LVGL redraw, not a change to panel scanout.
Avoid touching the screen during the test. Per-core idle-counter CPU estimates
can be especially noisy if an idle task is not switched out during a sample.

The display uses two full-screen RGB565 framebuffers in PSRAM and the RGB
driver's 20-line internal-RAM bounce buffers. LVGL renders into the inactive
framebuffer. After the last dirty area, a task on the LCD interrupt core switches
framebuffers and waits for bounce-frame completion before LVGL reuses the old
buffer. This synchronization is unchanged from the previous test firmware.

The overlay reports:

- **FPS:** completed framebuffer switches per second over the last actual
  sampling interval, not the panel's scan rate. Updating the overlay itself
  causes a small amount of drawing and can contribute to the count.
- **CPU:** approximate average busy percentage across both cores, calculated
  from the change in each FreeRTOS idle task's runtime. This is not the LVGL
  task's CPU usage alone. Interrupt time can be attributed to the interrupted
  task, and sampling and runtime-statistics overhead affect the estimate.

The serial monitor additionally reports refresh timing, redraw area, scanout
counters, and internal heap every five seconds. These measurements do not
prove the display is flicker-free; check the physical screen too. Scene changes
and the on-screen metrics update can be included in an adjacent sample.

The per-scene serial samples include estimated busy percentages for each
core as well as their average. The five-second diagnostics also report:

- **Present interval:** time between completed framebuffer switches.
- **Between presented refreshes:** time from the end of one LVGL refresh that
  presented a frame to the start of the next such refresh. This includes task
  delays, other LVGL work, diagnostics, and any refreshes without a presentation;
  it is not exclusively scheduler time.
- **LVGL task delay:** requested delay after conversion to FreeRTOS ticks, and
  the actual elapsed time until the task resumes. Tick phase and other tasks can
  affect the observed delay.

These are wall-clock measurements, not CPU execution times. Interval and gap
samples can cross reporting windows or scene boundaries. Idle-task counters are
updated on context switches, so short per-core samples can be noisy; interrupt
attribution also affects them.

The default build compiles LVGL at `-O2`, uses two FreeRTOS drawing workers
with 32 KiB stacks, and adds a 48 KiB PSRAM pool to LVGL's 64 KiB internal
pool. LVGL examples are disabled in the defaults; the separate widgets demo
build setting remains available, although the firmware does not start it.
Touch is initialized and polled when the GT911 is available, but no
touch is needed for the benchmark. The transport prototype and old scroll
benchmark scripts remain in the repository as historical code; their serial
commands do not run on this firmware.

## Build, upload, and monitor

From this directory:

```powershell
pio run
pio run -t upload --upload-port COM6
pio device monitor --port COM6
```

Replace `COM6` with the board's port if necessary. The benchmark starts
automatically and cycles through its scenes indefinitely. Close the serial
monitor before uploading. To test the metric calculations on a host with GCC:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_benchmark_metrics.c src/benchmark_metrics.c -o .pio/benchmark_metrics_test.exe
.\.pio\benchmark_metrics_test.exe
```

A fresh ESP-IDF configuration enables FreeRTOS runtime statistics using the
ESP timer in `sdkconfig.defaults`. Existing generated `sdkconfig` files take
precedence over defaults. If upgrading an existing checkout, use
`pio run -t menuconfig` to enable `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`,
select the ESP timer as the runtime statistics clock, and use the 32-bit
counter type. Repeat for the `-O3` environment if using it. The build fails
explicitly if the required statistics are disabled.
Existing configurations may also retain `CONFIG_LV_BUILD_EXAMPLES=y`; disable
it in each environment's menuconfig to stop building the examples.

### Scheduler tick A/B test

The default `crowpanel_advance_5_v1_1` environment uses a 100 Hz FreeRTOS
tick; `crowpanel_advance_5_v1_1_tick1000` uses 1000 Hz. Both use the same
alternating LVGL timer periods and synchronized framebuffer-switching code. A higher tick
rate can add overhead; it is not assumed to improve performance or visual
stability.

Flash each build in turn, closing the monitor before each upload:

```powershell
pio run -e crowpanel_advance_5_v1_1 -t upload --upload-port COM6
pio device monitor --port COM6
# Close the monitor, then:
pio run -e crowpanel_advance_5_v1_1_tick1000 -t upload --upload-port COM6
pio device monitor --port COM6
```

For each run, leave the display untouched and capture at least two complete
eight-scene cycles (about three minutes and 15 seconds). Confirm the startup `Timing` line reports
the intended tick rate and note any flickering or instability. Compare the
same scene and period's FPS, per-core CPU, refresh timings and cadence, preferably
over multiple cycles rather than at scene transitions. Replace `COM6` if needed.
To restore the baseline, flash the first environment again.

Each environment has its own generated `sdkconfig`; existing files take
precedence over defaults. The 1000 Hz build fails if its generated config does
not actually specify 1000 Hz. For a controlled comparison, keep other settings
the same between the two configs.

The optional `crowpanel_advance_5_v1_1_o3` environment builds LVGL at `-O3`
with 16 KiB worker stacks for a fresh configuration. Existing `-O3`
configurations using 32 KiB stacks must be changed to 16 KiB in menuconfig;
the larger `-O3` IRAM image can otherwise prevent a worker from starting.
This build remains experimental; earlier transport-screen tests found no
meaningful improvement over `-O2`.
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
