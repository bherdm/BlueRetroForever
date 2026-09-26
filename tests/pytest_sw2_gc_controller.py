''' Tests for the Switch 2 NSO GameCube controller, which speaks its own protocol over BLE.

The adapter brings the controller up through a sequence of requests, each answered by an ack
notification; the tests answer them the way the controller does, then feed input reports. The
phantom-stick fixes are covered here: reports are held until the calibration is loaded, an
error ack neither advances the sequence nor feeds it garbage, implausible calibration falls back
to the defaults, and the triggers' idle noise is inside a deadzone.
'''
from itertools import islice
import numpy as np
import pytest
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from device_data.sw2 import sw2_gc_btns_mask, sw2_gc_axes, sw2_gc_trigger
from device_data.br import system, dev_mode, bt_conn_type, axis, bt_type, bt_subtype
from device_data.gc import gc_axes


DEVICE_NAME = 'DeviceName'  # what a Switch 2 controller answers to a name request

ACK_HDL = 0x001A
REPORT_HDL = 0x000A
CMD_READ_SPI, SUBCMD_READ_SPI = 0x02, 0x04
CMD_SET_LED, SUBCMD_SET_LED = 0x09, 0x07
CMD_PAIRING = 0x15
PAIRING_STEPS = (0x01, 0x04, 0x02, 0x03)  # the acks, in the order the adapter asks
TYPE_RSP, TYPE_ERR = 0x01, 0x00
INT_BLE = 0x01
VID_NINTENDO, PID_NSO_GC = 0x057E, 0x2073
USER_CALIB_MAGIC = 0xA1B2
PRE_CALIB_REPORT_LIMIT = 180

BLE_GC_PAD = [[system.GC, dev_mode.PAD, bt_conn_type.BT_LE]]


def pack12(first, second):
    ''' Two 12-bit values in three bytes, low nibbles first, as the controller packs them. '''
    return bytes([first & 0xFF, ((first >> 8) & 0xF) | ((second & 0xF) << 4), (second >> 4) & 0xFF])


def stick_calib(x_neutral, y_neutral, x_max, y_max, x_min, y_min):
    ''' The nine bytes of one stick's calibration: centres, then the reach above and below. '''
    return pack12(x_neutral, y_neutral) + pack12(x_max, y_max) + pack12(x_min, y_min)


def ack(cmd, subcmd, value=b'', kind=TYPE_RSP):
    ''' An ack notification: command, type, interface, subcommand, then a 64-byte value. '''
    return bytes([cmd, kind, INT_BLE, subcmd]).hex() + value.ljust(64, b'\0').hex()


def info_value(vid=VID_NINTENDO, pid=PID_NSO_GC):
    ''' The controller's identity, where the adapter reads it. '''
    value = bytearray(64)
    value[30:32] = vid.to_bytes(2, 'little')
    value[32:34] = pid.to_bytes(2, 'little')
    return bytes(value)


def factory_calib_value(calib):
    ''' A factory calibration read: the nine bytes sit at offset 52; erased flash reads 0xFF. '''
    value = bytearray(b'\xff' * 64)
    if calib:
        value[52:61] = calib
    return bytes(value)


def user_calib_value(left=None, right=None):
    ''' A user calibration read: a magic word ahead of each stick's nine bytes when it is set. '''
    value = bytearray(b'\xff' * 64)
    if left:
        value[12:14] = USER_CALIB_MAGIC.to_bytes(2, 'little')
        value[14:23] = left
    if right:
        value[44:46] = USER_CALIB_MAGIC.to_bytes(2, 'little')
        value[46:55] = right
    return bytes(value)


def report(buttons=0, lx=0x800, ly=0x800, rx=0x800, ry=0x800, lt=None, rt=None):
    ''' A type 1 input report, 63 bytes as the adapter reads it. '''
    idle = sw2_gc_trigger['neutral']
    data = bytearray(63)
    data[4:8] = buttons.to_bytes(4, 'little')
    data[10:13] = pack12(lx, ly)
    data[13:16] = pack12(rx, ry)
    data[60] = idle if lt is None else lt
    data[61] = idle if rt is None else rt
    return data.hex()


def settle(blueretro):
    ''' Two reports at rest: the adapter bridges nothing before its second report from a device. '''
    for _ in range(2):
        blueretro.send_att_notify(REPORT_HDL, report())


def bring_up(blueretro, left=None, right=None, user_left=None, user_right=None):
    ''' Answer the adapter's bring-up as the controller would, through to reports flowing.

    Returns the ack reply that carried the calibration the adapter settled on.
    '''
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_type'] == bt_type.SW2
    assert rsp['device_name']['device_subtype'] == bt_subtype.SUBTYPE_DEFAULT

    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, info_value()))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, b'\0' * 12 + b'\xaa' * 16))
    for step in PAIRING_STEPS:
        blueretro.send_att_notify(ACK_HDL, ack(CMD_PAIRING, step))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, b'\0' * 12 + b'\xbb' * 16))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, factory_calib_value(left)))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, factory_calib_value(right)))
    rsp = blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, user_calib_value(user_left, user_right)))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_SET_LED, SUBCMD_SET_LED))
    settle(blueretro)
    return rsp


