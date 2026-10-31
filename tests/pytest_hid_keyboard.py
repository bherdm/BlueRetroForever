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

    # A Consumer Control collection keeps every media key it lists
    assert rsp['hid_reports'][2]["report_id"] == 2
    assert len(rsp['hid_reports'][2]["usages"]) == 22
    assert rsp['hid_reports'][2]["usages"][0]["usage_page"] == 0x0C
    assert rsp['hid_reports'][2]["usages"][0]["usage"] == 0x1B1
    assert rsp['hid_reports'][2]["usages"][1]["usage"] == 0x223
    assert rsp['hid_reports'][2]["usages"][1]["bit_offset"] == 1


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


N64_KB = [[system.N64, dev_mode.KB, bt_conn_type.BT_BR_EDR]]

# The RANDnet read's codes: the RND-001's keys by their place on a Japanese keyboard. Keys it
# doesn't have, right Shift aside the right-hand modifiers among them, aren't sent
A, B, C, D, E, F = 0x0D07, 0x0708, 0x0508, 0x0507, 0x0601, 0x0607
LCTRL, LSHIFT, LALT, LGUI = 0x1107, 0x0E01, 0x1008, 0x0F07
RSHIFT = 0x0E06
HOME = 0x1010
MENU, KANA, HENKAN, MUHENKAN = 0x0B05, 0x1006, 0x0E02, 0x1002

# The extended read's codes (version 2, libdragon's keyboard.h): HID's own, Keyboard/Keypad usages
# as they are, Consumer usages from 0x1000 and Generic Desktop ones from 0x2000
H_A, H_B, H_C, H_D = 0x04, 0x05, 0x06, 0x07
H_MODS = list(range(0xE0, 0xE8))  # LCtrl LShift LAlt LGui RCtrl RShift RAlt RGui
H_HOME, H_INSERT, H_MENU, H_F13 = 0x4A, 0x49, 0x65, 0x68
H_KANA, H_HENKAN, H_MUHENKAN = 0x88, 0x8A, 0x8B


def kb_report(keys=(), mods=0):
    ''' A boot keyboard report: report ID 1, the modifier byte, a reserved byte, six keys. '''
    keys = list(keys) + [0] * (6 - len(keys))
    return f'a101{mods:02x}00' + ''.join(f'{key:02x}' for key in keys)


def ext(*codes):
    ''' The extended read's ten slots. '''
    return list(codes) + [0] * (10 - len(codes))


