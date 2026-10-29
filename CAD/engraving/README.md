# CrowPanel Advance 5.0 V1.1 rear markings

## Files

- `crowpanel-v1.1-rear-engraving.svg`: engraving artwork, black on transparent,
  with a **136.4 × 84.7 mm** canvas. Text is converted to paths, so no fonts
  are needed. There is no border, registration mark, or cutting outline.
- `crowpanel-v1.1-rear-engraving.png`: the same artwork, black on white,
  3222 × 2001 pixels at 600 dpi. Import at 136.4 × 84.7 mm; some software
  ignores the embedded resolution. Pixel rounding causes a <0.02 mm difference.
- `crowpanel-v1.1-registration-PRINT-ONLY.svg`: artwork with a blue nominal
  panel outline and four 3.2 mm board-hole circles. **Print only**, not for
  engraving. These are registration guides, not verified cutting geometry.

## Alignment and limitations

The orientation matches the supplied photograph: USB on the right, three
connectors along the top, and text read from the rear. The panel envelope is
136.4 × 84.7 mm. The four board-hole centres are at (5.2, 5.2),
(131.2, 5.2), (5.2, 79.5), and (131.2, 79.5) mm from the upper-left corner.

The *artwork* is a reconstruction: the photograph was registered to those
four hole centres and the text, icon, and table positions were estimated from
the image. Elecrow does not provide dimensions for the rear-cover markings
in the references used. Letterforms are approximations. Image perspective,
low resolution, and parallax limit accuracy; neither exact placement nor
clearance from openings is guaranteed. The ruler in the photograph was not
used as a precision measurement.

Print the registration guide at **100%**, verify its 136.4 × 84.7 mm
rectangle with a ruler, and check the hole centres and markings against your
actual panel or intended replacement plate before engraving. Adjust positions
if needed. Do not scale to fit. The rear panel is not the same as the
repository's reflected wall-mount layout: verify the orientation and the
openings on the particular surface you intend to engrave.

Assign all black paths and strokes to **engraving**, not cutting. On the SVG,
the icon and table have stroked outlines; configure the laser software
accordingly. Test on scrap first. Do not laser an assembled panel, and verify
the material and ventilation requirements before processing.

## Sources and regeneration

- [Elecrow V1.1 documentation](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_5.0-HMI_ESP32_AI_Display.html):
  function-selection table and port naming.
- [Elecrow V1.1 EAGLE board file](https://raw.githubusercontent.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/refs/heads/master/Eagle_SCH%26PCB/Version%201.1/ESP32%20Display%205.0%20inch%20V1.1.brd):
  board hole positions and UART0 pin labels.
- Panel envelope and coordinate mapping are also documented in `../README.md`.
- `create_engraving.py` generates the SVG files; it requires NumPy, fontTools,
  and the Windows Arial fonts. The PNG was rasterized separately at 600 dpi.
