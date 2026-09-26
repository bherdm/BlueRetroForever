''' Switch 2 controllers, which speak their own protocol over BLE. '''
from enum import IntEnum
from bit_helper import bit
from device_data.br import axis


class sw2_btns(IntEnum):
    ''' Bits of the 32-bit buttons word in a type 1 report. '''
    Y = 0
    X = 1
    B = 2
    A = 3
    R_SR = 4
    R_SL = 5
    R = 6
    ZR = 7
    MINUS = 8
    PLUS = 9
    RJ = 10
    LJ = 11
    HOME = 12
    CAPTURE = 13
    C = 14
    UNKNOWN = 15
    DOWN = 16
    UP = 17
    RIGHT = 18
    LEFT = 19
    L_SR = 20
    L_SL = 21
    L = 22
    ZL = 23
    GR = 24
    GL = 25


# The NSO GameCube controller: indexed by BlueRetro's generic button, the controller's bit.
sw2_gc_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(sw2_btns.LEFT), bit(sw2_btns.RIGHT), bit(sw2_btns.DOWN), bit(sw2_btns.UP),
    0, 0, 0, 0,
    bit(sw2_btns.B), bit(sw2_btns.X), bit(sw2_btns.A), bit(sw2_btns.Y),
    bit(sw2_btns.PLUS), bit(sw2_btns.C), bit(sw2_btns.HOME), bit(sw2_btns.CAPTURE),
    0, bit(sw2_btns.ZL), bit(sw2_btns.L), 0,
    0, bit(sw2_btns.ZR), bit(sw2_btns.R), 0,
]

# What the adapter assumes of the sticks when the controller's calibration isn't usable.
sw2_gc_axes = {
    axis.LX: {'neutral': 0x800, 'abs_max': 1225, 'abs_min': 1225, 'deadzone': 0},
    axis.LY: {'neutral': 0x800, 'abs_max': 1225, 'abs_min': 1225, 'deadzone': 0},
    axis.RX: {'neutral': 0x800, 'abs_max': 1120, 'abs_min': 1120, 'deadzone': 0},
    axis.RY: {'neutral': 0x800, 'abs_max': 1120, 'abs_min': 1120, 'deadzone': 0},
}

# The analog triggers: eight-bit, idling around 30, with the static deadzone the adapter applies.
sw2_gc_trigger = {'neutral': 30, 'abs_max': 195, 'deadzone': 9}