def assert_reads(rsp, keys, bitfield, ext_keys, ext_status):
    ''' The RANDnet read (0x13) and the extended read (0x2A) the N64 would get. '''
    assert rsp['wired_output']['keys'] == keys
    assert rsp['wired_output']['bitfield'] == bitfield
    assert rsp['wired_output']['ext_keys'] == ext_keys
    assert rsp['wired_output']['ext_status'] == ext_status


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_too_many_keys(blueretro):
    ''' The RANDnet read carries three keys: a fourth sets its "too many keys" bit (0x10), while
    the extended read carries all four. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    rsp = blueretro.send_hid_report(kb_report([0x04, 0x05, 0x06]))
    assert_reads(rsp, [A, B, C], 0, ext(H_A, H_B, H_C), 0)

    rsp = blueretro.send_hid_report(kb_report([0x04, 0x05, 0x06, 0x07]))
    assert_reads(rsp, [A, B, C], 0x10, ext(H_A, H_B, H_C, H_D), 0)

    rsp = blueretro.send_hid_report(kb_report())
    assert_reads(rsp, [0, 0, 0], 0, ext(), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_press_order(blueretro):
    ''' Keys keep the order they were pressed in, whatever their place in the report, and a
    released key leaves without moving the others. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    blueretro.send_hid_report(kb_report([0x06]))
    blueretro.send_hid_report(kb_report([0x04, 0x06]))
    rsp = blueretro.send_hid_report(kb_report([0x04, 0x05, 0x06]))
    assert_reads(rsp, [C, A, B], 0, ext(H_C, H_A, H_B), 0)

    rsp = blueretro.send_hid_report(kb_report([0x05, 0x06]))
    assert_reads(rsp, [C, B, 0], 0, ext(H_C, H_B), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_modifiers_first(blueretro):
    ''' The held modifiers come first, as their codes, in USB order, so Ctrl+Shift+A fits the
    RANDnet read's three slots. Of the right-hand ones, the RND-001 has only Shift. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    blueretro.send_hid_report(kb_report([0x04]))
    rsp = blueretro.send_hid_report(kb_report([0x04], mods=0x03))
    assert_reads(rsp, [LCTRL, LSHIFT, A], 0, ext(*H_MODS[:2], H_A), 0)

    rsp = blueretro.send_hid_report(kb_report(mods=0xF0))
    assert_reads(rsp, [RSHIFT, 0, 0], 0, ext(*H_MODS[4:]), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_home(blueretro):
    ''' Home is the RANDnet read's status bit 0x01 and takes no slot there; in the extended read
    it's HID's Home key. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    rsp = blueretro.send_hid_report(kb_report([0x4A, 0x04]))
    assert_reads(rsp, [A, 0, 0], 0x01, ext(H_HOME, H_A), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_jis_and_pc_keys(blueretro):
    ''' On the RANDnet read, Menu and the JIS conversion keys send their RND-001 codes, and keys the
    RND-001 doesn't have aren't sent. The extended read sends every one's usage. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    rsp = blueretro.send_hid_report(kb_report([0x65, 0x88, 0x8A]))
    assert_reads(rsp, [MENU, KANA, HENKAN], 0, ext(H_MENU, H_KANA, H_HENKAN), 0)

    rsp = blueretro.send_hid_report(kb_report([0x8B, 0x49, 0x68]))
    assert_reads(rsp, [MUHENKAN, 0, 0], 0, ext(H_MUHENKAN, H_INSERT, H_F13), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_ten_keys(blueretro):
    ''' The extended read carries ten keys in all, the modifiers among them, and sets 0x10 past
    ten. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    mods = [LCTRL, LSHIFT, LALT, LGUI, RSHIFT]

    rsp = blueretro.send_hid_report(kb_report([0x04, 0x05], mods=0xFF))
    assert_reads(rsp, mods[:3], 0x10, ext(*H_MODS, H_A, H_B), 0)

    rsp = blueretro.send_hid_report(kb_report([0x04, 0x05, 0x06, 0x07, 0x08, 0x09], mods=0xFF))
    assert_reads(rsp, mods[:3], 0x10, ext(*H_MODS, H_A, H_B), 0x10)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_rollover_error(blueretro):
    ''' A keyboard's own rollover error (usage 0x01 in every slot) sets 0x10 on both reads and
    keeps the last good keys. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    blueretro.send_hid_report(kb_report([0x04, 0x05]))
    rsp = blueretro.send_hid_report(kb_report([0x01] * 6))
    assert_reads(rsp, [A, B, 0], 0x10, ext(H_A, H_B), 0x10)

    rsp = blueretro.send_hid_report(kb_report())
    assert_reads(rsp, [0, 0, 0], 0, ext(), 0)


# HID_DESC with a System Control collection (report 3: Power, Sleep, Wake as bits) and a second
# Consumer Control collection of the array kind (report 4: two 16-bit slots)
HID_DESC_SYSTEM = HID_DESC + bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x80,        # Usage (System Control)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x03,        #   Report ID (3)
    0x19, 0x81,        #   Usage Minimum (System Power Down)
    0x29, 0x83,        #   Usage Maximum (System Wake Up)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x03,        #   Report Count (3)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    0x95, 0x05,        #   Report Count (5)
    0x81, 0x03,        #   Input (Const,Var,Abs)
    0xC0,              # End Collection
    0x05, 0x0C,        # Usage Page (Consumer)
    0x09, 0x01,        # Usage (Consumer Control)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x04,        #   Report ID (4)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x02,  #   Logical Maximum (0x2FF)
    0x19, 0x00,        #   Usage Minimum (Unassigned)
    0x2A, 0xFF, 0x02,  #   Usage Maximum (0x2FF)
    0x75, 0x10,        #   Report Size (16)
    0x95, 0x02,        #   Report Count (2)
    0x81, 0x00,        #   Input (Data,Array,Abs)
    0xC0,              # End Collection
])

# The extended read's Consumer and Generic Desktop codes
H_PLAY_PAUSE, H_VOLUME_UP, H_AC_HOME, H_0306 = 0x10CD, 0x10E9, 0x1223, 0x1306
H_POWER, H_SLEEP = 0x2081, 0x2082


def consumer_report(*bits):
    ''' Report 2, HID_DESC's 22 media keys a bit each: Play/Pause is bit 8, Volume Up bit 12,
    AC Home bit 1, usage 0x0306 bit 16. '''
    value = sum(1 << bit for bit in bits)
    return 'a102' + value.to_bytes(3, 'little').hex()


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_media_keys(blueretro):
    ''' A keyboard's media keys reach the extended read after its keys, in press order; the RND-001
    has none, so the RANDnet read doesn't send them. The generic bits keep the keys held while a
    media key changes. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    blueretro.send_hid_report(kb_report([0x04]))
    rsp = blueretro.send_hid_report(consumer_report(8))
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_PLAY_PAUSE), 0)
    assert rsp['generic_input']['btns'] == [1, 0, 0, 0]

    rsp = blueretro.send_hid_report(consumer_report(8, 12, 1))
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_PLAY_PAUSE, H_AC_HOME, H_VOLUME_UP), 0)

    rsp = blueretro.send_hid_report(consumer_report(16))
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_0306), 0)

    rsp = blueretro.send_hid_report(consumer_report())
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_system_keys(blueretro):
    ''' Power, Sleep and Wake come after the media keys, on the extended read only; a media report
    and a system report each leave the other's keys alone, and an array report names its keys by
    value. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC_SYSTEM)

    blueretro.send_hid_report(kb_report([0x04]))
    rsp = blueretro.send_hid_report('a10301')
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_POWER), 0)

    rsp = blueretro.send_hid_report('a104e9000000')
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_VOLUME_UP, H_POWER), 0)

    rsp = blueretro.send_hid_report('a10302')
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_VOLUME_UP, H_SLEEP), 0)

    rsp = blueretro.send_hid_report('a10300')
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A, H_VOLUME_UP), 0)

    rsp = blueretro.send_hid_report('a10400000000')
    assert_reads(rsp, [A, 0, 0], 0, ext(H_A), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_media_key_past_ten(blueretro):
    ''' A media key counts toward the extended read's ten. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    mods = [LCTRL, LSHIFT, LALT, LGUI, RSHIFT]

    blueretro.send_hid_report(kb_report([0x04, 0x05], mods=0xFF))
    rsp = blueretro.send_hid_report(consumer_report(8))
    assert_reads(rsp, mods[:3], 0x10, ext(*H_MODS, H_A, H_B), 0x10)


