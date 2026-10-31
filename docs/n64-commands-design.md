# Commands from the N64 (design, 2026-10-30)

## The goal

A ROM can ask BlueRetroForever to open and close pairing, say what's connected, disconnect a
device, and set how bright its LEDs get. It can do that from any port, in any mode.

- **No libdragon change:** programs send the command through libdragon's raw Joybus call,
  `joybus_exec_cmd`.
- **The verbs** (the user's list, 2026-10-30):
  1. Open pairing, close pairing: the board's button, and the serial `inq start` and `inq stop`.
  2. Status: each port empty, gamepad, keyboard or mouse; whether pairing is open; the protocol
     version.
  3. Disconnect one port's device, or all of them.
  4. The LEDs' brightness cap. A level, never off.
  5. Later: mappings and configs, once the rest of the stack matures. Not in this design.

## On the wire

**One command, 0x7B, a verb after it:**
- **Send, 4 bytes:** the command ID, the verb, then two argument bytes.
- **Reply, 10 bytes, the same for every verb:**

| Byte | Meaning |
| --- | --- |
| 0 | Marker 0x42 |
| 1 | Protocol version, 1 |
| 2 | The verb, echoed |
| 3 | Result: 0 accepted, 1 unknown verb, 2 bad argument |
| 4 | Flags: bit 0 pairing open |
| 5 | The LED brightness level |
| 6-9 | Each port: 0 empty, 1 gamepad, 2 keyboard, 3 mouse |

**Verbs:**

| Verb | Name | Arguments |
| --- | --- | --- |
| 0x00 | Status | none |
| 0x01 | Open pairing | none |
| 0x02 | Close pairing | none |
| 0x03 | Disconnect | port 0-3, or 0xFF for all |
| 0x04 | LED brightness | a level from 1 (0 is refused: off isn't allowed) |

**How the board answers:**
- **Any port, any mode:** the N64 driver's interrupt handles the command before it picks the
  port's mode, so a gamepad, mouse or keyboard port all answer it.
- **Built ahead:** the console allows no time to compute a reply on the wire. The host task keeps
  the status block current, and the interrupt copies it, adding the echoed verb and the result.
- **Actions happen after the reply:** the interrupt queues the verb to the system task, the way
  rumble is queued today. So the reply shows the state before the action, and a ROM reads status
  again to see pairing open or a device gone.
- **Old firmware:** it answers an unknown command with the port's identity (3 bytes, no marker),
  so a ROM knows the commands aren't there.

**What's left out on purpose:** factory reset, reboot, and writing mappings or configs. A ROM with
a bug shouldn't be able to wipe someone's setup.

## LED brightness

The user's dev board has two LEDs on the board itself, a blue one (every BlueRetro has it) and a
green one, and no port LEDs. Dimming must not strobe an LED, so PWM is out.

**Today's blue LED** runs from a PWM channel: off is 0%, solid (an error) is 100%, which is
steady, and pairing breathes between about 1% and 24%, which is PWM.

**Ways to dim it without PWM:**
- **The pad's drive strength** (`gpio_set_drive_capability`): four steps, about 5, 10, 20 and
  40 mA at most. They change the brightness only where the board's series resistor lets more
  current through than the weakest step allows. That's unknown without the schematic, so it has
  to be tried.
- **The internal pull-up alone:** the pin as an input with its pull-up (about 45 kΩ) lights an
  active-high LED with tens of microamps. That's a faint, steady glow, the most subtle level
  there is.
- **A bigger series resistor:** hardware, and not the firmware's to do.

**Pairing without PWM** would blink slowly at the chosen level, not breathe.

**The verb's level** would be one of those steps (1 the pull-up glow, then the drive strengths).
It's saved to NVS, so it survives a power cycle.

**The green LED** is driven by nothing in the firmware, unless it's on one of hw1's port LED pins
(2, 4, 12 or 15). If it's lit whenever the board has power, it's a power LED, and only hardware
can change it.

## What it costs

- **The N64:** nothing in libdragon. A program sends 4 bytes and reads 10.
- **The firmware:**
  - one case in the N64 interrupt;
  - a 10-byte status block the host task refreshes when a device connects or leaves, or pairing
    opens or closes;
  - a verb queue to the system task, which calls what the serial commands already call.

## How it's tested

- **QEMU:** there's no Joybus wire there. The verb handler and the status block are tested through
  the test harness, by a new command that hands a verb to the handler.
- **A real N64:** a small test ROM of its own sends each verb, and a simulated keyboard pairs when pairing opens. The
  status then shows its keyboard on port 1, and disconnect drops it.

## The user's rulings (2026-10-30)

1. **The command ID is 0x7B,** far from anything Nintendo or a known accessory uses.
2. **An LED level set from the console is saved,** to NVS, as the board's setting.
3. **The keyboard option** is reading (a). Console-native makes 0x13 suit RANDnet software on any
   keyboard; joybus is key positions as they are, for homebrew. It gets a design of its own,
   after the commands.
4. **The test ROM is a small ROM of its own.**
5. **The green LED is lit whenever the board has power:** a power LED, which only hardware can
   dim.
6. **Pairing blinks slowly** at the chosen level.

## Keyboard settings from a ROM (2026-10-31)

The user asked for this: ROMs read and change each port's keyboard settings (mode, layout,
arrangement; docs/classic-keyboard-design.md). N64FlashcartMenu is the first
planned user. This replaces "Later: mappings and configs" for the keyboard settings only; button mappings
stay out.

**Two verbs join 0x7B.** Nothing with 0x7B has shipped, and result 1 (unknown verb) already tells a
ROM a verb is missing, so the protocol stays version 1.

| Verb | Name | Arguments | Reply bytes 6-9 |
| --- | --- | --- | --- |
| 0x05 | Keyboard settings | none | each port's settings byte |
| 0x06 | Set keyboard settings | arg0: port 0-3, or 0xFF for all; arg1: a settings byte | the devices, as status |

**The settings byte:**

| Bits | Meaning |
| --- | --- |
| 0-2 | Layout: 0 US, 1 UK, 2 DE, 3 FR, 4 JP |
| 3 | Arrangement: 0 Mac, 1 PC |
| 4 | Mode: 0 full layout, 1 classic |
| 5-6 | Reserved, 0 |
| 7 | Set: until power off, not saved. Read: the port's setting isn't the saved one |

**How the board answers** (built on `claude/kb-iso-jis`, tested in QEMU):
- **Read:** the interrupt copies the status block as now, then four bytes from a second DRAM
  block that the system task refreshes from each port's settings as they apply.
- **Set:** the interrupt refuses a port past the fourth, a layout above 4 or a reserved bit with
  result 2, and queues the verb. The system task writes the settings and saves them as BlueRosetta
  and serial `kb` do (`config_update`). With bit 7, it keeps them until power off instead, over the
  saved ones (`kb_cfg_temp` in `adapter/config.h`), and doesn't save. Saving a port's settings
  again, from a ROM, BlueRosetta or serial `kb`, ends its settings until power off.
- **The settings apply at once:** the 0x2A reply's settings byte changes, and a new mode silences
  the port for 1.5 s.
- **The reply shows the state before the change,** as for every verb. A ROM reads again to see it.

**A program that's running when its port's mode changes:** libdragon probes 0x2A only when a
keyboard connects. So a mode change, from a ROM, BlueRosetta or serial `kb`, leaves the port silent
for 1.5 s, and libdragon probes the keyboard again when it's back (docs/keyboard-design.md, version
2). The port still answers 0x7B while silent.

