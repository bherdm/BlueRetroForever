# Bluetooth keyboards on the N64 (design, 2026-10-30)

The extended read is now version 2, sending HID's codes untranslated: see "Version 2 of the
extended read" at the end. What this design says of 0x2A's codes, bytes and LEDs is version 1's.

## The goal

The user's words: support every key on any of the keyboard layouts we've discussed, including all
media keys, for the homebrew community.
- **Layouts:** US ANSI, UK ISO, German ISO (QWERTZ), French ISO (AZERTY) and Japanese JIS.
- **Better RANDnet support is a must,** and it's a feature of the same firmware: libdragon's
  keyboard module, its code table and the extended read.
- **This design is the N64 only.** The GameCube may follow next week and the Dreamcast in about
  three months, each designed then.

## The shape

**The user's rulings (2026-10-30):**
- **Command 0x2A, the extended read, is the new system for homebrew.** It's built from the
  keyboard's own state in HID keycodes. BlueRetro translates those into libdragon's codes, so the
  N64 translates nothing. The user's reasoning: BlueRetro's hardware does the translation better
  than the N64 can, and that saves the game's time.
- **Command 0x13, the emulated RANDnet keyboard, gets libdragon's fixes to its table.**
- **No layout setting.** Both reads send key positions, and the program picks the layout through
  libdragon.

**libdragon's call (2026-10-30), at the user's request: 0x13 is built from the keyboard's state
too.**
- **The change:** the user had ruled 0x13 onto the generic path, the button space every input is
  converted into. On that path four RND-001 keys can't reach it, since the space's 118 keys are full
  and have no room for Menu, Kana, Henkan or Muhenkan. The path also has no press order.
- **One builder, two lengths:** 0x13 is 0x2A's ordered list, with Home taken out as a status bit
  and cut to three. That's less code than a second table, and the two reads can never disagree
  about a key's code.
- **The generic path keeps everything else:** a keyboard on a gamepad output, BlueRetro's combos
  and its macros. Only the N64 keyboard mode's 0x13 moves.

## 1. Keyboard state, per Bluetooth device

```c
struct kb_state {
    uint8_t mods;         /* the eight modifiers, USB order: LCtrl LShift LAlt LGui RCtrl RShift RAlt RGui */
    uint8_t keys[10];     /* Keyboard/Keypad usages held, in press order; 0 = empty */
    uint8_t key_cnt;
    uint8_t overflow;     /* more keys held than keys[] holds, or the keyboard's own rollover error */
    uint16_t consumer[4]; /* Consumer-page usages held (0x0C), in press order */
    uint8_t system;       /* Generic Desktop system keys held: bit 0 Power (0x81), 1 Sleep (0x82), 2 Wake (0x83) */
    uint8_t leds;         /* the console's last LED byte, in HID order: Num, Caps */
};
```

That's about 24 bytes for each of the seven Bluetooth devices.

**Building it:**
- **Keyboard/Keypad report:** the modifier byte plus the key array or bitmap, both forms
  `hid_kb_to_generic` already reads. New keys are appended and released ones removed, so `keys[]`
  stays in press order. Usages 0x01-0x03 (rollover and error codes) set `overflow` and leave the
  last good keys in place.
- **Consumer and system reports:** a new report type, `KB_EXT`, for a keyboard device's
  consumer-page and system-control reports. Today `hid_report_fingerprint` types them
  `REPORT_NONE` and `bt_host_bridge` drops them. They're typed `KB_EXT` only on a device that also
  has a `KB` report, so a gamepad's consumer usages (Home, Back) keep their path.
- **Apple's Fn/Globe** comes on Apple's vendor page. It's per-model handling, done once the rest is
  in.

The generic conversion still runs beside it.

## 2. The translation, on BlueRetro

The table is libdragon's (`randnet-keyboard` branch, `include/keyboard.h` and
`src/joybus/keyboard_usage.c`).
- **Keyboard/Keypad page:** one 256-entry table.
  - The RND-001's 85 keys send their matrix codes by their place on a Japanese keyboard: invert
    libdragon's list.
  - Every other usage sends `0x2000 | usage`. RCtrl, AltGr and RGui are 0x20E4, 0x20E6 and
    0x20E7.
- **Consumer page:** `((0x21 + (usage >> 8)) << 8) | (usage & 0xFF)`.
- **System keys:** 0x2481-0x2483. Apple Fn/Globe will be 0x2503.
- **No code with a zero byte:** libdragon drops a 0x2A reply whole if a code has one. Only consumer
  usages 0x0100 and 0x0200 would make one (0x2200 and 0x2300). Neither is a key, so the firmware
  drops them.