FB_TYPE_KB_LEDS = 7


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_leds(blueretro):
    ''' The console's LEDs reach the keyboard's LED output report as HID's bits, Num 0x01 to Kana
    0x10. The interrupt keeps the ones each read lights, the RANDnet read's Num and Caps or the
    extended read's five, which QEMU has no wire for. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    rsp = blueretro.send_fb(0, FB_TYPE_KB_LEDS, '03')
    assert rsp['bt_output'] == {'report_id': 1, 'data': [0x03]}

    rsp = blueretro.send_fb(0, FB_TYPE_KB_LEDS, '1f')
    assert rsp['bt_output'] == {'report_id': 1, 'data': [0x1F]}

    rsp = blueretro.send_fb(0, FB_TYPE_KB_LEDS, '04')
    assert rsp['bt_output'] == {'report_id': 1, 'data': [0x04]}


FULL_LAYOUT, CLASSIC = 0, 1
US, UK, DE, FR, JP = range(5)
MAC, PC = 0, 1
KANA, HENKAN, MUHENKAN, CAPS = 0x1006, 0x0E02, 0x1002, 0x0F05
AT, LPAREN, Z_KEY, CARET, LESS, HASH = 0x0504, 0x0806, 0x0D08, 0x0C06, 0x0902, 0x0605


def keyboard_cfg(blueretro, mode, layout, arrangement):
    ''' Port 1's keyboard settings: the output's 2 bytes, then the keyboard's 3. '''
    return blueretro.send_out_cfg(0, f"{dev_mode.KB:02x}00{mode:02x}{layout:02x}{arrangement:02x}")