def trigger_expectation(raw):
    ''' What a raw trigger value becomes on the wire, by the adapter's own arithmetic. '''
    src_max = sw2_gc_trigger['abs_max']
    deadzone = int(np.single(0.0135 * src_max)) + sw2_gc_trigger['deadzone']
    scale = np.single(gc_axes[axis.LM]['abs_max'] / (src_max - deadzone))
    generic = raw - sw2_gc_trigger['neutral']
    mapped = int(np.single((generic - deadzone) * scale)) if generic > deadzone else 0
    return generic, mapped, mapped + gc_axes[axis.LM]['neutral']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_reports_wait_for_the_calibration(blueretro):
    ''' A report that arrives before the calibration is loaded is held back, not bridged. '''
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_type'] == bt_type.SW2

    rsp = blueretro.send_att_notify(REPORT_HDL, report())
    assert 'wireless_input' not in rsp
    assert 'wired_output' not in rsp


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_reports_flow_once_the_calibration_never_comes(blueretro):
    ''' A controller whose calibration reads keep failing still works, on the defaults. '''
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_type'] == bt_type.SW2
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, info_value()))

    for _ in range(PRE_CALIB_REPORT_LIMIT):
        rsp = blueretro.send_att_notify(REPORT_HDL, report())
        assert 'wireless_input' not in rsp

    # The next report opens the gate, on the defaults; the adapter then wants its two at rest.
    rsp = blueretro.send_att_notify(REPORT_HDL, report())
    assert rsp['type_update']['device_type'] == bt_type.SW2
    settle(blueretro)

    rsp = blueretro.send_att_notify(REPORT_HDL, report())
    assert rsp['wireless_input']['axes'] == [0x800, 0x800, 0x800, 0x800]
    for ax in islice(axis, 0, 4):
        assert rsp['generic_input']['axes'][ax] == 0
        assert rsp['wired_output']['axes'][ax] == gc_axes[ax]['neutral']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_error_ack_is_ignored(blueretro):
    ''' An error ack neither advances the bring-up nor feeds it garbage as calibration. '''
    left = stick_calib(0x7B0, 0x830, 0x4B0, 0x4A0, 0x480, 0x4C0)
    right = stick_calib(0x810, 0x7F0, 0x460, 0x470, 0x450, 0x440)
    garbage = stick_calib(0x123, 0x456, 0x100, 0x100, 0x100, 0x100)

    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_type'] == bt_type.SW2
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, info_value()))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, b'\0' * 12 + b'\xaa' * 16))
    for step in PAIRING_STEPS:
        blueretro.send_att_notify(ACK_HDL, ack(CMD_PAIRING, step))
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, b'\0' * 12 + b'\xbb' * 16))

    # The left factory calibration read fails: an error ack, carrying whatever was in the buffer.
    rsp = blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, factory_calib_value(garbage), kind=TYPE_ERR))
    assert 'calib_data' not in rsp

    # Then it succeeds, and the rest follows: the calibration must be the real one.
    blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, factory_calib_value(left)))
    rsp = blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, factory_calib_value(right)))
    assert rsp['calib_data']['neutral'] == [0x7B0, 0x830, 0x810, 0x7F0]
    assert rsp['calib_data']['rel_max'] == [0x4B0, 0x4A0, 0x460, 0x470]
    assert rsp['calib_data']['rel_min'] == [0x480, 0x4C0, 0x450, 0x440]

    rsp = blueretro.send_att_notify(ACK_HDL, ack(CMD_READ_SPI, SUBCMD_READ_SPI, user_calib_value()))
    assert rsp['calib_data']['neutral'] == [0x7B0, 0x830, 0x810, 0x7F0]
    blueretro.send_att_notify(ACK_HDL, ack(CMD_SET_LED, SUBCMD_SET_LED))
    settle(blueretro)

    # A stick resting at its calibrated centre is at rest on the wire.
    rsp = blueretro.send_att_notify(REPORT_HDL, report(lx=0x7B0, ly=0x830, rx=0x810, ry=0x7F0))
    for ax in islice(axis, 0, 4):
        assert rsp['generic_input']['axes'][ax] == 0
        assert rsp['wired_output']['axes'][ax] == gc_axes[ax]['neutral']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_implausible_calibration_falls_back_to_defaults(blueretro):
    ''' A centre far from mid-range is not honoured: the stick is scaled on the defaults. '''
    left = stick_calib(0x200, 0x200, 0x4B0, 0x4A0, 0x480, 0x4C0)
    rsp = bring_up(blueretro, left=left)
    assert rsp['calib_data']['neutral'][0:2] == [0x200, 0x200]

    rsp = blueretro.send_att_notify(REPORT_HDL, report())
    assert rsp['wireless_input']['axes'] == [0x800, 0x800, 0x800, 0x800]
    for ax in islice(axis, 0, 4):
        assert rsp['generic_input']['axes'][ax] == 0
        assert rsp['wired_output']['axes'][ax] == gc_axes[ax]['neutral']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_default_buttons_mapping(blueretro):
    ''' Press each button and check the default mapping. '''
    bring_up(blueretro)

    for sw2_btns, br_btns in btns_generic_test_data(sw2_gc_btns_mask):
        rsp = blueretro.send_att_notify(REPORT_HDL, report(buttons=sw2_btns))
        assert rsp['wireless_input']['btns'] == sw2_btns
        assert rsp['generic_input']['btns'][0] == br_btns


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_axes_default_scaling(blueretro):
    ''' With no usable calibration, the sticks scale on the defaults. '''
    bring_up(blueretro)

    for axes in axes_test_data_generator(sw2_gc_axes, gc_axes, 0.0135):
        rsp = blueretro.send_att_notify(REPORT_HDL, report(
            lx=axes[axis.LX]['wireless'], ly=axes[axis.LY]['wireless'],
            rx=axes[axis.RX]['wireless'], ry=axes[axis.RY]['wireless']))

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_axes_scaling_with_calibration(blueretro):
    ''' With the controller's calibration loaded, the sticks scale on it. '''
    left = stick_calib(0x7B0, 0x830, 0x4B0, 0x4A0, 0x480, 0x4C0)
    right = stick_calib(0x810, 0x7F0, 0x460, 0x470, 0x450, 0x440)
    rsp = bring_up(blueretro, left=left, right=right)

    calib = rsp['calib_data']
    sw2_calib_axes = {}
    for ax in islice(axis, 0, 4):
        sw2_calib_axes[ax] = {
            'neutral': calib['neutral'][ax], 'abs_max': calib['rel_max'][ax],
            'abs_min': calib['rel_min'][ax], 'deadzone': calib['deadzone'][ax],
        }

    for axes in axes_test_data_generator(sw2_calib_axes, gc_axes, 0.0135):
        rsp = blueretro.send_att_notify(REPORT_HDL, report(
            lx=axes[axis.LX]['wireless'], ly=axes[axis.LY]['wireless'],
            rx=axes[axis.RX]['wireless'], ry=axes[axis.RY]['wireless']))

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_user_calibration_needs_its_magic(blueretro):
    ''' A user calibration counts only where its magic word is set; the factory one stands otherwise. '''
    left = stick_calib(0x7B0, 0x830, 0x4B0, 0x4A0, 0x480, 0x4C0)
    right = stick_calib(0x810, 0x7F0, 0x460, 0x470, 0x450, 0x440)
    user_right = stick_calib(0x820, 0x7E0, 0x400, 0x410, 0x420, 0x430)
    rsp = bring_up(blueretro, left=left, right=right, user_right=user_right)
    assert rsp['calib_data']['neutral'] == [0x7B0, 0x830, 0x820, 0x7E0]
    assert rsp['calib_data']['rel_max'] == [0x4B0, 0x4A0, 0x400, 0x410]


