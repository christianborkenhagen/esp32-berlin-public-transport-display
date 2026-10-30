"""CrowPanel Advance 5.0 V1.1 for lvgl_micropython's RGBBus."""

import machine
import lvgl as lv
import lcd_bus
import rgb_display

WIDTH = 800
HEIGHT = 480
_TOUCH = 0x5D
_BACKLIGHT = 0x30
_STATUS = 0x814E


class GT911:
    """Poll the same preconfigured GT911 as the existing ESP-IDF firmware."""

    def __init__(self, i2c, display):
        self.i2c = i2c
        self.x = 0
        self.y = 0
        self.pressed = False
        self.indev = lv.indev_create()
        self.indev.set_type(lv.INDEV_TYPE.POINTER)
        self.indev.set_display(display)
        self.indev.set_read_cb(self.read)
        self.indev.enable(True)

    def read(self, _indev, data):
        try:
            status = self.i2c.readfrom_mem(_TOUCH, _STATUS, 1, addrsize=16)[0]
            if status & 0x80:
                count = status & 0x0F
                if 0 < count <= 5:
                    point = self.i2c.readfrom_mem(_TOUCH, _STATUS + 1, 8, addrsize=16)
                    self.x = min(WIDTH - 1, point[1] | point[2] << 8)
                    self.y = min(HEIGHT - 1, point[3] | point[4] << 8)
                    self.pressed = True
                else:
                    self.pressed = False
                self.i2c.writeto_mem(_TOUCH, _STATUS, b"\x00", addrsize=16)
        except OSError:
            self.pressed = False
        data.point.x = self.x
        data.point.y = self.y
        data.state = lv.INDEV_STATE.PRESSED if self.pressed else lv.INDEV_STATE.RELEASED
        data.continue_reading = False


class Board:
    """Keep the driver, buffers and input device alive for the event loop."""

    def __init__(self):
        if not lv.is_initialized():
            lv.init()

        # The V1.1 backlight controller is on I2C, not a GPIO/PWM pin.
        sda, scl = machine.Pin(15), machine.Pin(16)
        self.i2c = machine.I2C(0, sda=sda, scl=scl, freq=100000)
        addresses = self.i2c.scan()
        if _BACKLIGHT not in addresses:
            raise RuntimeError("CrowPanel backlight controller 0x30 not found")
        self.i2c.writeto(_BACKLIGHT, b"\x10")
        self.i2c.writeto(_BACKLIGHT, b"\x06")
        self.i2c.init(sda=sda, scl=scl, freq=400000)
        if _TOUCH not in self.i2c.scan():
            raise RuntimeError("GT911 at 0x5d not found")

        # Match the known-good V1.1 RGB timing and pin mapping.
        self.bus = lcd_bus.RGBBus(
            hsync=40, vsync=41, de=42, pclk=39,
            data0=21, data1=47, data2=48, data3=45,
            data4=38, data5=9, data6=10, data7=11,
            data8=12, data9=13, data10=14, data11=7,
            data12=17, data13=18, data14=3, data15=46,
            freq=16000000, hsync_pulse_width=4, hsync_back_porch=8,
            hsync_front_porch=8, vsync_pulse_width=4, vsync_back_porch=8,
            vsync_front_porch=8, pclk_active_low=True,
        )
        caps = lcd_bus.MEMORY_INTERNAL | lcd_bus.MEMORY_DMA
        self.buffer1 = self.bus.allocate_framebuffer(WIDTH * 20 * 2, caps)
        self.buffer2 = self.bus.allocate_framebuffer(WIDTH * 20 * 2, caps)
        if self.buffer1 is None or self.buffer2 is None:
            raise MemoryError("Could not allocate two 20-line LVGL DMA buffers")
        self.driver = rgb_display.RGBDisplay(
            data_bus=self.bus, display_width=WIDTH, display_height=HEIGHT,
            frame_buffer1=self.buffer1, frame_buffer2=self.buffer2,
            color_space=lv.COLOR_FORMAT.RGB565, rgb565_byte_swap=False,
        )
        self.driver.init()
        self.display = lv.display_get_default()
        if self.display is None:
            raise RuntimeError("LVGL RGB display initialization failed")
        self.touch = GT911(self.i2c, self.display)
