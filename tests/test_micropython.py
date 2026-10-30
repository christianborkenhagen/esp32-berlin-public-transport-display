"""Host-side logic/contract tests; these do not replace a device test."""

import importlib.util
import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1] / "firmware" / "micropython"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


model = load("mp_model_test", ROOT / "model.py")


class Clock:
    value = 0

    @classmethod
    def ticks_ms(cls):
        return cls.value

    @staticmethod
    def ticks_diff(a, b):
        # MicroPython tick counter wraps at a power of two.
        period = 1 << 30
        return ((a - b + period // 2) % period) - period // 2


class ModelTests(unittest.TestCase):
    def test_triangle(self):
        self.assertEqual([model.triangle(t, 1000) for t in (0, 500, 1000, 1500, 2000)],
                         [0, 500, 1000, 500, 0])

    def test_independent_stations_and_rollover(self):
        departures = model.Departures()
        departures.update()
        self.assertEqual(departures.minutes[0][0], 1)
        departures.select(1)
        departures.update()
        self.assertEqual(departures.minutes[1][0], 20)
        departures.select(0)
        self.assertEqual(departures.minutes[0][0], 1)
        self.assertEqual(departures.next_index, [1, 1])
        with self.assertRaises(ValueError):
            departures.select(2)

    def test_period_switch_and_tick_wrap(self):
        benchmark = model.Benchmark((1 << 30) - 2000)
        for i in range(8):
            now = ((1 << 30) - 2000 + (i + 1) * 12000) % (1 << 30)
            self.assertTrue(benchmark.advance_if_due(now, Clock.ticks_diff))
        self.assertEqual((benchmark.scene, benchmark.period_ms), (0, 25))

    def test_meter_counts_recorded_flushes_not_timer_calls(self):
        meter = model.FlushMeter(0)
        for _ in range(7):
            meter.record()
        self.assertEqual(meter.sample(1400, Clock.ticks_diff), 5.0)
        self.assertEqual(meter.sample(2400, Clock.ticks_diff), 0.0)


class FakeObj:
    FLAG = types.SimpleNamespace(SCROLLABLE=1, CLICKABLE=2, HIDDEN=4)

    def __init__(self, parent=None):
        self.parent = parent
        self.children = []
        self.flags = set()
        self.events = []
        self.text = None
        self.value = None
        self.invalidations = 0
        if parent:
            parent.children.append(self)

    def remove_style_all(self):
        pass

    def set_pos(self, x, y):
        self.x, self.y = x, y

    def set_size(self, width, height):
        self.width, self.height = width, height

    def set_x(self, value):
        self.x = value

    def set_value(self, value):
        self.value = value

    def set_text(self, text):
        self.text = text

    def clean(self):
        self.children.clear()

    def add_flag(self, flag):
        self.flags.add(flag)

    def clear_flag(self, flag):
        self.flags.discard(flag)

    def add_event_cb(self, callback, event, data):
        self.events.append((callback, event, data))

    def invalidate(self):
        self.invalidations += 1

    def scroll_to_y(self, value, animation):
        self.scroll_y = value

    def __getattr__(self, name):
        if name.startswith("set_style_") or name in {
            "set_scroll_dir", "center", "set_rotation", "set_bg_angles",
            "set_range", "remove_style",
        }:
            return lambda *args: None
        raise AttributeError(name)


class FakeTimer:
    def __init__(self, callback=None, period=None):
        self.callback, self.period = callback, period

    def set_period(self, value):
        self.period = value


class FakeDisplay:
    def __init__(self):
        self.last = True
        self.callbacks = []
        self.refresh_timer = FakeTimer()

    def add_event_cb(self, *args):
        self.callbacks.append(args)

    def flush_is_last(self):
        return self.last

    def get_refr_timer(self):
        return self.refresh_timer


def make_lv():
    lv = types.ModuleType("lvgl")
    lv.obj = FakeObj
    lv.arc = FakeObj
    lv.label = FakeObj
    lv.EVENT = types.SimpleNamespace(CLICKED=1, FLUSH_START=2)
    lv.PART = types.SimpleNamespace(MAIN=1, INDICATOR=2, KNOB=3)
    lv.DIR = types.SimpleNamespace(VER=1)
    lv.ANIM = types.SimpleNamespace(OFF=0)
    lv.INDEV_TYPE = types.SimpleNamespace(POINTER=1)
    lv.INDEV_STATE = types.SimpleNamespace(PRESSED=1, RELEASED=0)
    lv.COLOR_FORMAT = types.SimpleNamespace(RGB565=1)
    lv.color_hex = lambda color: color
    lv.screen_active = lambda: FakeObj()
    lv.timer_create = lambda callback, period, _data: FakeTimer(callback, period)
    return lv


class UITests(unittest.TestCase):
    def setUp(self):
        self.lv = make_lv()
        with patch.dict(sys.modules, {"lvgl": self.lv, "model": model}):
            self.ui = load("mp_ui_test", ROOT / "ui.py")
        self.ui.time = Clock
        Clock.value = 0
        self.display = FakeDisplay()
        self.app = self.ui.App(self.display, now=Clock.ticks_ms)

    def test_station_tabs_and_mode(self):
        self.assertFalse(self.app.benchmark_active)
        self.assertEqual(self.app.transport.children[0].text, "BERLIN | DEPARTURES")
        self.assertEqual(self.app.transport.children[1].text, "SIMULATION - no live data")
        self.assertEqual(self.app.bench.children[0].children[2].children[0].text, "Back")
        self.app.select_station(1)
        self.assertEqual(self.app.departures.station, 1)
        self.app.on_departure(None)
        self.assertEqual(self.app.times[0].text, "20 min")
        self.app.select_mode(True)
        self.assertTrue(self.app.benchmark_active)
        self.assertEqual(self.app.scene_items[0].value, 0)
        self.app.select_mode(False)
        self.assertFalse(self.app.benchmark_active)

    def test_eight_scenes_and_static_idle(self):
        self.app.select_mode(True)
        for scene in range(8):
            self.app.benchmark.scene = scene
            self.app.load_scene()
            self.assertIn(model.SCENES[scene], self.app.title.text)
        self.app.benchmark.scene = 5
        self.app.load_scene()
        self.assertEqual(self.app.status.text, "Static scene: measurements in serial console")
        invalidations = self.app.screen.invalidations
        self.app.render_scene(123)
        self.assertEqual(self.app.screen.invalidations, invalidations)
        self.app.benchmark.scene = 7
        self.app.load_scene()
        self.app.render_scene(123)
        self.assertGreater(self.app.screen.invalidations, invalidations)

    def test_only_last_flush_is_counted(self):
        self.app.select_mode(True)
        self.display.last = False
        self.app.on_flush(None)
        self.assertEqual(self.app.meter.count, 0)
        self.display.last = True
        self.app.on_flush(None)
        Clock.value = 1000
        self.app.on_metrics(None)
        self.assertIn("1.0", self.app.status.text)
        self.assertIn("not panel FPS", self.app.status.text)
        self.app.benchmark.scene = 5
        self.app.load_scene()
        before = self.app.status.text
        Clock.value = 2000
        self.app.on_metrics(None)
        self.assertEqual(self.app.status.text, before)

    def test_missing_counter_hook_is_not_reported_as_fps(self):
        display = types.SimpleNamespace(get_refr_timer=lambda: FakeTimer())
        self.app = self.ui.App(display, now=Clock.ticks_ms)
        self.app.select_mode(True)
        self.assertFalse(self.app.metric_available)
        self.assertIn("unavailable", self.app.status.text)

    def test_period_changes_only_after_eight_scenes(self):
        self.app.select_mode(True)
        for i in range(8):
            Clock.value = (i + 1) * 12000
            self.app.on_tick(None)
        self.assertEqual(self.app.benchmark.scene, 0)
        self.assertEqual(self.app.tick_timer.period, 25)
        self.assertEqual(self.display.refresh_timer.period, 25)


class BoardTests(unittest.TestCase):
    def setUp(self):
        self.lv = make_lv()
        self.lv.is_initialized = lambda: True
        self.display = FakeDisplay()
        self.lv.display_get_default = lambda: self.display

        class FakeIndev:
            def set_type(self, _value): pass
            def set_display(self, _display): pass
            def set_read_cb(self, callback): self.callback = callback
            def enable(self, _value): pass

        self.lv.indev_create = FakeIndev

        class FakeI2C:
            def __init__(self, host, **kwargs):
                self.host, self.configs = host, [kwargs]
                self.writes = []
                self.status = 0
                self.point = bytes((0, 0x20, 0x03, 0xDF, 0x01, 0, 0, 0))

            def scan(self): return [0x30, 0x5D]
            def init(self, **kwargs): self.configs.append(kwargs)
            def writeto(self, address, data): self.writes.append((address, data))
            def writeto_mem(self, address, register, data, **kwargs):
                self.writes.append((address, register, data, kwargs["addrsize"]))
            def readfrom_mem(self, address, register, size, **kwargs):
                if register == 0x814E:
                    return bytes((self.status,))
                return self.point

        class FakeBus:
            def __init__(self, **kwargs): self.config = kwargs
            def allocate_framebuffer(self, size, caps): return bytearray(size)

        class FakeDriver:
            def __init__(self, **kwargs): self.config = kwargs
            def init(self): pass

        machine = types.SimpleNamespace(Pin=lambda n: n, I2C=FakeI2C)
        lcd_bus = types.SimpleNamespace(RGBBus=FakeBus, MEMORY_INTERNAL=1, MEMORY_DMA=2)
        rgb_display = types.SimpleNamespace(RGBDisplay=FakeDriver)
        with patch.dict(sys.modules, {"lvgl": self.lv, "machine": machine,
                                      "lcd_bus": lcd_bus, "rgb_display": rgb_display}):
            self.board_module = load("mp_board_test", ROOT / "board.py")
        self.board = self.board_module.Board()

    def test_known_board_parameters_and_backlight(self):
        self.assertEqual(self.board.bus.config["freq"], 16000000)
        self.assertEqual(self.board.bus.config["data0"], 21)
        self.assertEqual(self.board.bus.config["data15"], 46)
        self.assertTrue(self.board.bus.config["pclk_active_low"])
        self.assertEqual(self.board.i2c.writes[:2], [(0x30, b"\x10"), (0x30, b"\x06")])
        self.assertEqual(len(self.board.buffer1), 800 * 20 * 2)

    def test_gt911_press_hold_and_release(self):
        data = types.SimpleNamespace(point=types.SimpleNamespace(x=0, y=0))
        self.board.i2c.status = 0x81
        self.board.touch.read(None, data)
        self.assertEqual((data.point.x, data.point.y, data.state), (799, 479, 1))
        self.board.i2c.status = 0
        self.board.touch.read(None, data)
        self.assertEqual(data.state, 1)
        self.board.i2c.status = 0x80
        self.board.touch.read(None, data)
        self.assertEqual(data.state, 0)


if __name__ == "__main__":
    unittest.main()
