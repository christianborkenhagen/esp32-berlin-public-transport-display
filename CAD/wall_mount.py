"""Parametric three-part replacement mount for the CrowPanel Advance 5.0 V1.1.

The board access layout is reflected across the case centerline to match the
user's edited default-view graphic. The mirrored USB-C opening is on the left
in the wall-facing view; rotating the display counterclockwise puts it at the
bottom. The script exports the backplate, enclosure frame, and middle plate
separately, along with an assembled STEP.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from build123d import (
    Align,
    Box,
    BuildPart,
    BuildSketch,
    Compound,
    Cylinder,
    Locations,
    Mode,
    Plane,
    Pos,
    RectangleRounded,
    Rot,
    Vector,
    export_step,
    export_stl,
    extrude,
    import_step,
    import_stl,
)


# Elecrow V1.1 EAGLE board: 132 x 80.3 mm with a 3 mm corner radius.
PCB_WIDTH = 132.0
PCB_HEIGHT = 80.3
PCB_CORNER_RADIUS = 3.0
PCB_TO_CASE_MARGIN_X = 2.2
PCB_TO_CASE_MARGIN_Y = 2.2

# Published envelope for the display assembly.
CASE_WIDTH = 136.4
CASE_HEIGHT = 84.7
CASE_CORNER_RADIUS = 5.0  # Estimate from the supplier assembly STEP and photos.

# Board hole positions from the V1.1 board file (origin at board bottom-left).
PCB_HOLES = (
    (3.0, 3.0),
    (129.0, 3.0),
    (3.0, 77.3),
    (129.0, 77.3),
)

# Layered wall-facing shell. The chamber is sized for a low-profile wall screw
# head; an uninterrupted shield separates the screw heads from the electronics.
BACKPLATE_THICKNESS = 2.0
WALL_SCREW_HEAD_HEIGHT = 2.2
WALL_SCREW_HEAD_CLEARANCE = 0.3
WALL_SCREW_HEAD_CHAMBER_DEPTH = WALL_SCREW_HEAD_HEIGHT + WALL_SCREW_HEAD_CLEARANCE
PCB_SHIELD_THICKNESS = 1.0
PCB_COMPONENT_CLEARANCE = (
    11.0 - BACKPLATE_THICKNESS - WALL_SCREW_HEAD_CHAMBER_DEPTH - PCB_SHIELD_THICKNESS
)
PCB_SHIELD_Z = BACKPLATE_THICKNESS + WALL_SCREW_HEAD_CHAMBER_DEPTH
PCB_SHIELD_INNER_Z = PCB_SHIELD_Z + PCB_SHIELD_THICKNESS
MOUNT_DEPTH = 11.0
SIDE_WALL_THICKNESS = 2.0

# The supplier STEP represents the original frame screws with an approximately
# 3 mm shank and 6.1 mm head. The backplate retains each head in a counterbore.
FRAME_POST_DIAMETER = 8.0
FRAME_SCREW_CLEARANCE_DIAMETER = 3.6
FRAME_SCREW_HEAD_DIAMETER = 6.1
FRAME_SCREW_HEAD_CLEARANCE = 0.4
FRAME_SCREW_HEAD_COUNTERBORE_DIAMETER = (
    FRAME_SCREW_HEAD_DIAMETER + FRAME_SCREW_HEAD_CLEARANCE
)
FRAME_SCREW_HEAD_HEIGHT = 1.0
FRAME_SCREW_HEAD_COUNTERBORE_DEPTH = 1.1

# Wall screw envelope for the keyholes in the outer backplate.
WALL_SCREW_SHANK_DIAMETER = 4.5
WALL_SCREW_HEAD_DIAMETER = 9.0
WALL_SHANK_CLEARANCE = 0.7
WALL_HEAD_CLEARANCE = 0.8
WALL_SLOT_NECK_WIDTH = WALL_SCREW_SHANK_DIAMETER + WALL_SHANK_CLEARANCE
WALL_SLOT_ENTRY_DIAMETER = WALL_SCREW_HEAD_DIAMETER + WALL_HEAD_CLEARANCE
WALL_SLOT_TRAVEL = 8.5
LANDSCAPE_WALL_SCREW_SPACING = 60.0
PORTRAIT_WALL_SCREW_SPACING = 42.0
PORTRAIT_WALL_SCREW_REST_X = 0.0

# Source package locations from the V1.1 EAGLE file. The rectangular windows
# include roughly 1 mm clearance around each connector package.
S1_S0_ACCESS_OPENING = (73.4394, 23.3802, 5.0, 5.0)
REAR_CONNECTOR_OPENINGS = (
    # Label, board x/y, opening width/height in board XY.
    ("UART0-OUT", 26.01, 71.82, 14.2, 8.9),
    ("UART1-OUT", 45.97, 71.82, 14.2, 8.9),
    ("I2C-OUT", 66.01, 71.82, 14.2, 8.9),
    # The two 1x18 extension headers are rotated R270 on the PCB.
    ("WIRELESS-1", 100.2322, 69.7638, 5.8, 20.3),
    ("WIRELESS-2", 115.8262, 69.7638, 5.8, 20.3),
    # The original STEP has a 5 x 5 mm access opening over the S1/S0 switch.
    ("S1-S0", *S1_S0_ACCESS_OPENING),
    ("UART0-IN", 114.557, 6.45, 17.0, 8.3),
    ("BAT", 37.551, 5.439, 10.0, 9.5),
    ("SPK", 38.54, 26.4, 10.0, 9.5),
)

BUTTON_HOLES = (
    ("BOOT", 4.2082, 33.6424),
    ("RESET", 4.2082, 27.1424),
)
BUTTON_ACCESS_DIAMETER = 3.8
MICROPHONE_HOLE = (3.55, 16.41)
MICROPHONE_HOLE_DIAMETER = 2.8
BUZZER_CENTER = (84.9314, 74.0448)
BUZZER_VENT_DIAMETER = 1.2

# The original 3D assembly has a bounded USB-C side opening, a left service
# window, and a bottom TF-card slot. Their XY intervals are read from the
# V1.2 STEP frame and mapped into the V1.1 case coordinates.
USB_C_SIDE_OPENING_WIDTH = 12.0
USB_C_SIDE_OPENING_HEIGHT = 5.0
USB_C_SIDE_OPENING_Z = 7.8
DEBUG_SIDE_OPENING_WIDTH = 9.0
DEBUG_SIDE_OPENING_HEIGHT = 4.5
DEBUG_SIDE_OPENING_Y = 24.09  # J15, the four-pin test header near the USB-C edge.
DEBUG_SIDE_OPENING_Z = 7.0
LEFT_SERVICE_OPENING_WIDTH = 21.0
LEFT_SERVICE_OPENING_HEIGHT = 5.0
LEFT_SERVICE_OPENING_Y = -10.38
LEFT_SERVICE_OPENING_Z = 7.0
TF_SLOT_OPENING_WIDTH = 22.0
TF_SLOT_OPENING_HEIGHT = 4.0
TF_SLOT_OPENING_X = -48.56
TF_SLOT_OPENING_Z = 6.5

# The outer wall plate clears the whole extension-board footprint and opens to
# the case's upper edge. The middle plate retains two separate header windows.
WIRELESS_MODULE_OUTER_OPENING = (108.0292, 69.7638, 25.0, 26.0)
MIDDLE_PLATE_EDGE_CLEARANCE = 0.3
MIDDLE_PLATE_LEDGE_WIDTH = 1.4
MIDDLE_PLATE_LEDGE_HEIGHT = 0.8

MOUNT_WIDTH = CASE_WIDTH
MOUNT_HEIGHT = CASE_HEIGHT
MIRROR_BOARD_LAYOUT_X = True
PORTRAIT_ROTATION_DEGREES = 90 if MIRROR_BOARD_LAYOUT_X else -90


def case_xy_from_board(board_x: float, board_y: float) -> tuple[float, float]:
    """Map board coordinates to case-centred XY, reflecting the layout in X."""
    x = board_x + PCB_TO_CASE_MARGIN_X - CASE_WIDTH / 2
    if MIRROR_BOARD_LAYOUT_X:
        x = -x
    return (
        x,
        board_y + PCB_TO_CASE_MARGIN_Y - CASE_HEIGHT / 2,
    )


def mirrored_side(axis: str) -> str:
    """Return the side-wall name after reflecting the board layout in X."""
    if not MIRROR_BOARD_LAYOUT_X:
        return axis
    return {"left": "right", "right": "left"}.get(axis, axis)


def screw_locations() -> tuple[tuple[float, float], ...]:
    return tuple(case_xy_from_board(x, y) for x, y in PCB_HOLES)


def cut_vertical_keyhole(x: float, rest_y: float) -> None:
    """Cut a vertical slot whose wide entry is below its gravity-rest point."""
    entry_y = rest_y - WALL_SLOT_TRAVEL
    cut_z = -1.0
    cut_depth = BACKPLATE_THICKNESS + 1.0
    with Locations((x, entry_y, cut_z)):
        Cylinder(
            WALL_SLOT_ENTRY_DIAMETER / 2,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )
    with Locations((x, (entry_y + rest_y) / 2, cut_z)):
        Box(
            WALL_SLOT_NECK_WIDTH,
            WALL_SLOT_TRAVEL,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )
    with Locations((x, rest_y, cut_z)):
        Cylinder(
            WALL_SLOT_NECK_WIDTH / 2,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )


def cut_horizontal_keyhole(rest_x: float, y: float) -> None:
    """Cut a portrait slot whose entry rotates below its gravity-rest point."""
    entry_direction = -1 if MIRROR_BOARD_LAYOUT_X else 1
    entry_x = rest_x + entry_direction * WALL_SLOT_TRAVEL
    cut_z = -1.0
    cut_depth = BACKPLATE_THICKNESS + 1.0
    with Locations((entry_x, y, cut_z)):
        Cylinder(
            WALL_SLOT_ENTRY_DIAMETER / 2,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )
    with Locations(((entry_x + rest_x) / 2, y, cut_z)):
        Box(
            WALL_SLOT_TRAVEL,
            WALL_SLOT_NECK_WIDTH,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )
    with Locations((rest_x, y, cut_z)):
        Cylinder(
            WALL_SLOT_NECK_WIDTH / 2,
            cut_depth,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )


def cut_rear_window(
    board_x: float,
    board_y: float,
    width: float,
    height: float,
    *,
    z_start: float,
    depth: float,
) -> None:
    """Cut one connector window through a selected flat layer."""
    x, y = case_xy_from_board(board_x, board_y)
    with Locations((x, y, z_start - 0.1)):
        Box(
            width,
            height,
            depth + 0.2,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )


def cut_access_pattern(
    *, z_start: float, depth: float, wide_wireless_opening: bool
) -> None:
    """Cut the access windows shared by the wall and middle plates."""
    for label, board_x, board_y, width, height in REAR_CONNECTOR_OPENINGS:
        if wide_wireless_opening and label.startswith("WIRELESS-"):
            continue
        cut_rear_window(
            board_x, board_y, width, height, z_start=z_start, depth=depth
        )

    if wide_wireless_opening:
        cut_rear_window(
            *WIRELESS_MODULE_OUTER_OPENING[:2],
            WIRELESS_MODULE_OUTER_OPENING[2],
            WIRELESS_MODULE_OUTER_OPENING[3],
            z_start=z_start,
            depth=depth,
        )

    for _label, board_x, board_y in BUTTON_HOLES:
        x, y = case_xy_from_board(board_x, board_y)
        with Locations((x, y, z_start - 0.1)):
            Cylinder(
                BUTTON_ACCESS_DIAMETER / 2,
                depth + 0.2,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
                mode=Mode.SUBTRACT,
            )

    mic_x, mic_y = case_xy_from_board(*MICROPHONE_HOLE)
    with Locations((mic_x, mic_y, z_start - 0.1)):
        Cylinder(
            MICROPHONE_HOLE_DIAMETER / 2,
            depth + 0.2,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
            mode=Mode.SUBTRACT,
        )

    buzzer_x, buzzer_y = case_xy_from_board(*BUZZER_CENTER)
    for dx, dy in ((-1.8, -1.0), (0.0, -1.0), (1.8, -1.0),
                   (-1.8, 1.0), (0.0, 1.0), (1.8, 1.0)):
        with Locations((buzzer_x + dx, buzzer_y + dy, z_start - 0.1)):
            Cylinder(
                BUZZER_VENT_DIAMETER / 2,
                depth + 0.2,
                align=(Align.CENTER, Align.CENTER, Align.MIN),
                mode=Mode.SUBTRACT,
            )


def cut_wall_window(
    *, axis: str, center: float, width: float, height: float, center_z: float
) -> None:
    """Cut a bounded rectangular window through one 2 mm side wall."""
    cutter_depth = SIDE_WALL_THICKNESS + 2.0
    if axis == "right":
        location = (CASE_WIDTH / 2 - SIDE_WALL_THICKNESS / 2, center, center_z)
        size = (cutter_depth, width, height)
    elif axis == "left":
        location = (-CASE_WIDTH / 2 + SIDE_WALL_THICKNESS / 2, center, center_z)
        size = (cutter_depth, width, height)
    elif axis == "top":
        location = (center, CASE_HEIGHT / 2 - SIDE_WALL_THICKNESS / 2, center_z)
        size = (width, cutter_depth, height)
    elif axis == "bottom":
        location = (center, -CASE_HEIGHT / 2 + SIDE_WALL_THICKNESS / 2, center_z)
        size = (width, cutter_depth, height)
    else:
        raise ValueError(f"Unknown wall axis: {axis}")
    with Locations(location):
        Box(*size, mode=Mode.SUBTRACT)


def build_backplate():
    """Build the separate 2 mm wall-facing plate and wall keyholes."""
    landscape_xs = (-LANDSCAPE_WALL_SCREW_SPACING / 2,
                    LANDSCAPE_WALL_SCREW_SPACING / 2)
    portrait_ys = (-PORTRAIT_WALL_SCREW_SPACING / 2,
                   PORTRAIT_WALL_SCREW_SPACING / 2)

    with BuildPart() as plate:
        with BuildSketch(Plane.XY):
            RectangleRounded(CASE_WIDTH, CASE_HEIGHT, CASE_CORNER_RADIUS)
        extrude(amount=BACKPLATE_THICKNESS)

        for x, y in screw_locations():
            with Locations((x, y, -0.1)):
                Cylinder(
                    FRAME_SCREW_CLEARANCE_DIAMETER / 2,
                    BACKPLATE_THICKNESS + 0.2,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                    mode=Mode.SUBTRACT,
                )
            with Locations((x, y, -0.01)):
                Cylinder(
                    FRAME_SCREW_HEAD_COUNTERBORE_DIAMETER / 2,
                    FRAME_SCREW_HEAD_COUNTERBORE_DEPTH + 0.01,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                    mode=Mode.SUBTRACT,
                )

        for x in landscape_xs:
            cut_vertical_keyhole(x, rest_y=0.0)
        for y in portrait_ys:
            cut_horizontal_keyhole(rest_x=PORTRAIT_WALL_SCREW_REST_X, y=y)

        cut_access_pattern(
            z_start=0.0,
            depth=BACKPLATE_THICKNESS,
            wide_wireless_opening=True,
        )

    return plate.part


def middle_plate_dimensions() -> tuple[float, float, float]:
    """Return the clearance-sized removable shield plate dimensions."""
    return (
        CASE_WIDTH - 2 * SIDE_WALL_THICKNESS - 2 * MIDDLE_PLATE_EDGE_CLEARANCE,
        CASE_HEIGHT - 2 * SIDE_WALL_THICKNESS - 2 * MIDDLE_PLATE_EDGE_CLEARANCE,
        max(0.5, CASE_CORNER_RADIUS - SIDE_WALL_THICKNESS - MIDDLE_PLATE_EDGE_CLEARANCE),
    )


def build_middle_plate():
    """Build the removable PCB shield; wireless access is only at the pins."""
    width, height, radius = middle_plate_dimensions()
    with BuildPart() as plate:
        with BuildSketch(Plane.XY.offset(PCB_SHIELD_Z)):
            RectangleRounded(width, height, radius)
        extrude(amount=PCB_SHIELD_THICKNESS)

        # The posts stop at the display-facing side of this plate, leaving a
        # small aligned screw passage through both plates.
        for x, y in screw_locations():
            with Locations((x, y, PCB_SHIELD_Z - 0.1)):
                Cylinder(
                    FRAME_SCREW_CLEARANCE_DIAMETER / 2,
                    PCB_SHIELD_THICKNESS + 0.2,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                    mode=Mode.SUBTRACT,
                )

        cut_access_pattern(
            z_start=PCB_SHIELD_Z,
            depth=PCB_SHIELD_THICKNESS,
            wide_wireless_opening=False,
        )

    return plate.part


def build_enclosure_frame():
    """Build the printable perimeter body with a ledge for the middle plate."""
    frame_holes = screw_locations()
    inner_width = CASE_WIDTH - 2 * SIDE_WALL_THICKNESS
    inner_height = CASE_HEIGHT - 2 * SIDE_WALL_THICKNESS
    inner_radius = max(0.5, CASE_CORNER_RADIUS - SIDE_WALL_THICKNESS)
    middle_width, middle_height, middle_radius = middle_plate_dimensions()
    ledge_inner_width = middle_width - 2 * MIDDLE_PLATE_LEDGE_WIDTH
    ledge_inner_height = middle_height - 2 * MIDDLE_PLATE_LEDGE_WIDTH
    ledge_inner_radius = max(0.5, middle_radius - MIDDLE_PLATE_LEDGE_WIDTH)
    ledge_z = PCB_SHIELD_Z - MIDDLE_PLATE_LEDGE_HEIGHT

    with BuildPart() as frame:
        # Open perimeter body. The broad center stays open to avoid trapped
        # support; a thin internal ledge carries the separate middle plate.
        with BuildSketch(Plane.XY.offset(BACKPLATE_THICKNESS)):
            RectangleRounded(CASE_WIDTH, CASE_HEIGHT, CASE_CORNER_RADIUS)
            RectangleRounded(
                inner_width,
                inner_height,
                inner_radius,
                mode=Mode.SUBTRACT,
            )
        extrude(amount=MOUNT_DEPTH - BACKPLATE_THICKNESS)

        with BuildSketch(Plane.XY.offset(ledge_z)):
            RectangleRounded(
                inner_width + 0.2,
                inner_height + 0.2,
                inner_radius + 0.1,
            )
            RectangleRounded(
                ledge_inner_width,
                ledge_inner_height,
                ledge_inner_radius,
                mode=Mode.SUBTRACT,
            )
        extrude(amount=MIDDLE_PLATE_LEDGE_HEIGHT)

        for x, y in frame_holes:
            with Locations((x, y, PCB_SHIELD_INNER_Z)):
                Cylinder(
                    FRAME_POST_DIAMETER / 2,
                    MOUNT_DEPTH - PCB_SHIELD_INNER_Z,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                )

        for x, y in frame_holes:
            with Locations((x, y, PCB_SHIELD_INNER_Z - 0.1)):
                Cylinder(
                    FRAME_SCREW_CLEARANCE_DIAMETER / 2,
                    MOUNT_DEPTH - PCB_SHIELD_INNER_Z + 0.2,
                    align=(Align.CENTER, Align.CENTER, Align.MIN),
                    mode=Mode.SUBTRACT,
                )

        cut_wall_window(
            axis=mirrored_side("right"),
            center=0.0,
            width=USB_C_SIDE_OPENING_WIDTH,
            height=USB_C_SIDE_OPENING_HEIGHT,
            center_z=USB_C_SIDE_OPENING_Z,
        )
        cut_wall_window(
            axis=mirrored_side("right"),
            center=DEBUG_SIDE_OPENING_Y,
            width=DEBUG_SIDE_OPENING_WIDTH,
            height=DEBUG_SIDE_OPENING_HEIGHT,
            center_z=DEBUG_SIDE_OPENING_Z,
        )
        cut_wall_window(
            axis=mirrored_side("left"),
            center=LEFT_SERVICE_OPENING_Y,
            width=LEFT_SERVICE_OPENING_WIDTH,
            height=LEFT_SERVICE_OPENING_HEIGHT,
            center_z=LEFT_SERVICE_OPENING_Z,
        )
        cut_wall_window(
            axis="bottom",
            center=(-TF_SLOT_OPENING_X if MIRROR_BOARD_LAYOUT_X else TF_SLOT_OPENING_X),
            width=TF_SLOT_OPENING_WIDTH,
            height=TF_SLOT_OPENING_HEIGHT,
            center_z=TF_SLOT_OPENING_Z,
        )

    return frame.part


def build_assembly(backplate=None, frame=None, middle_plate=None):
    """Return the three separate printable parts in their assembled positions."""
    return Compound(children=[
        backplate if backplate is not None else build_backplate(),
        frame if frame is not None else build_enclosure_frame(),
        middle_plate if middle_plate is not None else build_middle_plate(),
    ])


def validate_parts(backplate, frame, middle_plate, assembly) -> None:
    """Check the three parts, screw stack, wireless relief, and orientations."""
    for label, shape in (("wall backplate", backplate),
                         ("enclosure frame", frame),
                         ("middle plate", middle_plate)):
        if not shape.is_valid or len(shape.solids()) != 1:
            raise RuntimeError(f"{label} is not one valid solid.")
    if len(assembly.solids()) != 3:
        raise RuntimeError("The assembled model must contain three separate solids.")
    if FRAME_SCREW_CLEARANCE_DIAMETER >= FRAME_SCREW_HEAD_DIAMETER:
        raise RuntimeError("The screw head must be wider than its shank hole.")
    if FRAME_SCREW_HEAD_COUNTERBORE_DIAMETER <= FRAME_SCREW_HEAD_DIAMETER:
        raise RuntimeError("The screw-head counterbore needs radial clearance.")
    if not (FRAME_SCREW_HEAD_HEIGHT <= FRAME_SCREW_HEAD_COUNTERBORE_DEPTH
            < BACKPLATE_THICKNESS):
        raise RuntimeError("The head counterbore must seat the head and leave a plate shoulder.")
    if (WALL_SCREW_HEAD_CHAMBER_DEPTH
            < WALL_SCREW_HEAD_HEIGHT + WALL_SCREW_HEAD_CLEARANCE):
        raise RuntimeError("Wall-screw head chamber is shallower than its configured head envelope.")
    if abs(BACKPLATE_THICKNESS + WALL_SCREW_HEAD_CHAMBER_DEPTH + PCB_SHIELD_THICKNESS
           + PCB_COMPONENT_CLEARANCE - MOUNT_DEPTH) > 1e-6:
        raise RuntimeError("Layer dimensions do not add up to the specified overall depth.")

    def require_void(shape, point: tuple[float, float, float], description: str) -> None:
        if shape.is_inside(Vector(*point), tolerance=1e-4):
            raise RuntimeError(f"Expected clearance at {description}, point={point}.")

    def require_solid(shape, point: tuple[float, float, float], description: str) -> None:
        if not shape.is_inside(Vector(*point), tolerance=1e-4):
            raise RuntimeError(f"Expected material at {description}, point={point}.")

    # Original frame screws use small aligned passages through both plates and
    # the frame posts. Their larger heads bear on the backplate counterbores.
    for index, (x, y) in enumerate(screw_locations(), start=1):
        require_void(backplate, (x, y, BACKPLATE_THICKNESS / 2),
                     f"backplate screw passage {index}")
        require_void(backplate, (x, y, BACKPLATE_THICKNESS - 0.1),
                     f"backplate shank passage {index}")
        require_solid(backplate, (x + 2.2, y, 1.2),
                      f"backplate counterbore shoulder {index}")
        require_void(frame, (x, y, 8.0), f"frame screw passage {index}")
        require_void(middle_plate, (x, y,
                                    PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                     f"middle plate screw passage {index}")
        require_solid(middle_plate, (x + 2.2, y,
                                     PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                      f"middle plate material beside screw passage {index}")
        post_probe_x = x + (1 if x > 0 else -1) * 2.5
        require_solid(frame, (post_probe_x, y, 8.0),
                      f"frame screw post material {index}")
        require_void(frame, (post_probe_x, y, PCB_SHIELD_Z + 0.5),
                     f"frame post gap at middle plate {index}")

    # Wall keyholes belong only to the outer plate; the chamber behind them is
    # open up to the middle plate, which remains a continuous screw shield.
    for x in (-LANDSCAPE_WALL_SCREW_SPACING / 2,
              LANDSCAPE_WALL_SCREW_SPACING / 2):
        require_void(backplate, (x, -WALL_SLOT_TRAVEL, BACKPLATE_THICKNESS / 2),
                     "landscape keyhole entry")
        require_void(backplate, (x, 0.0, BACKPLATE_THICKNESS / 2),
                     "landscape keyhole neck")
        require_void(frame, (x, 0.0, BACKPLATE_THICKNESS + 1.0),
                     "landscape screw-head chamber")
        require_solid(middle_plate, (x, 0.0,
                                     PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                      "landscape screw shield")
    for y in (-PORTRAIT_WALL_SCREW_SPACING / 2,
              PORTRAIT_WALL_SCREW_SPACING / 2):
        entry_x = PORTRAIT_WALL_SCREW_REST_X + (
            -1 if MIRROR_BOARD_LAYOUT_X else 1
        ) * WALL_SLOT_TRAVEL
        require_void(backplate, (entry_x, y, BACKPLATE_THICKNESS / 2),
                     "portrait keyhole entry")
        require_void(backplate, (PORTRAIT_WALL_SCREW_REST_X, y,
                                 BACKPLATE_THICKNESS / 2),
                     "portrait keyhole neck")
        require_solid(middle_plate, (PORTRAIT_WALL_SCREW_REST_X, y,
                                     PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                      "portrait screw shield")

    # The wall plate opens across the extension-board footprint. The middle
    # plate remains solid between the two independent header pin windows.
    wireless_x, wireless_y = case_xy_from_board(
        WIRELESS_MODULE_OUTER_OPENING[0], WIRELESS_MODULE_OUTER_OPENING[1]
    )
    require_void(backplate, (wireless_x, wireless_y, BACKPLATE_THICKNESS / 2),
                 "outer extension-board clearance window")
    require_solid(middle_plate, (wireless_x, wireless_y,
                                 PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                  "solid bridge between middle-plate header openings")
    for label, board_x, board_y, _width, _height in REAR_CONNECTOR_OPENINGS:
        x, y = case_xy_from_board(board_x, board_y)
        require_void(middle_plate, (x, y, PCB_SHIELD_Z + PCB_SHIELD_THICKNESS / 2),
                     f"{label} middle-plate access")

    # USB-C must be accessible on the expected landscape edge and, after a
    # counterclockwise quarter-turn, through the portrait bottom wall.
    usb_wall_x = ((-1 if MIRROR_BOARD_LAYOUT_X else 1)
                  * (CASE_WIDTH / 2 - SIDE_WALL_THICKNESS / 2))
    require_void(frame, (usb_wall_x, 0.0, USB_C_SIDE_OPENING_Z), "USB-C side hole")
    portrait = Rot(0, 0, PORTRAIT_ROTATION_DEGREES) * assembly
    require_void(portrait, (0.0, -CASE_WIDTH / 2 + 2.0, USB_C_SIDE_OPENING_Z),
                 "portrait bottom USB-C access")

    for first_name, first, second_name, second in (
        ("backplate", backplate, "frame", frame),
        ("frame", frame, "middle plate", middle_plate),
        ("backplate", backplate, "middle plate", middle_plate),
    ):
        overlap = sum(shape.volume for shape in first.intersect(second))
        if overlap > 1e-5:
            raise RuntimeError(
                f"{first_name} and {second_name} overlap by {overlap:.6f} mm^3."
            )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).parent / "exports",
        help="Directory for STEP and STL exports (default: CAD/exports).",
    )
    args = parser.parse_args()

    backplate = build_backplate()
    frame = build_enclosure_frame()
    middle_plate = build_middle_plate()
    assembly = build_assembly(backplate, frame, middle_plate)
    validate_parts(backplate, frame, middle_plate, assembly)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    assembly_step_path = args.output_dir / "wall_mount_assembly.step"

    middle_width, middle_height, _ = middle_plate_dimensions()
    print_parts = (
        (
            "wall_backplate",
            backplate,
            backplate,
            (MOUNT_WIDTH, MOUNT_HEIGHT, BACKPLATE_THICKNESS),
        ),
        (
            "enclosure_frame",
            frame,
            Pos(0, 0, -BACKPLATE_THICKNESS) * frame,
            (MOUNT_WIDTH, MOUNT_HEIGHT, MOUNT_DEPTH - BACKPLATE_THICKNESS),
        ),
        (
            "middle_plate",
            middle_plate,
            Pos(0, 0, -PCB_SHIELD_Z) * middle_plate,
            (middle_width, middle_height, PCB_SHIELD_THICKNESS),
        ),
    )

    export_step(assembly, assembly_step_path)
    for part_name, _assembled_part, print_part, expected_dimensions in print_parts:
        export_step(print_part, args.output_dir / f"{part_name}.step")
        stl_path = args.output_dir / f"{part_name}.stl"
        export_stl(print_part, stl_path, tolerance=0.08, angular_tolerance=0.15)

        exported_step = import_step(args.output_dir / f"{part_name}.step")
        exported_stl = import_stl(stl_path)
        if not exported_step.is_valid:
            raise RuntimeError(f"The {part_name} STEP export is invalid.")
        if not stl_path.is_file() or stl_path.stat().st_size == 0:
            raise RuntimeError(f"The {part_name} STL export is missing or empty.")
        for export_name, exported in (("STEP", exported_step), ("STL", exported_stl)):
            bounds = exported.bounding_box()
            dimensions = (bounds.size.X, bounds.size.Y, bounds.size.Z)
            if any(abs(actual - expected) > 0.1
                   for actual, expected in zip(dimensions, expected_dimensions)):
                raise RuntimeError(
                    f"{part_name} {export_name} has unexpected dimensions: {dimensions}."
                )

    assembled_check = import_step(assembly_step_path)
    if not assembled_check.is_valid or len(assembled_check.solids()) != 3:
        raise RuntimeError("The assembled STEP must re-import as three valid solids.")
    assembly_bounds = assembled_check.bounding_box()
    assembly_dimensions = (
        assembly_bounds.size.X, assembly_bounds.size.Y, assembly_bounds.size.Z
    )
    if any(abs(actual - expected) > 0.1 for actual, expected in zip(
        assembly_dimensions, (MOUNT_WIDTH, MOUNT_HEIGHT, MOUNT_DEPTH)
    )):
        raise RuntimeError(f"Assembly STEP has unexpected dimensions: {assembly_dimensions}.")

    # Remove the former one-piece files so they cannot be mistaken for the
    # revised three-part print set.
    for old_export in ("wall_mount.step", "wall_mount.stl"):
        stale = args.output_dir / old_export
        if stale.exists():
            stale.unlink()

    print(f"Valid three-part assembly: {assembly_dimensions[0]:.2f} x "
          f"{assembly_dimensions[1]:.2f} x {assembly_dimensions[2]:.2f} mm")
    print(f"Assembly STEP: {assembly_step_path}")
    for part_name, _assembled_part, _print_part, _expected_dimensions in print_parts:
        print(f"{part_name}: {args.output_dir / (part_name + '.stl')}")

    # Always send the model when the Python script is executed.
    from ocp_vscode import Camera, show

    show(assembly, reset_camera=Camera.BOTTOM)


if __name__ == "__main__":
    main()
