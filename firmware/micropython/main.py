"""Entry point: upload all four .py files to an LVGL-MicroPython build."""

import time
import lvgl as lv
from board import Board
from ui import App


def run():
    # Explicit references keep the drivers, buffers and event callbacks alive.
    board = Board()
    app = App(board.display)
    previous = time.ticks_ms()
    while True:
        now = time.ticks_ms()
        delta = time.ticks_diff(now, previous)
        previous = now
        if delta > 0:
            lv.tick_inc(delta)
        lv.task_handler()
        time.sleep_ms(5)


if __name__ == "__main__":
    run()