It runs when a report arrives, so both replies are ready before the console asks.

**What it fixes over today's 0x13**, from libdragon's walks:
- Minus and Backslash are no longer swapped.
- Page Up and Page Down get their matrix codes, and Insert and the keypad keys get page codes.
- Menu, Kana, Henkan and Muhenkan arrive from keyboards that have them.
- RCtrl, AltGr and RGui send their own codes, not Kana, Henkan and Muhenkan.
- HID 0x32 and 0x35 no longer share 0x0D05.

## 3. The ordered list, and the two replies

**The list,** built from `kb_state` with the table:
1. the held modifiers, as their codes (LCtrl 0x1107, LShift 0x0E01, LAlt 0x1008, LGui 0x0F07,
   RCtrl 0x20E4, RShift 0x0E06, RAlt 0x20E6, RGui 0x20E7);
2. the Keyboard/Keypad keys in press order, Home as 0x1010;
3. the consumer and system keys.

Putting the modifiers first keeps Ctrl+S and Shift+letter readable in three slots.

**Command 0x2A** (libdragon f6e95e4a, `joybus_cmd_keyboard_extended_read_port_t` in
`src/joybus/joybus_commands.h`):
- **The bytes:** 23 in all. Marker 0x42, version 1, status, then ten big-endian codes, 0 for none.
- **Slots:** the list's first ten.
- **Status 0x10:** more than ten codes are in the list, or the keyboard reports its own rollover
  error. libdragon then keeps the last keys it had.
- **The probe:** libdragon sends 0x2A when a keyboard connects. A reply with the marker means 0x2A
  from then on. Firmware from before 0x2A answers with its identity, and a real RND-001 doesn't
  answer, three times over. Either means 0x13.

**Command 0x13:**
- **The bytes:** three big-endian codes and the status byte, as an RND-001 answers.
- **Slots:** the list with Home taken out, cut to three.
- **Status:** 0x01 while Home is held, and 0x10 when more than three codes remain, or on the
  keyboard's rollover error.
- **Order:** libdragon compares each read's keys as a set and ignores a read with 0x10 set, so
  order never decides what it keeps. Press order is for programs that read 0x13 themselves.

**Both replies** sit in the output buffer, built ahead, since the console allows no time to compute
one on the wire. A Controller Pak read already answers 33 bytes, so 23 fits, and two extended reads
fit one Joybus block.

## 4. LEDs back to the Bluetooth keyboard

The 0x13 and 0x2A commands carry the console's LED byte, in RANDnet's bits: Num 0x01, Caps 0x02,
Power 0x04.
- **Mapping:** Num and Caps become HID's Num (bit 0) and Caps (bit 1), and Power is dropped.
  Passed through raw, Power would light the keyboard's Scroll Lock, and libdragon lights Power by
  default.
- **Sending:** a change becomes a HID output report to the keyboard. On Classic that's DATA|OUTPUT
  on the interrupt channel; on BLE, the output report's characteristic.

Today BlueRetro ignores the byte, so Caps Lock's LED can't light.

## What it costs

- **The N64:** nothing past libdragon's own module, which is linked only into programs that call
  `keyboard_init`. It reads codes as they arrive and never translates one.
- **Firmware RAM:** about 24 bytes a device for `kb_state`, plus one more report slot (`KB_EXT`).
- **Firmware code:**
  - the state builder;
  - the 512-byte translation table, `const`, so in flash;
  - the list builder, with the 0x2A and 0x13 replies cut from it;
  - the `KB_EXT` fingerprint;
  - the LED output report.
- **Per report:** one table read per key, on the ESP32.
- **Configs:** none changed.
- **Dropped:** today's RCtrl/AltGr/RGui route to Kana, Henkan and Muhenkan, which a Randnet Disk
  program on a non-JIS keyboard used. A JIS keyboard brings the real keys, and the goal is
  homebrew.

## How it's tested

- **QEMU** (hardware-free):
  - 0x13: each layout's sample keys, modifiers first, press order, Home's bit and 0x10;
  - 0x2A: ten slots and 0x10, a media key, a system key, and the zero-byte drop;
  - LEDs out.
  - `wired_output` logs the 0x13 codes already, and the 0x2A reply is logged the same way.
- **A simulated Bluetooth keyboard:**
  - **Layouts:** all five are there (`us`, `uk`, `de`, `fr` and `jp`), with `kb_walk_*` and
    `kb_text_*`.
  - **Media keys:** not yet. It needs a consumer report in its keyboard's report map, and media-key
    steps.
  - **LEDs:** it should log the LED output reports it receives.