**What it costs:**
- **The firmware:** two if/else branches in the IRAM handler, four bytes of DRAM, and a case in
  the system task. Measured with 0x2A's version 2 and the silence: 236 bytes of IRAM, which leaves
  the universal build 638. The save is a flash write, as BlueRosetta's are, made by the system
  task, never the interrupt.
- **The N64:** nothing in libdragon. A program sends 4 bytes and reads 10 through
  `joybus_exec_cmd`.
- **Risk:** a ROM with a bug can set a port's keyboard wrong, never wipe a setup. BlueRosetta,
  serial `kb` or the menu sets it right again.

**How it's tested:**
- **QEMU** (`tests/pytest_console_cmd.py`): read gives the defaults; a set shows at a later read,
  and on the config file unless bit 7 was set; bad layouts and reserved bits are refused; 0xFF sets
  every port.
- **The test ROM** (`tests/n64_console_cmd`) sends both verbs.
- **A real N64:** KeyboardTest with a simulated keyboard, a port switched to classic by the test ROM, then the 0x2A
  probe's fallback after the keyboard reconnects.

**The user's ruling (2026-10-31):** the ROM chooses, and saved is the default. Bit 7 of a set
keeps the setting until power off. A menu's settings page saves; a menu switching a port to classic
for one game, a 64DD disk image of RANDnet software say, leaves the user's saved choice standing.

**Re-probing after a mode change, ruled (2026-10-31):** the
port stays silent for 1.5 s, so libdragon sees a reconnect. No libdragon call.

**The settings byte** is also in 0x2A's version 2 reply, so libdragon reads the layout and
arrangement live (docs/keyboard-design.md).
