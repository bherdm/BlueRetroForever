''' Tests for BlueRetroForever's own command from a console's program (0x7B,
docs/n64-commands-design.md): the reply is the status block, and the verb runs after it. '''
from pytest_hid_keyboard import DEVICE_NAME, HID_DESC


STATUS, PAIRING_OPEN, PAIRING_CLOSE, DISCONNECT, LED_LEVEL = range(5)
OK, UNKNOWN_VERB, BAD_ARG = range(3)
DEV_NONE, DEV_PAD, DEV_KB, DEV_MOUSE = range(4)


def test_console_cmd_status(blueretro):
    ''' The reply opens with the marker 0x42 and version 1, echoes the verb, and says it was
    accepted. '''
    rsp = blueretro.send_console_cmd(STATUS)
    assert rsp['console_reply'][:4] == [0x42, 1, STATUS, OK]


def test_console_cmd_refuses(blueretro):
    ''' An unknown verb, a port past the fourth and an LED level off the scale are refused, and
    nothing runs. '''
    assert blueretro.send_console_cmd(0x09)['console_reply'][3] == UNKNOWN_VERB
    assert blueretro.send_console_cmd(DISCONNECT, 4)['console_reply'][3] == BAD_ARG
    for level in (0, 6):
        assert blueretro.send_console_cmd(LED_LEVEL, level)['console_reply'][3] == BAD_ARG


def test_console_cmd_led_level(blueretro):
    ''' The LED level is set after the reply, so the next status shows it. There is no off. '''
    rsp = blueretro.send_console_cmd(LED_LEVEL, 3)
    assert rsp['console_reply'][3] == OK
    assert blueretro.send_console_cmd(STATUS)['console_reply'][5] == 3

    blueretro.send_console_cmd(LED_LEVEL, 2)
    assert blueretro.send_console_cmd(STATUS)['console_reply'][5] == 2


def test_console_cmd_pairing(blueretro):
    ''' Opening pairing shows in the next status's flags, and closing it clears them. '''
    blueretro.send_console_cmd(PAIRING_OPEN)
    assert blueretro.send_console_cmd(STATUS)['console_reply'][4] & 0x01

    blueretro.send_console_cmd(PAIRING_CLOSE)
    assert not blueretro.send_console_cmd(STATUS)['console_reply'][4] & 0x01


def test_console_cmd_keyboard_on_port_1(blueretro):
    ''' A keyboard on BlueRetro's first output shows as a keyboard on port 1. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    blueretro.send_hid_report('a1010000040000000000')

    rsp = blueretro.send_console_cmd(STATUS)
    assert rsp['console_reply'][6:10] == [DEV_KB, DEV_NONE, DEV_NONE, DEV_NONE]