def assert_classic(rsp, keys, bitfield=0):
    ''' The RND-001's read, and no extended read: classic mode is an RND-001. '''
    assert rsp['wired_output']['keys'] == keys
    assert rsp['wired_output']['bitfield'] == bitfield
    assert rsp['wired_output']['ext_marker'] == 0


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_translates_us(blueretro):
    ''' Classic, US PC: a key is sent as the RND-001 key that types the same character, with the
    Shift that key needs. Shift+2 is @, which JIS types right of P without Shift. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    keyboard_cfg(blueretro, CLASSIC, US, PC)

    assert_classic(blueretro.send_hid_report(kb_report([0x1F], mods=0x02)), [AT, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x26], mods=0x02)), [LSHIFT, LPAREN, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x04])), [A, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report()), [0, 0, 0])

    keyboard_cfg(blueretro, FULL_LAYOUT, US, PC)
    rsp = blueretro.send_hid_report(kb_report([0x1F], mods=0x02))
    assert rsp['wired_output']['keys'] == [LSHIFT, 0x0505, 0]
    assert rsp['wired_output']['ext_marker'] == 0x42


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_translates_german(blueretro):
    ''' Classic, German PC: Z is where US has Y, AltGr+Q is @, and ö, which the RND-001 can't
    type, is dropped. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    keyboard_cfg(blueretro, CLASSIC, DE, PC)

    assert_classic(blueretro.send_hid_report(kb_report([0x1C])), [Z_KEY, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x14], mods=0x40)), [AT, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x33])), [0, 0, 0])


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_stand_ins(blueretro):
    ''' The RND-001's かな, 変換 and 無変換: right Ctrl, right Windows and Menu on a PC; right
    Option, right Cmd and left Cmd on a Mac. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    keyboard_cfg(blueretro, CLASSIC, US, PC)
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x10)), [KANA, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x80)), [HENKAN, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x65])), [MUHENKAN, 0, 0])

    keyboard_cfg(blueretro, CLASSIC, US, MAC)
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x40)), [KANA, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x80)), [HENKAN, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x08)), [MUHENKAN, 0, 0])


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_mac_option(blueretro):
    ''' On a Mac, left Option types what AltGr does on a PC: Option+L is @ on German, and
    Option+3 is # on UK, which JIS types with Shift. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    keyboard_cfg(blueretro, CLASSIC, DE, MAC)
    assert_classic(blueretro.send_hid_report(kb_report([0x0F], mods=0x04)), [AT, 0, 0])
    keyboard_cfg(blueretro, CLASSIC, UK, MAC)
    assert_classic(blueretro.send_hid_report(kb_report([0x20], mods=0x04)), [LSHIFT, HASH, 0])


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_japanese(blueretro):
    ''' A Japanese keyboard's keys are the RND-001's own, so they go as they are. On a PC one there
    are no stand-ins; Apple's has かな and 英数, and its Cmd keys stand in for 変換 and 無変換. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    keyboard_cfg(blueretro, CLASSIC, JP, PC)
    assert_classic(blueretro.send_hid_report(kb_report([0x2F])), [AT, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x10)), [0, 0, 0])

    keyboard_cfg(blueretro, CLASSIC, JP, MAC)
    assert_classic(blueretro.send_hid_report(kb_report([0x90])), [KANA, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x91])), [CAPS, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report(mods=0x80)), [HENKAN, 0, 0])


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_classic_drops_media_keys(blueretro):
    ''' The RND-001 has no media keys. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    keyboard_cfg(blueretro, CLASSIC, US, PC)

    blueretro.send_hid_report(kb_report([0x04]))
    assert_classic(blueretro.send_hid_report(consumer_report(8)), [A, 0, 0])


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_apple_iso_swap(blueretro):
    ''' Apple's own ISO keyboards report the key above Tab and the one beside left Shift as each
    other: on a Mac ISO layout they're swapped back, in both modes. German Mac has ^ above Tab and
    < beside Shift. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_vid_pid(0x05AC, 0x0267)
    blueretro.send_hid_desc(HID_DESC)

    keyboard_cfg(blueretro, CLASSIC, DE, MAC)
    assert_classic(blueretro.send_hid_report(kb_report([0x64])), [CARET, 0, 0])
    assert_classic(blueretro.send_hid_report(kb_report([0x35])), [LSHIFT, LESS, 0])

    keyboard_cfg(blueretro, FULL_LAYOUT, DE, MAC)
    rsp = blueretro.send_hid_report(kb_report([0x64]))
    assert rsp['wired_output']['keys'] == [0x0D05, 0, 0]
    assert rsp['wired_output']['ext_keys'] == ext(0x35)

    # A PC arrangement leaves an Apple keyboard's keys as it reports them, and the RND-001 has no
    # key beside left Shift
    keyboard_cfg(blueretro, FULL_LAYOUT, DE, PC)
    rsp = blueretro.send_hid_report(kb_report([0x64]))
    assert rsp['wired_output']['keys'] == [0, 0, 0]
    assert rsp['wired_output']['ext_keys'] == ext(0x64)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_settings(blueretro):
    ''' The extended read carries the port's settings as 0x7B's byte: bits 0-2 the layout, bit 3
    the arrangement, bit 4 the mode. A new layout or arrangement shows at once; a new mode lets the
    port's keys go and silences it for a moment, so a program probes the keyboard again. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    keyboard_cfg(blueretro, FULL_LAYOUT, US, MAC)

    rsp = keyboard_cfg(blueretro, FULL_LAYOUT, DE, PC)
    assert rsp['kb_cfg_applied'] == {'port': 0, 'silenced': 0, 'ext_settings': DE | PC << 3}
    rsp = blueretro.send_hid_report(kb_report([0x04]))
    assert rsp['wired_output']['ext_settings'] == DE | PC << 3
    assert rsp['wired_output']['ext_marker'] == 0x42

    rsp = keyboard_cfg(blueretro, CLASSIC, DE, PC)
    assert rsp['kb_cfg_applied'] == {'port': 0, 'silenced': 1, 'ext_settings': DE | PC << 3 | CLASSIC << 4}

    rsp = keyboard_cfg(blueretro, FULL_LAYOUT, JP, MAC)
    assert rsp['kb_cfg_applied'] == {'port': 0, 'silenced': 1, 'ext_settings': JP}

    # The same settings again change nothing
    rsp = keyboard_cfg(blueretro, FULL_LAYOUT, JP, MAC)
    assert 'kb_cfg_applied' not in rsp