@pytest.mark.parametrize('blueretro', BLE_GC_PAD, indirect=True)
def test_sw2_gc_triggers_idle_noise_is_inside_the_deadzone(blueretro):
    ''' Triggers idling a few counts above neutral stay at rest; a real press comes through. '''
    bring_up(blueretro)
    idle = sw2_gc_trigger['neutral']
    deadzone = int(np.single(0.0135 * sw2_gc_trigger['abs_max'])) + sw2_gc_trigger['deadzone']

    for raw in (idle, idle + 3, idle + 7, idle + deadzone):
        rsp = blueretro.send_att_notify(REPORT_HDL, report(lt=raw, rt=raw))
        generic, mapped, wired = trigger_expectation(raw)
        for ax in (axis.LM, axis.RM):
            assert rsp['generic_input']['axes'][ax] == generic
            assert rsp['mapped_input']['axes'][ax] == 0
            assert rsp['wired_output']['axes'][ax] == gc_axes[ax]['neutral']

    for raw in (idle + deadzone + 2, idle + 100, idle + sw2_gc_trigger['abs_max']):
        rsp = blueretro.send_att_notify(REPORT_HDL, report(lt=raw, rt=raw))
        generic, mapped, wired = trigger_expectation(raw)
        assert mapped > 0
        for ax in (axis.LM, axis.RM):
            assert rsp['generic_input']['axes'][ax] == generic
            assert rsp['mapped_input']['axes'][ax] == mapped
            assert rsp['wired_output']['axes'][ax] == wired
