# CrowPanel Advance 5.0 V1.1 three-part wall mount

The parametric build123d model keeps the factory front plate and four original
corner screws. The replacement mount is now three separate printable parts:

1. **Wall backplate:** a flat 2 mm plate with landscape and portrait keyholes.
   Its wireless-module opening clears both header rows and the board area
   between them so an extension board can fit in that region.
2. **Enclosure frame:** a perimeter body with the side openings, corner screw
   posts, a 2.5 mm wall-screw chamber, and a ledge that supports the middle
   plate. Its center stays open, so the screw chamber does not need trapped
   print supports.
3. **Middle plate:** a removable 1 mm PCB shield seated on the frame ledge. In
   the wireless area it has two separate openings for the header pin rows and
   remains solid between them. It also has 3.6 mm screw-shank holes aligned
   with the backplate and frame-post bores.

The four original frame screws clamp the stack together. Each screw head sits
in a 6.5 mm diameter, 1.1 mm deep counterbore in the backplate and bears on the
shoulder around its 3.6 mm shank hole. The frame posts start on the display
side of the middle plate, leaving that plate free to slide into the frame from
the wall side. From the wall toward the display, the assembled order is
backplate, screw-head chamber, middle plate, component clearance, PCB/display,
and the supplier front plate. The overall mount depth remains 11 mm.

The default OCP view shows the wall-facing side. Board access cutouts remain
reflected left-to-right across the case centerline to match your edited
graphic.

| Layer, from wall toward display | Thickness/depth |
| --- | ---: |
| Flat wall backplate | 2 mm |
| Wall-screw head chamber | 2.5 mm |
| Middle PCB shield | 1 mm |
| Component clearance to mating plane | 5.5 mm |

The wall screw heads sit in the chamber between the backplate and middle
plate. The middle plate shields the PCB from the screw heads. The wide keyhole
entries are below their resting ends after installation, so lowering the
display carries the screw shanks upward into the narrow seats.

The wall-facing outer plate remains flat around its openings. The wireless
extension-board clearance is a 25.0 mm wide through-opening centered between
the header rows and extended through the plate's upper edge. Its dimensions are
editable; check the width against the extension board you intend to use before
printing the full set.

| Display orientation | USB-C edge | Wall screw pair | Spacing |
| --- | --- | --- | ---: |
| Landscape | Right when viewed from the screen | Landscape keyholes | 60 mm |
| Portrait, rotate counterclockwise | Bottom | Portrait keyholes | 42 mm |

Both wall screw patterns are inside the 136.4 × 84.7 mm case outline. The slot
pair not used in a given orientation remains on the rear face.

## Source dimensions and assumptions

