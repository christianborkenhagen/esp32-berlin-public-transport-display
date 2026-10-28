# Berlin Public Transport Display

PlatformIO / ESP-IDF project for the **Elecrow CrowPanel Advance 5.0 V1.1**
(PCB SKU **DIS02050A**, ESP32-S3-WROOM-1-N16R8).

## LVGL widgets demo

The firmware configures the board for 16 MB QIO flash and octal PSRAM, uses the
project partition table, turns on the V1.1 backlight controller, and initializes
the 800×480 RGB panel. It then starts LVGL 9.4.0's widgets demo using one
full-screen RGB565 framebuffer in PSRAM, two 20-line LVGL draw buffers in
internal RAM, and the RGB driver's 10-line bounce buffers. LVGL renders partial
areas and the RGB driver copies them into the framebuffer. When the GT911
responds at I²C address `0x5D`, the firmware polls it for touch coordinates so
the demo controls can be used. The LVGL worker runs on core 1; touch is polled at
30 ms over 400 kHz I²C, with short transaction timeouts so a missing touch
response cannot stall the UI for a long period.

ESP-IDF Component Manager downloads LVGL from the manifest in
`src/idf_component.yml` on the first build. The widget demo and its fonts are
enabled in `sdkconfig.defaults`.

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
- The RGB panel uses one 750 KiB framebuffer in PSRAM, two 20-line LVGL draw
  buffers in internal RAM, and two 10-line ESP-IDF bounce buffers in internal
  RAM. LVGL renders dirty areas into the draw buffers; the driver copies them
  into the framebuffer, then copies framebuffer lines into the bounce buffers
  for DMA scanout. At 39.0 fps, active 16-bit pixels amount to about 30 MB/s of
  framebuffer reads. The ESP-IDF driver is configured to restart scanout at
  VSYNC if it detects a DMA desynchronization.
- The GPIO map and V1.1 I²C controller details follow [Elecrow's board
  documentation](https://elecrow.com/wiki/CrowPanel_Advance_5.0-HMI_ESP32_AI_Display.html).
  The pixel clock range follows the [ST7262 datasheet](https://www.orientdisplay.com/wp-content/uploads/2020/11/ST7262.pdf).
  Confirm signal stability on the actual board when changing timing or clock
  settings.
- Do not commit Wi-Fi credentials; they are not needed for this hardware test.
