''' Tests for generic HID keyboard. '''
import pytest
from device_data.br import system, dev_mode, bt_conn_type


DEVICE_NAME = 'HID Keyboard'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x06,        # Usage (Keyboard)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x01,        #   Report ID (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x08,        #   Report Count (8)
    0x05, 0x07,        #   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        #   Usage Minimum (Keyboard LeftControl)
    0x29, 0xE7,        #   Usage Maximum (Keyboard Right GUI)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x08,        #   Report Size (8)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,        #   Report Count (5)
    0x75, 0x01,        #   Report Size (1)
    0x05, 0x08,        #   Usage Page (LED)
    0x19, 0x01,        #   Usage Minimum (Num Lock)
    0x29, 0x05,        #   Usage Maximum (Kana)
    0x91, 0x02,        #   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x03,        #   Report Size (3)
    0x91, 0x03,        #   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x06,        #   Report Count (6)
    0x75, 0x08,        #   Report Size (8)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x05, 0x07,        #   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        #   Usage Minimum (Undefined)
    0x29, 0xFF,        #   Usage Maximum (0xFF)
    0x81, 0x00,        #   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              # End Collection
    0x05, 0x0C,        # Usage Page (Consumer)
    0x09, 0x01,        # Usage (Consumer Control)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x02,        #   Report ID (2)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x16,        #   Report Count (22)
    0x0A, 0xB1, 0x01,  #   Usage (AL Screen Saver)
    0x0A, 0x23, 0x02,  #   Usage (AC Home)
    0x0A, 0xAE, 0x01,  #   Usage (AL Keyboard Layout)
    0x0A, 0x8A, 0x01,  #   Usage (AL Email Reader)
    0x09, 0x40,        #   Usage (Menu)
    0x09, 0x6F,        #   Usage (Display Brightness Increment)
    0x0A, 0x21, 0x02,  #   Usage (AC Search)
    0x09, 0xB6,        #   Usage (Scan Previous Track)
    0x09, 0xCD,        #   Usage (Play/Pause)
    0x09, 0xB5,        #   Usage (Scan Next Track)
    0x09, 0xE2,        #   Usage (Mute)
    0x09, 0xEA,        #   Usage (Volume Decrement)
    0x09, 0xE9,        #   Usage (Volume Increment)
    0x09, 0x30,        #   Usage (Power)
    0x0A, 0x83, 0x01,  #   Usage (AL Consumer Control Configuration)
    0x0A, 0x24, 0x02,  #   Usage (AC Back)
    0x0A, 0x06, 0x03,  #   Usage (0x0306)
    0x0A, 0x08, 0x03,  #   Usage (0x0308)
    0x0A, 0x01, 0x03,  #   Usage (0x0301)
    0x0A, 0x83, 0x01,  #   Usage (AL Consumer Control Configuration)
    0x0A, 0x0A, 0x03,  #   Usage (0x030A)
    0x09, 0x70,        #   Usage (Display Brightness Decrement)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x02,        #   Report Size (2)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              # End Collection

    # 147 bytes
])


def test_hid_keyboard_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'HID Keyboard'
    
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]["report_id"] == 1
    assert rsp['hid_reports'][0]["report_tag"] == 0
    assert rsp['hid_reports'][0]["usages"][6]["bit_offset"] == 56
    assert rsp['hid_reports'][0]["report_type"] == 0

    assert rsp['hid_reports'][1]["report_id"] == 1
    assert rsp['hid_reports'][1]["report_tag"] == 1

    assert rsp['hid_reports'][2]["report_id"] == 2
    assert rsp['hid_reports'][2]["usages"][0]["usage_page"] == 0x0C
    assert rsp['hid_reports'][2]["usages"][0]["usage"] == 0x223


def test_hid_keyboard_iso_and_jis_keys(blueretro):
    ''' The ISO key left of Z (HID 0x64) and the JIS ro and yen keys (0x87, 0x89) reach their
    generic keys, KB_102ND, KB_RO and KB_YEN (generic bits 115-117); a usage no generic key
    carries (0x88, katakana/hiragana) presses nothing, KB_A included. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    for usage, generic in ((0x64, 115), (0x87, 116), (0x89, 117)):
        rsp = blueretro.send_hid_report(f'a1010000{usage:02x}0000000000')
        assert rsp['generic_input']['btns'] == [0, 0, 0, 1 << (generic - 96)]

    rsp = blueretro.send_hid_report('a101000088' + '0000000000')
    assert rsp['generic_input']['btns'] == [0, 0, 0, 0]


def test_hid_keyboard_first_report_arrives(blueretro):
    ''' A keyboard's very first report is a key: it reaches the adapter, where other devices'
    first two reports are let go while they settle. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    rsp = blueretro.send_hid_report('a1010000040000000000')
    assert rsp['generic_input']['btns'] == [1, 0, 0, 0]


@pytest.mark.parametrize('blueretro', [[system.N64, dev_mode.KB, bt_conn_type.BT_BR_EDR]], indirect=True)
def test_hid_keyboard_n64_too_many_keys(blueretro):
    ''' The RANDnet report carries three keys: a fourth held sets the keyboard's "too many keys"
    status bit (0x10), and the three slots keep the first three in generic order. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # A, B, C: three slots, no status bit (KB_A 0x070D, KB_B 0x0807, KB_C 0x0805)
    rsp = blueretro.send_hid_report('a1010000040506000000')
    assert rsp['wired_output']['keys'] == [0x070D, 0x0807, 0x0805]
    assert rsp['wired_output']['bitfield'] == 0

    # A, B, C, D: D comes second in generic order (KB_D 0x0705), C is the fourth
    rsp = blueretro.send_hid_report('a1010000040506070000')
    assert rsp['wired_output']['keys'] == [0x070D, 0x0705, 0x0807]
    assert rsp['wired_output']['bitfield'] == 0x10

    rsp = blueretro.send_hid_report('a1010000000000000000')
    assert rsp['wired_output']['keys'] == [0, 0, 0]
    assert rsp['wired_output']['bitfield'] == 0
