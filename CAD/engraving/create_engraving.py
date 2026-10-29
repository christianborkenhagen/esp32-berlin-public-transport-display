"""Recreate the V1.1 rear markings from the supplied photograph.

The photograph is registered to the four V1.1 mounting-hole centres.
All artwork positions and glyph sizes are photographic estimates, not CAD data.
"""
from pathlib import Path
from xml.sax.saxutils import escape

import numpy as np
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

OUT = Path(__file__).resolve().parent
W, H = 136.4, 84.7

# Photo coordinates of the four screw centres, matched to case coordinates in
# millimetres. The 126 x 74.3 mm spacing is from the manufacturer's V1.1 PCB.
photo = [(94.5, 69.0), (588.0, 69.5), (94.5, 360.0), (588.0, 356.5)]
world = [(5.2, 5.2), (131.2, 5.2), (5.2, 79.5), (131.2, 79.5)]
a, b = [], []
for (px, py), (x, y) in zip(photo, world):
    a += [[px, py, 1, 0, 0, 0, -x * px, -x * py],
          [0, 0, 0, px, py, 1, -y * px, -y * py]]
    b += [x, y]
matrix = np.append(np.linalg.solve(a, b), 1).reshape(3, 3)


def xy(x, y):
    p = matrix @ [x, y, 1]
    return float(p[0] / p[2]), float(p[1] / p[2])


def box(x0, y0, x1, y1):
    a = xy(x0, y0)
    b = xy(x1, y1)
    return a[0], a[1], b[0] - a[0], b[1] - a[1]


fonts = {
    "regular": TTFont(r"C:\Windows\Fonts\arial.ttf"),
    "bold": TTFont(r"C:\Windows\Fonts\arialbd.ttf"),
}
elements = []


def add_text(label, rect, weight="regular", vertical=False):
    """Fit vector glyph contours exactly to the estimated ink bounding box."""
    x, y, width, height = rect
    if vertical:
        cx, cy = x + width / 2, y + height / 2
        x, y, width, height = cx - height / 2, cy - width / 2, height, width

    font = fonts[weight]
    glyphs = font.getGlyphSet()
    cmap = font.getBestCmap()
    metrics = font["hmtx"].metrics
    bounds_pen = BoundsPen(glyphs)
    cursor = 0
    for char in label:
        name = cmap[ord(char)]
        glyphs[name].draw(TransformPen(bounds_pen, (1, 0, 0, 1, cursor, 0)))
        cursor += metrics[name][0]
    xmin, ymin, xmax, ymax = bounds_pen.bounds
    sx, sy = width / (xmax - xmin), height / (ymax - ymin)

    pen = SVGPathPen(glyphs)
    cursor = 0
    for char in label:
        name = cmap[ord(char)]
        glyphs[name].draw(TransformPen(pen, (1, 0, 0, 1, cursor, 0)))
        cursor += metrics[name][0]
    transform = f"translate({x - xmin*sx:.5f} {y + ymax*sy:.5f}) scale({sx:.8f} {-sy:.8f})"
    if vertical:
        transform = f"rotate(-90 {cx:.5f} {cy:.5f}) " + transform
    elements.append(
        f'<path aria-label="{escape(label)}" d="{pen.getCommands()}" '
        f'transform="{transform}" fill="#000"/>'
    )


def text(label, coords, weight="regular", vertical=False):
    add_text(label, box(*coords), weight, vertical)


def polygon(points, closed=False, fill="none", stroke="#000", sw=0.19):
    coords = [xy(*p) for p in points]
    d = "M " + " L ".join(f"{x:.4f},{y:.4f}" for x, y in coords)
    if closed:
        d += " Z"
    elements.append(
        f'<path d="{d}" fill="{fill}" stroke="{stroke}" '
        f'stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"/>'
    )