# Apple's Fn (Globe), usage 0x03 on Apple's vendor page. Apple's aluminium Bluetooth keyboard has it
# beside Eject in a report of its own (report 0x11, a Consumer Control collection, page 0x00FF)
HID_DESC_KB_ONLY = HID_DESC[:HID_DESC.index(0xC0) + 1]
HID_DESC_APPLE_EJECT_FN = HID_DESC_KB_ONLY + bytes([
    0x05, 0x0C,        # Usage Page (Consumer)
    0x09, 0x01,        # Usage (Consumer Control)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x11,        #   Report ID (0x11)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x03,        #   Report Count (3)
    0x81, 0x01,        #   Input (Const)
    0x95, 0x01,        #   Report Count (1)
    0x09, 0xB8,        #   Usage (Eject)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    0x06, 0xFF, 0x00,  #   Usage Page (0x00FF, Apple's vendor page)
    0x09, 0x03,        #   Usage (Fn)
    0x81, 0x02,        #   Input (Data,Var,Abs)
    0x95, 0x03,        #   Report Count (3)
    0x81, 0x01,        #   Input (Const)
    0xC0,              # End Collection
])
# Others carry Fn in the keyboard report, in the byte after the modifiers (here on page 0xFF00)
HID_DESC_APPLE_FN_IN_KB = HID_DESC_KB_ONLY.replace(
    bytes([0x95, 0x01, 0x75, 0x08, 0x81, 0x03]),
    bytes([0x95, 0x01, 0x75, 0x01, 0x06, 0x00, 0xFF, 0x09, 0x03, 0x81, 0x02, 0x75, 0x07, 0x81, 0x03]))
