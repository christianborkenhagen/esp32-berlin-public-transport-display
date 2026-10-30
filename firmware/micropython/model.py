"""Platform-independent state for the MicroPython/LVGL prototype."""

SCENES = (
    "Speedometer", "Moving blocks", "Animated arcs", "Translucent layers",
    "Scrolling list", "Static idle", "Static 1 pixel", "Static full redraw",
)
COLORS = (0x27C5B1, 0x4D92FF, 0xFCA85D, 0xF47487, 0xAB8EFA, 0xF3D35E)
STATION_NAMES = ("Alexanderplatz", "Hauptbahnhof")
# Fictional departures, not a transport API.
STATIONS = (
    (("U2", "Pankow", 0xE52829, 2),
     ("S5", "Spandau", 0xF28623, 4),
     ("M10", "Hauptbahnhof", 0xD52432, 6),
     ("U8", "Wittenau", 0x2357A4, 8),
     ("S7", "Potsdam Hbf", 0x8F468C, 10),
     ("M4", "Hackescher Markt", 0xD52432, 12),
     ("U5", "Alexanderplatz", 0x925C36, 14),
     ("S3", "Ostkreuz", 0x155B9E, 16),
     ("TXL", "Sample destination", 0xA13131, 18),
     ("U6", "Alt-Tegel", 0x7C468A, 20)),
    (("S3", "Erkner", 0x155B9E, 1),
     ("S5", "Strausberg Nord", 0xF28623, 3),
     ("U5", "Hönow", 0x925C36, 5),
     ("M10", "Warschauer Straße", 0xD52432, 7),
     ("S7", "Ahrensfelde", 0x8F468C, 9),
     ("S9", "BER Airport", 0x8C1D25, 11),
     ("M5", "Hohenschönhausen", 0xD52432, 13),
     ("S75", "Wartenberg", 0x8F468C, 15),
     ("U5", "Alexanderplatz", 0x925C36, 17),
     ("S3", "Spandau", 0x155B9E, 19)),
)


def triangle(elapsed_ms, duration_ms):
    """Return an integer in 0..1000, reversing every duration_ms."""
    phase = elapsed_ms % (2 * duration_ms)
    if phase > duration_ms:
        phase = 2 * duration_ms - phase
    return phase * 1000 // duration_ms


class Departures:
    def __init__(self):
        self.station = 0
        self.minutes = [[row[3] for row in station] for station in STATIONS]
        self.next_index = [0] * len(STATIONS)

    def select(self, station):
        if station < 0 or station >= len(STATIONS):
            raise ValueError("unknown station")
        self.station = station

    def update(self):
        """Update one displayed simulated departure and return its row index."""
        station = self.station
        i = self.next_index[station] % len(STATIONS[station])
        self.next_index[station] += 1
        value = self.minutes[station][i]
        self.minutes[station][i] = value - 1 if value > 1 else 20
        return i


class Benchmark:
    """Scene sequencing; timestamps must be compared with ticks_diff."""

    def __init__(self, now):
        self.scene = 0
        self.period_ms = 33
        self.started = now

    def reset(self, now):
        self.scene = 0
        self.period_ms = 33
        self.started = now

    def advance_if_due(self, now, ticks_diff):
        if ticks_diff(now, self.started) < 12000:
            return False
        self.scene = (self.scene + 1) % len(SCENES)
        if self.scene == 0:
            self.period_ms = 25 if self.period_ms == 33 else 33
        self.started = now
        return True


class FlushMeter:
    """Count LVGL frame flush submissions, not scanout or presented frames."""

    def __init__(self, now):
        self.count = 0
        self.reset(now)

    def reset(self, now):
        self.previous_count = self.count
        self.previous_time = now

    def record(self):
        self.count += 1

    def sample(self, now, ticks_diff):
        elapsed = ticks_diff(now, self.previous_time)
        if elapsed <= 0:
            return None
        result = (self.count - self.previous_count) * 1000.0 / elapsed
        self.reset(now)
        return result
