# Four-orientation acrylic-backplate wall mount

This is a **prototype**, not a verified load-bearing mount. It keeps the
CrowPanel Advance 5.0 acrylic frame and uses its four additional backplate
holes, rather than the corner PCB/frame screws.

## Attachment and safety

The user confirmed threaded inserts behind the four additional M3 holes. The
inserts are not shown in the supplier STEP. Before assembly, measure their
depth and choose M3 screws that engage securely without bottoming out or
reaching the PCB. Do not overtighten the acrylic.

The mounting mechanism is gravity-retained, **not positively locked**. Lifting
the display can release it. Do not use it where accidental lifting, vibration
or a fall could cause injury. Assess the wall, fixings, print strength and
actual device clearance yourself; a physical fit/load test is essential.

## Design

The mount consists of two printed parts:

1. **Acrylic carrier:** a continuous 3 mm backing sheet with a raised
   perimeter and pads, four mounting bosses, and eight individual one-way
   keyholes, two for each orientation. Rear ports, buttons and the
   wireless-module area have clearance openings. The factory corner screws
   are covered; shallow reliefs on the acrylic-facing side clear their heads.
   Four 3.4 mm clearance holes and 6.6 mm diameter, 2.2 mm deep head recesses are
   provided for compact M3 screws. Each boss stands 1.0 mm above the rest of
   the carrier.
2. **Wall plate:** two integral, tapered studs and two recessed wall-screw
   holes, approximately 28.2 mm apart. The holes are slightly offset
   horizontally, so use the plate as a marking template. They clear 4.8 mm
   shanks; the head recesses are 9.0 mm in diameter and 2.0 mm deep. Select
   suitable wall fixings and check that their heads fit flush.

The nominal gap from the raised perimeter and pads to the flat acrylic face is
**1 mm**; the recessed sheet is farther away and only the four bosses touch
the **engraved outside** of the rear acrylic, away from the PCB and display.
Check the protrusion of your BOOT/RESET buttons and other features
against the real part. From the wall to the acrylic's nominal outer face is
10.85 mm. The hidden corner reliefs assume screw heads up to 6.2 mm in
diameter, projecting no more than 1.1 mm from the acrylic; measure the actual
screws before assembly.

The wall plate stays in the same position for all four orientations. Rotate
the display, align the two studs with the two large entry holes **below** their
corresponding narrow ends, push toward the wall and lower the display into the
ends of the slots. Lift to remove. These are separate keyholes rather than
cross-shaped slots, so the studs have a gravity stop in every orientation.
The tapered heads favour support-free printing but permit some movement away
from the wall before bearing on the retaining surface.

The carrier leaves openings for the modelled connectors and wireless headers,
and the S1/S0 opening has additional clearance. Check the actual connectors,
cables and any extension board: the model does not include every possible plug
or accessory.

## Notebook and exports

Open `four_orientation_wall_mount.ipynb` in VS Code, select the project Python
environment containing `build123d` and `ocp_vscode`, and start OCP CAD Viewer
from its sidebar. Run the cells in order. They show the complete supplier
assembly, carrier blank, finished carrier, wall plate and all four assembled
orientations. The final views separately identify the rear acrylic, middle
frame, PCB and components, display, front acrylic and original screws, so
individual parts can be hidden in the viewer tree. Edit dimensions near the
top and rerun all cells. The last cell exports:

- `exports/acrylic_carrier.stl` and `.step`
- `exports/wall_plate.stl` and `.step`
- `exports/mount_assembly.step` — a positioning preview, not a print file
- `exports/complete_display_assembly.step` — supplier device and mount
  together, also a preview rather than a print file

Print the STLs separately, already oriented as exported: carrier flat with its
1 mm bosses upwards; wall plate flat with the studs upwards. Check the stud
ramps and keyhole tolerances in your slicer. Test-fit the parts before
attaching anything to the display or wall; printer and material tolerances
may require adjustment. The model uses 0.6 mm nominal diametral clearance for
both the stud neck and head.

The notebook checks physical layer order, intersection with the original
supplier parts, supplier hole coordinates, model validity, collision at sampled
insertion and sliding positions in all orientations, each stud's gravity stop
and head retention, and main connector cutout envelopes for both parts. These
checks are geometric only: they do not verify physical fit, strength, screw
engagement, button protrusion, or wall safety.

The notebook is the source of truth for the design; edit it directly.

## Reference geometry

`reference/elecrow_v12_original_assembly.step` is Elecrow's
`advance-hmi5_0-20260116.stp`. The original
`reference/elecrow_v12_acrylic_backplate.step` is its extracted `5_0BOTTOM`
solid:

https://github.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/master/3D%20file

The hole centres in the original STEP's XY coordinates are `(3.5, 65.15)`,
`(128.5, 65.15)`, `(128.5, 15.15)` and `(17.334, 27.192)` mm. The asymmetric
last position is intentional. For the assembled view, the supplier model is
rotated 180° about Y, preserving its physical handedness; the two printed
parts are reflected in X so that they fit the real device. The supplier STEP
does not include the four M3 mounting screws or threaded inserts. The user
confirmed that the V1.1 and V1.2 acrylic backplates are the same; verify the
actual part before printing.