H_FN, H_EJECT = 0x3003, 0x10B8
F1 = 0x0B01


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_apple_fn_beside_eject(blueretro):
    ''' Apple's Fn reaches the extended read as 0x3003, after the media and system keys. The RANDnet
    read has no Fn. Eject, in the same report, is a media key. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_vid_pid(0x05AC, 0x0255)
    blueretro.send_hid_desc(HID_DESC_APPLE_EJECT_FN)

    blueretro.send_hid_report(kb_report([0x04], mods=0x02))
    rsp = blueretro.send_hid_report('a11110')
    assert_reads(rsp, [LSHIFT, A, 0], 0, ext(H_MODS[1], H_A, H_FN), 0)

    rsp = blueretro.send_hid_report('a11118')
    assert_reads(rsp, [LSHIFT, A, 0], 0, ext(H_MODS[1], H_A, H_EJECT, H_FN), 0)

    rsp = blueretro.send_hid_report('a11108')
    assert_reads(rsp, [LSHIFT, A, 0], 0, ext(H_MODS[1], H_A, H_EJECT), 0)

    rsp = blueretro.send_hid_report('a11100')
    assert_reads(rsp, [LSHIFT, A, 0], 0, ext(H_MODS[1], H_A), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_apple_fn_in_keyboard_report(blueretro):
    ''' Fn in the keyboard report itself, on another of Apple's vendor pages. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_vid_pid(0x05AC, 0x0267)
    blueretro.send_hid_desc(HID_DESC_APPLE_FN_IN_KB)

    rsp = blueretro.send_hid_report('a1010001' + '3a' + '00' * 5)
    assert_reads(rsp, [F1, 0, 0], 0, ext(0x3A, H_FN), 0)

    rsp = blueretro.send_hid_report('a1010000' + '3a' + '00' * 5)
    assert_reads(rsp, [F1, 0, 0], 0, ext(0x3A), 0)


@pytest.mark.parametrize('blueretro', N64_KB, indirect=True)
def test_hid_keyboard_n64_fn_only_from_apple(blueretro):
    ''' Another maker's vendor page is its own: the same usage there isn't Fn. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_vid_pid(0x045E, 0x0001)
    blueretro.send_hid_desc(HID_DESC_APPLE_FN_IN_KB)

    rsp = blueretro.send_hid_report('a1010001' + '3a' + '00' * 5)
    assert_reads(rsp, [F1, 0, 0], 0, ext(0x3A), 0)