- **A real N64:** KeyboardTest-N64 on libdragon's module, after each step.

## The order of work

1. **`kb_state`, the table and the list,** from the keyboard report, with the 0x2A and 0x13
   replies cut from it.
2. **Media and system keys (`KB_EXT`)** into `kb_state`, with a simulated keyboard's media keys to test them.
3. **LEDs out** to the Bluetooth keyboard.

## What happens to the work so far

- **Carried over** (on `claude/kb-iso-jis`, not flashed):
  - the serial commands (58f1a4d);
  - the ISO and JIS keys past HID 0x63, for the generic path, so for a keyboard on a gamepad output
    (6125ad1);
  - the first-report fix (6125ad1).
- **Replaced:** 6125ad1's excess-keys bit in `n64_kb_from_generic` gives way to the list builder.
  Its QEMU test carries over, with expectations for the new order.
- **Not needed:** the widening of the generic button space (dropped).

## Version 2 of the extended read (2026-10-31)

libdragon serves full layout only, and reads only 0x2A. The firmware follows libdragon's plan, on
the user's rulings:
- **Codes are HID's, untranslated:**
  - Keyboard/Keypad usages as they are, 0x0004 to 0x00E7, the modifiers 0xE0 to 0xE7 among them.
  - Consumer (media) usages are 0x1000 | the usage, every one below 0x1000.
  - Generic Desktop system keys are 0x2000 | the usage: Power 0x2081, Sleep 0x2082, Wake 0x2083.
  - Apple's Fn/Globe is 0x3003: usage 0x03 on Apple's vendor page, which an Apple keyboard (VID
    0x05AC) puts on page 0x00FF, 0xFF00 or 0xFF01, as Linux's hid-apple finds it. It's read from
    the keyboard report or from a report of its own (beside Eject on Apple's aluminium Bluetooth
    keyboard). Another maker's vendor page is let go. Tested in QEMU only: the keyboard simulator presents no
    Apple keyboard.
- **Order and overflow:** the modifiers, then the keys in press order, then the media, system and
  Apple keys, as libdragon's keyboard.h documents. More than ten fills the ten slots and sets 0x10.
- **The bytes:** 24 in all. Marker 0x42, version 2, status, the port's settings byte (0x7B's,
  docs/n64-commands-design.md), then ten big-endian codes, 0 for none.
- **The settings byte** follows any settings change at once. Its mode bit is always 0 here, since
  a classic port answers 0x2A with its identity. Bit 7 is set while a program's settings until
  power off (0x7B) sit over the saved ones.
- **LEDs:** 0x2A's LED byte is HID's five bits, Num 0x01 to Kana 0x10, passed through. 0x13's
  keeps the RND-001's: Num and Caps reach the keyboard, Power stays on the console.
- **A mode change** lets the port's keys go and leaves the port silent for 1.5 s, longer than
  libdragon's once-a-second identify. libdragon sees the keyboard leave, and probes it again when
  it's back. The port still answers 0x7B while silent.
- **0x13 sends only the RND-001's keys:** in full layout, by position; in classic, translated. A
  key the RND-001 doesn't have, a media key and a system key aren't sent, and the old page codes
  (0x2000 | usage, libdragon's Consumer and Desktop pages) are gone: nothing reads them, and they
  were never a standard (libdragon's answer, 2026-10-31). libdragon no longer sends
  0x13.

**Proven on a real N64** (2026-10-31, libdragon dev a597ac0b, KeyboardTest 210cc91; the first
hardware run of version 2):
- **Walks:** every key arrived as its own HID usage, in order: US 96/96, UK 97/97, DE 97/97, FR
  97/97, JIS 101/101.
- **Rollover:** ten held keys (four modifiers, six keys) in order; fourteen held gave the first ten
  and 0x10.
- **Beyond the RND-001:** F13-F24, keypad = and ",", LANG1 and LANG2, Menu, 20 media keys (two at
  once), Power, Sleep and Wake.
- **LEDs:** HID's bits reached the keyboard.
- **Settings:** 0x05 and 0x06 through libdragon's 0x7B module. A layout change until power off
  showed in keyboard_get_layout within a few frames, with no reconnect. A switch to classic dropped
  the keyboard after the silence, and back to full layout it came back as US Mac.
- **The N64's cost:** 23-27 µs a frame warm, 47-49 µs cold, with a keyboard connected; nothing
  without one.
- **Not covered:** Apple's Fn, which the keyboard simulator can't send.
