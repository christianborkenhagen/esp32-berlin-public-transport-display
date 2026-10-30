"""MicroPython LVGL transport prototype and benchmark.

The measured rate is LVGL's last-flush submissions, not RGB scanout or
confirmed presented frames. No CPU estimate is available in this port.
"""

import time
import lvgl as lv
from model import Benchmark, COLORS, Departures, FlushMeter, SCENES, STATIONS, STATION_NAMES, triangle


def rectangle(parent, x, y, width, height, color, radius=0, opacity=255):
    obj = lv.obj(parent)
    obj.remove_style_all()
    obj.set_pos(x, y)
    obj.set_size(width, height)
    obj.set_style_bg_color(lv.color_hex(color), 0)
    obj.set_style_bg_opa(255, 0)
    obj.set_style_radius(radius, 0)
    obj.set_style_opa(opacity, 0)
    obj.clear_flag(lv.obj.FLAG.SCROLLABLE)
    return obj


def label(parent, text, x, y, size=14, color=0xFFFFFF):
    obj = lv.label(parent)
    obj.set_text(text)
    obj.set_pos(x, y)
    obj.set_style_text_color(lv.color_hex(color), 0)
    font = getattr(lv, "font_montserrat_%d" % size, None)
    if font is not None:
        obj.set_style_text_font(font, 0)
    return obj


def button(parent, caption, x, y, width, callback):
    obj = rectangle(parent, x, y, width, 40, 0x334759, 4)
    obj.add_flag(lv.obj.FLAG.CLICKABLE)
    text = label(obj, caption, 0, 0)
    text.center()
    obj.add_event_cb(callback, lv.EVENT.CLICKED, None)
    return obj


