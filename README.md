# Berlin Public Transport Display

PlatformIO / ESP-IDF project for the **Elecrow CrowPanel Advance 5.0 V1.1**
(PCB SKU **DIS02050A**, ESP32-S3-WROOM-1-N16R8).

## First test application

The initial firmware:

- configures the board for 16 MB QIO flash and octal PSRAM;
- uses the project partition table, with a large application partition and a
  separate SPIFFS data partition;
- probes the touch controller at I²C address `0x5D`;
- attempts to set the V1.1 backlight controller at `0x30` to maximum brightness;
- initializes the 800×480 RGB panel and draws color bars plus a black/white footer.

After flashing, the test should validate basic panel and backlight wiring. The
touch check only confirms that the controller responds over I²C; it does not yet
read touch coordinates. The firmware has been compile-tested but not yet tested
on the physical board.

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

Use the board's programming USB connection for upload. The RGB test pattern
should show eight vertical color bars and a black/white strip at the bottom.
The serial monitor reports whether I²C devices were detected.

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

## Hardware notes

- Panel: 800×480 RGB, ST7262.
- I²C bus: SDA GPIO 15, SCL GPIO 16.
- Touch: capacitive GT911, I²C address `0x5D`.
- Backlight: V1.1 controller on I²C address `0x30`.
- The pin map and backlight command are based on Elecrow's V1.1 example and
  should still be confirmed by flashing and testing the actual board.
- Do not commit Wi-Fi credentials; they are not needed for this hardware test.