The PCB outline, screw holes, and rear connector coordinates come from
[Elecrow's V1.1 EAGLE board file](https://raw.githubusercontent.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/refs/heads/master/Eagle_SCH%26PCB/Version%201.1/ESP32%20Display%205.0%20inch%20V1.1.brd):

- PCB outline: 132.0 × 80.3 mm, 3.0 mm corner radius.
- Four 3.2 mm board holes at (3, 3), (129, 3), (3, 77.3), and (129, 77.3) mm.
- USB-C J1 at (124.4746, 40.15) mm.
- Rear-facing UART/I2C connectors, extension headers, BOOT/RESET buttons,
  battery and speaker connectors, microphone, buzzer, and the S1/S0 access
  opening use the same board coordinates. The 5 × 5 mm S1/S0 opening matches
  the original STEP housing at the K1 switch location.
- `MIRROR_BOARD_LAYOUT_X` reflects physical board access features across the
  case centerline.

Elecrow lists the display assembly as 136.4 × 84.7 × 15.4 mm in its
[product README](https://github.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/master/readme.md).
The side USB-C, debug, service, and TF-card openings follow the housing layout
in [Elecrow's 3D files](https://github.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/master/3D%20file).
The available assembly is V1.2; Elecrow notes minor housing changes and
shorter buttons, so verify these openings against the V1.1 unit. The outer
case radius, wall, screw seat, middle-plate ledge, and extension-board window
are first-fit values.

## Model parameters

Edit constants near the top of `wall_mount.py`:

- `BACKPLATE_THICKNESS`, `WALL_SCREW_HEAD_CHAMBER_DEPTH`,
  `PCB_SHIELD_THICKNESS`, and `PCB_COMPONENT_CLEARANCE` control the 11 mm
  stack depth.
- `WIRELESS_MODULE_OUTER_OPENING` sets the wider backplate window. The middle
  plate uses the two separate `WIRELESS-1` and `WIRELESS-2` connector windows
  from `REAR_CONNECTOR_OPENINGS`.
- `MIDDLE_PLATE_EDGE_CLEARANCE`, `MIDDLE_PLATE_LEDGE_WIDTH`, and
  `MIDDLE_PLATE_LEDGE_HEIGHT` set the fit and support ledge for the removable
  shield.
- `FRAME_POST_DIAMETER` and `FRAME_SCREW_CLEARANCE_DIAMETER` set the screw
  posts and shank passages. `FRAME_SCREW_HEAD_COUNTERBORE_DIAMETER` and
  `FRAME_SCREW_HEAD_COUNTERBORE_DEPTH` set the backplate head seat. The current
  nominal values are based on the supplier STEP screw geometry; check them
  against the physical screws and confirm their reach before printing the full
  set.
- `WALL_SCREW_SHANK_DIAMETER`, `WALL_SCREW_HEAD_DIAMETER`, clearances, slot
  travel, and the two spacing values set the wall keyholes.
- `MIRROR_BOARD_LAYOUT_X` controls the physical cutout reflection.
The wall screw envelope defaults to a 4.5 mm shank and 9.0 mm head. Use
masonry anchors and screws suited to the actual wall.

## Build, export, and view

Select the repository `.venv` interpreter in VS Code. From the repository root,
run:

```sh
.venv/bin/python CAD/wall_mount.py
```

The script validates and exports each printable part with its flat mating face
at Z=0, plus a three-part assembly STEP for CAD viewing:

- `CAD/exports/wall_backplate.stl` and `.step`
- `CAD/exports/enclosure_frame.stl` and `.step`
- `CAD/exports/middle_plate.stl` and `.step`
- `CAD/exports/wall_mount_assembly.step`

Import the three STL files individually into Bambu Studio to print the parts.
The assembly STEP is for viewing and CAD exchange; do not import it alongside
the three STL files for printing. The parts share nearly the same XY footprint,
so put them on separate plates or print jobs rather than arranging them
overlapping on one plate. The backplate and middle plate print flat. The
enclosure frame prints with its backplate mating edge on the build plate.

The script calls `show(assembly, reset_camera=Camera.BOTTOM)` when executed, so
the OCP viewer receives all three pieces in their assembled positions. Opening
the `.py` file alone does not execute it; run it to send the model to the
viewer.

## Assembly and fit check

1. Print the three parts in PETG. Check the extension-board window against the
   actual wireless/extension board before committing to a full print.
2. Hold the enclosure frame with its wall-facing opening accessible. Insert
   the middle plate from that side and slide it onto the internal ledge. Its
   four small screw holes align with the frame-post bores; the posts begin on
   the plate's display-facing side. Confirm the wireless pin openings align
   with the headers.
3. Place the backplate over the frame. The screw heads seat in its larger
   counterbores while the shanks pass through the smaller holes, middle plate,
   and frame posts. Fit the display PCB and factory front plate, then tighten
   the four original screws to clamp the stack.
4. Check that wall-screw heads fit in the chamber, the middle plate protects
   the PCB, and the USB-C plug, TF card, controls, and other connectors clear.
5. Drill two masonry holes 60 mm apart for landscape or 42 mm apart for
   portrait. Set screw heads to project far enough to enter the keyholes and
   seat in the 2.5 mm chamber. Hang through the wide entries, then lower until
   both shanks rise into the narrow seats.
6. Verify that the display settles securely under gravity and lifts off for
   removal.

These are first-fit prototypes. Confirm the V1.1 frame screw reach, middle
plate fit, wireless extension-board clearance, wall keyhole fit, connector
access, and cable clearance against the actual hardware before treating the
exports as final.