def rounded_rect(coords, radius=1.0, sw=0.22):
    x, y, w, h = box(*coords)
    elements.append(f'<rect x="{x:.4f}" y="{y:.4f}" width="{w:.4f}" height="{h:.4f}" '
                    f'rx="{radius}" fill="none" stroke="#000" stroke-width="{sw}"/>')


# All coordinates below are estimated from the photograph (pixel coordinates).
text("UART0-OUT", (160, 94, 210, 101))
text("UART1-OUT", (240, 94, 290, 101))
text("I2C-OUT", (325, 94, 362, 101))
text("Advance-5.0", (295, 206, 395, 220), "bold")
text("BOOT", (106, 235, 136, 244))
text("RESET", (106, 261, 144, 270))
text("USB-5V-IN", (563, 207, 603, 214))
text("CHG", (578, 148, 583, 163), vertical=True)
text("PWR", (587, 148, 592, 163), vertical=True)

# Wireless-module direction marker.
rounded_rect((493, 153, 520, 186), radius=1.3, sw=0.25)
polygon([(502.5, 167), (507, 159), (511.5, 167)],
        closed=True, fill="#000", stroke="none")
text("W-M", (497, 172, 516, 180), "bold")

# S1/S0 switch labels and the V1.1 function selection table.
text("1", (354, 267, 359, 276))
text("0", (354, 279, 360, 287))
text("S1", (362, 289, 372, 296))
text("S0", (377, 289, 385, 296))
polygon([(387, 256), (479, 256), (479, 296), (387, 296)], closed=True, sw=0.22)
polygon([(414, 256), (414, 296)], sw=0.22)
for yy in (266, 276, 286):
    polygon([(387, yy), (479, yy)], sw=0.22)
text("S1-S0", (389, 258, 412, 264))
for values, yy in [("00", 268), ("01", 278), ("11", 288)]:
    text(values[0], (390, yy, 394, yy + 6))
    text(values[1], (401, yy, 405, yy + 6))
text("MIC&SPK", (416, 268, 458, 274))
text("WM", (416, 278, 434, 284))
text("MIC&TF CARD", (416, 288, 476, 294))

# TF-card pictogram and arrow. No card slot cut is included.
polygon([(151, 325), (148, 329), (150, 329), (150, 336),
         (152, 336), (152, 329), (154, 329)], closed=True, fill="#000", stroke="none")
polygon([(143, 339), (162, 339), (162, 345), (165, 348), (165, 352),
         (162, 355), (165, 359), (165, 371), (143, 371)],
        closed=True, sw=0.28)
polygon([(144, 367), (149, 365.5), (158, 365.5), (164, 367)], sw=0.22)
text("TF", (145, 351, 161, 362))
text("SPK", (225, 359, 241, 367))
text("UART0-IN", (512, 359, 553, 367))

text("GND", (515, 307, 522, 323), vertical=True)
text("5V-IN", (524, 299, 531, 323), vertical=True)
text("IO43-TX0", (533, 282, 540, 323), vertical=True)
text("IO44-RX0", (542, 282, 549, 323), vertical=True)


def svg(inner):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" '
            f'viewBox="0 0 {W} {H}">\n'
            '<title>CrowPanel Advance 5.0 V1.1 rear markings — estimated</title>\n'
            + "\n".join(inner) + "\n</svg>\n")


(OUT / "crowpanel-v1.1-rear-engraving.svg").write_text(svg(elements), encoding="utf-8")

# A separate print-only registration sheet; these blue marks are NOT engraving.
guide = elements + [
    f'<rect x="0.1" y="0.1" width="{W-0.2}" height="{H-0.2}" '
    'rx="5" fill="none" stroke="#1976d2" stroke-width="0.2" stroke-dasharray="1 1"/>'
]
for x, y in world:
    guide.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="none" '
                 'stroke="#1976d2" stroke-width="0.2"/>')
(OUT / "crowpanel-v1.1-registration-PRINT-ONLY.svg").write_text(svg(guide), encoding="utf-8")
print("Created SVG artwork and print-only registration guide.")