class App:
    def __init__(self, display, now=None):
        self.display = display
        self.now = now or time.ticks_ms
        start = self.now()
        self.departures = Departures()
        self.benchmark = Benchmark(start)
        self.meter = FlushMeter(start)
        self.benchmark_active = False
        self.metric_available = hasattr(display, "flush_is_last") and hasattr(lv.EVENT, "FLUSH_START")
        self.pixel_on = False
        self.scene_items = []

        self.screen = lv.screen_active()
        self.screen.clean()
        self.screen.set_style_bg_color(lv.color_hex(0x101923), 0)
        self.screen.set_style_bg_opa(255, 0)
        self.screen.clear_flag(lv.obj.FLAG.SCROLLABLE)

        self.transport = rectangle(self.screen, 0, 0, 800, 480, 0x101923)
        label(self.transport, "BERLIN | DEPARTURES", 20, 12, 16)
        label(self.transport, "SIMULATION - no live data", 20, 40, 12, 0xB4C1CE)
        self.tabs = []
        for i in range(2):
            tab = button(self.transport, STATION_NAMES[i], 20 + i * 225, 69, 215,
                         lambda _event, station=i: self.select_station(station))
            self.tabs.append(tab)
        button(self.transport, "Benchmark", 645, 16, 135,
               lambda _event: self.select_mode(True))
        self.departure_list = rectangle(self.transport, 0, 117, 800, 363, 0x101923)
        self.departure_list.add_flag(lv.obj.FLAG.SCROLLABLE)
        self.departure_list.set_scroll_dir(lv.DIR.VER)
        self.times = []

        self.bench = rectangle(self.screen, 0, 0, 800, 480, 0x111D2B)
        header = rectangle(self.bench, 0, 0, 800, 76, 0x17283A)
        self.title = label(header, "DISPLAY BENCHMARK", 18, 6, 16)
        self.status = label(header, "", 18, 42, 14, 0x75E1CE)
        button(header, "Back", 704, 12, 80, lambda _event: self.select_mode(False))
        self.scene = rectangle(self.bench, 0, 76, 800, 404, 0x111D2B)
        self.bench.add_flag(lv.obj.FLAG.HIDDEN)

        if self.metric_available:
            self.display.add_event_cb(self.on_flush, lv.EVENT.FLUSH_START, None)
        self.tick_timer = lv.timer_create(self.on_tick, 33, None)
        self.metrics_timer = lv.timer_create(self.on_metrics, 1000, None)
        self.departure_timer = lv.timer_create(self.on_departure, 5000, None)
        self.select_station(0)

    def select_station(self, index):
        self.departures.select(index)
        for i, tab in enumerate(self.tabs):
            tab.set_style_bg_color(lv.color_hex(0x256590 if i == index else 0x334759), 0)
        self.departure_list.clean()
        self.times = []
        for i, (line, destination, color, _minutes) in enumerate(STATIONS[index]):
            row = rectangle(self.departure_list, 12, i * 62, 764, 60, 0x1C2A36)
            badge = rectangle(row, 9, 10, 68, 40, color)
            line_label = label(badge, line, 0, 0, 16)
            line_label.center()
            label(row, destination, 96, 20, 16)
            self.times.append(label(row, "%d min" % self.departures.minutes[index][i], 648, 20, 16))

    def on_departure(self, _timer):
        if not self.benchmark_active:
            index = self.departures.update()
            self.times[index].set_text("%d min" % self.departures.minutes[self.departures.station][index])

    def select_mode(self, benchmark):
        self.benchmark_active = benchmark
        if benchmark:
            self.transport.add_flag(lv.obj.FLAG.HIDDEN)
            self.bench.clear_flag(lv.obj.FLAG.HIDDEN)
            self.benchmark.reset(self.now())
            self.set_period()
            self.load_scene()
        else:
            self.bench.add_flag(lv.obj.FLAG.HIDDEN)
            self.transport.clear_flag(lv.obj.FLAG.HIDDEN)

    def set_period(self):
        self.tick_timer.set_period(self.benchmark.period_ms)
        # This controls LVGL's refresh timer if exposed by the binding; it is
        # still a target, not a guarantee that rendering meets this cadence.
        if hasattr(self.display, "get_refr_timer"):
            timer = self.display.get_refr_timer()
            if timer is not None:
                timer.set_period(self.benchmark.period_ms)

    def arc(self, x, y, size, color, width, maximum):
        obj = lv.arc(self.scene)
        obj.set_pos(x, y)
        obj.set_size(size, size)
        obj.set_rotation(135 if maximum == 160 else 270)
        obj.set_bg_angles(0, 270 if maximum == 160 else 360)
        obj.set_range(0, maximum)
        obj.set_style_arc_width(width, lv.PART.MAIN)
        obj.set_style_arc_width(width, lv.PART.INDICATOR)
        obj.set_style_arc_color(lv.color_hex(0x293A49), lv.PART.MAIN)
        obj.set_style_arc_color(lv.color_hex(color), lv.PART.INDICATOR)
        obj.remove_style(None, lv.PART.KNOB)
        obj.clear_flag(lv.obj.FLAG.CLICKABLE)
        return obj

    def load_scene(self):
        scene = self.benchmark.scene
        self.scene.clean()
        self.scene_items = []
        self.pixel_on = False
        self.title.set_text("DISPLAY BENCHMARK | " + SCENES[scene])
        if scene >= 5:
            self.status.set_text("Static scene: measurements in serial console")
        elif self.metric_available:
            self.status.set_text("LVGL flush/s: measurement starting")
        else:
            self.status.set_text("LVGL flush/s: unavailable")
        if scene == 0:
            self.scene_items.append(self.arc(24, 24, 356, 0xFFBF21, 38, 160))
            self.speed_label = label(self.scene, "0", 175, 180, 16, 0x70DDFF)
            label(self.scene, "mph", 180, 225, 14, 0x88B9D7)
            label(self.scene, "SPEEDOMETER", 455, 84, 16)
            label(self.scene, "Animated gauge", 455, 125, 14, 0xAEC8D4)
        elif scene == 1:
            for i in range(12):
                self.scene_items.append(rectangle(self.scene, 0, 18 + (i // 3) * 94,
                                                  82, 64, COLORS[i % 6], 10))
        elif scene == 2:
            for i in range(12):
                self.scene_items.append(self.arc(35 + (i % 6) * 126, 40 + (i // 6) * 165,
                                                 96, COLORS[i % 6], 12, 100))
        elif scene == 3:
            for i in range(8):
                self.scene_items.append(rectangle(self.scene, 0, 20 + (i % 3) * 105,
                                                  210, 150, COLORS[i % 6], 38, 153))
        elif scene == 4:
            self.scrolling = rectangle(self.scene, 0, 0, 800, 404, 0x111D2B)
            self.scrolling.add_flag(lv.obj.FLAG.SCROLLABLE)
            self.scrolling.set_scroll_dir(lv.DIR.VER)
            for i in range(16):
                row = rectangle(self.scrolling, 15, i * 66, 770, 60, 0x233549)
                rectangle(row, 10, 10, 54, 40, 0x27C5B1, 6)
                label(row, "Animated list item %d" % (i + 1), 85, 20, 16)
        else:
            label(self.scene, "STATIC DISPLAY", 45, 45, 16)
            descriptions = ("No periodic changes", "One pixel changes each period",
                            "Full-screen invalidation each period")
            label(self.scene, descriptions[scene - 5], 45, 90, 14, 0xAEC8D4)
            label(self.scene, "Measurement: LVGL flush/s, not panel FPS", 45, 130, 14, 0x75E1CE)
            if scene == 6:
                self.pixel = rectangle(self.scene, 400, 180, 1, 1, 0)
            for i, color in enumerate(COLORS):
                rectangle(self.scene, 45 + i * 115, 210, 85, 85, color, 10)
        self.meter.reset(self.now())
        self.render_scene(0)
        print("BENCH scene=%d name=%s period=%dms" %
              (scene, SCENES[scene], self.benchmark.period_ms))

    def on_tick(self, _timer):
        if not self.benchmark_active:
            return
        now = self.now()
        if self.benchmark.advance_if_due(now, time.ticks_diff):
            self.set_period()
            self.load_scene()
            return
        self.render_scene(time.ticks_diff(now, self.benchmark.started))

    def render_scene(self, elapsed):
        scene = self.benchmark.scene
        if scene == 0:
            speed = triangle(elapsed, 4500) * 160 // 1000
            self.scene_items[0].set_value(speed)
            self.speed_label.set_text(str(speed))
        elif scene == 1:
            for i, item in enumerate(self.scene_items):
                start = (i % 3) * 110
                item.set_x(start + (710 - 2 * start) * triangle(elapsed, 1000 + i * 87) // 1000)
        elif scene == 2:
            for i, item in enumerate(self.scene_items):
                item.set_value(5 + 95 * triangle(elapsed, 900 + i * 85) // 1000)
        elif scene == 3:
            for i, item in enumerate(self.scene_items):
                start = 30 + (i % 4) * 70
                item.set_x(start + (560 - 2 * start) * triangle(elapsed, 1300 + i * 140) // 1000)
        elif scene == 4:
            self.scrolling.scroll_to_y((16 * 66 - 404) * triangle(elapsed, 3500) // 1000, lv.ANIM.OFF)
        elif scene == 6:
            self.pixel_on = not self.pixel_on
            self.pixel.set_style_bg_color(lv.color_hex(0xFFFFFF if self.pixel_on else 0), 0)
        elif scene == 7:
            # This intentionally invalidates the complete screen, not just the
            # background. Static-idle (scene 5) changes no UI properties.
            self.screen.invalidate()

    def on_flush(self, _event):
        # FLUSH_START occurs before the asynchronous bus transfer completes.
        if self.display.flush_is_last():
            self.meter.record()

    def on_metrics(self, _timer):
        if not self.benchmark_active or not self.metric_available:
            return
        rate = self.meter.sample(self.now(), time.ticks_diff)
        if rate is None:
            return
        print("BENCH scene=%d LVGL last-flush submissions/s=%.1f (not presented FPS)" %
              (self.benchmark.scene, rate))
        if self.benchmark.scene < 5:
            self.status.set_text("LVGL flush/s: %.1f | target %d ms (not panel FPS)" %
                                 (rate, self.benchmark.period_ms))
