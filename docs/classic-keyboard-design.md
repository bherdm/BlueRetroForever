# The classic keyboard mode (design, 2026-10-31)

## The goal

The user's words: any user with any Bluetooth keyboard should be able to fully use RANDnet
software and homebrew software that may use any layout or key. So a keyboard connects in one of
two modes (the user's rulings, 2026-10-30 and 2026-10-31):

- **full-layout** (it was "joybus"), today's behaviour: every key is sent as its position, on both
  reads. Homebrew reads the extended read (0x2A) through libdragon.
- **classic:** the keyboard plays the RND-001 for retail RANDnet software, which expects a
  Japanese keyboard with kana input on the same keys. Only 0x13 is answered. A program probing
  0x2A gets the identity reply, as from a real RND-001, and falls back to 0x13.

This design is the N64 only.

## What a non-Japanese keyboard can't do in RANDnet software today

1. **Keys it doesn't have:** Kana (かな), Henkan (変換), Muhenkan (無変換), Ro (ろ, the _ key) and
   Yen (¥). On a JIS keyboard they're HID 0x88, 0x8A, 0x8B, 0x87 and 0x89. Zenkaku/Hankaku sits
   where a US keyboard has `, so that one is there.
2. **Symbols in other places:** RANDnet software reads keys as JIS positions. On a US keyboard,
   Shift+2 is @, but the RND-001 types " there, so a US user gets " for @. German and French
   keyboards move letters as well: Z and Y swap, and A, Q, M and W move.

## The design

### 1. Substitutes for the missing keys

In classic mode, three keys that every layout has, and that RANDnet software has no use
for, send the missing ones. Which three is question 3. The old firmware's choice was RCtrl → Kana,
AltGr → Henkan and RGui → Muhenkan, but AltGr is needed for characters on German and French
keyboards.

### 2. Characters, translated (question 2)

With the keyboard's layout set (us, uk, de, fr or jp), each press is read as the character it
prints on that layout. BlueRetro then sends the RND-001 key, with or without Shift, that types the
same character.

- **US Shift+2 (@)** is sent as the RND-001's @ key, the one right of P, without Shift. Shift is
  left out of the slots while that key is held.
- **German Z** is sent as the RND-001's Z key, wherever it sits on the German keyboard.
- **A character the RND-001 can't type** (ä ö ü ß € é è à ç ù) is dropped.
- **Kana input:** letters stay letters, so on US and UK keyboards each key's kana is the JIS one.
  On German and French keyboards the kana follow the letter, not the place.
- **Keys that type no character** (arrows, F-keys, Enter and so on) and the substitutes are sent as
  their RND-001 keys, as now.

The layout table is the same data a keyboard simulator's layouts hold: each layout's
character to (key, modifiers), inverted.

### 3. The setting

- **In the firmware:** each output port's config gains a keyboard section: the mode (full-layout
  or classic), the layout (us, uk, de, fr or jp) and the arrangement (PC or Mac). That's config
  version 4, migrated from version 3, and new ports default to full-layout, US and Mac.
- **In BlueRosetta:** the Settings screen's output ports gain a keyboard section with three
  pickers: mode, layout and arrangement. The output
  characteristic takes the old 2-byte form and the new one, so an older app still works.
- **Where it applies:** in classic mode, 0x13 is translated and 0x2A isn't answered. In
  full-layout mode, both send positions, as now.

### What it costs

- **The N64:** nothing.
- **The firmware:**
  - per layout, a character table of about 100 entries;
  - the RND-001's character table;
  - the translation step in the 0x13 builder, with Shift withheld;
  - the config's version 4 and its migration.
- **BlueRosetta:** two pickers per output port, and the longer output config.

### How it's tested

- **QEMU:** for each layout, the sample text's keys go in and the 0x13 codes come out, as the
  RND-001 would type the same text. The substitutes are covered, and an untypable character is
  dropped.
- **A real N64:** a simulated Bluetooth keyboard types each layout's sample text in classic mode, and KeyboardTest-N64 shows the
  RND-001 codes. There's no 64DD, so RANDnet software itself can't be run: QEMU and KeyboardTest
  are the proof.

## The user's rulings (2026-10-31)

1. **The setting is per output port,** as the device mode is.
2. **Pending:** whether characters are translated (my recommendation is below).
3. **Pending:** which three keys stand in for Kana, Henkan and Muhenkan. This depends on 2.
4. **No extended read in classic mode.** A keyboard is either the classic retro keyboard, for
   retail software, or full-layout.
5. **There's no 64DD:** QEMU and KeyboardTest-N64 are the proof.

## The user's rulings on the setting (2026-10-31)

- **Each port has a keyboard section:** the mode, the layout and the arrangement.
- **The mode** is full-layout (default) or classic.
- **The layout** is one of US (default), UK, German, French or Japanese.
- **The arrangement** is PC or Mac (default Mac). It's the user's choice, with no detection,
  because the symbols differ per platform. Apple's UK layout has @ on Shift+2 like US, and its
  German and French layouts type their extra characters with Option combinations that aren't
  Windows' AltGr ones. So each language has a PC table and a Mac table.
- **Apple's own ISO keyboards** report the key above Tab (§) and the one next to left Shift
  (< >) as each other, which macOS and Linux undo. In both modes, when the arrangement is Mac, the
  keyboard's vendor ID is Apple's (0x05AC) and the layout is ISO (UK, German or French), the
  firmware swaps them back, so even full-layout positions are right.
- **Translation:** the layout picker means characters are translated, as recommended (2a).

## The stand-in keys (the user's ruling, 2026-10-31)

In classic mode, the keys a keyboard lacks come from these:

| Arrangement | かな (Kana) | 変換 (Henkan) | 無変換 (Muhenkan) | Notes |
| --- | --- | --- | --- | --- |
| PC | Right Ctrl | Right Windows | Menu | AltGr keeps typing characters |
| Mac | Right Option | Right ⌘ | Left ⌘ | Characters that need Option come from left Option |

- **A PC Japanese keyboard** has every RND-001 key, so it needs no stand-ins.
- **Apple's Japanese keyboards** have かな and 英数: かな sends Kana, and 英数 sends 英数, which is
  Caps Lock's place on a JIS keyboard. Their ⌘ keys stand in for 変換 and 無変換, as above.

## The keyboard section on the wire

Each output's settings grow from 2 bytes to 5, as the BLE output characteristic carries them:

| Byte | Meaning | Values (0 is the default) |
| --- | --- | --- |
| 0 | Device | as now |
| 1 | Accessory | as now |
| 2 | Keyboard mode | 0 full-layout, 1 classic |
| 3 | Layout | 0 US, 1 UK, 2 German, 3 French, 4 Japanese |
| 4 | Arrangement | 0 Mac, 1 PC |

- **Zero is every default,** so version 3's ports migrate as full-layout, US and Mac by
  zero-filling.
- **Old firmware** answers 2 bytes, so the app shows no keyboard settings for it.
- **An old app** writes 2 bytes, and the firmware keeps the port's keyboard bytes. Today
  `att_cfg.c`'s output write copies a whole `out_cfg` whatever length was written, so with 5 bytes
  it needs a length check, or a 2-byte write fills the keyboard bytes from past its end.
- **The console the adapter drives** is read with the config command 0x08: two bytes, the system
  as the System list numbers it and the answer's version. A universal build, which takes swappable
  cables, answers Auto (0) until the console first talks. Older firmware answers the ABI version's
  one byte. BlueRosetta shows the keyboard section when it's the N64 (BlueRosetta-App 8cb85a2).
- **BlueRosetta** has the keyboard section (BlueRosetta-App 4575699, 6987f79, 8cb85a2): three pickers per port, and
  in classic mode the stand-in keys for the chosen layout and arrangement. It hides the section
  on 2-byte firmware and never writes 5 bytes to it.

## My recommendation on 2 and 3

**2: translate, with the layout as a per-port setting that defaults to US.**
- **Why translate:** retail RANDnet software knows only the JIS layout. Without translation, a US,
  UK, German or French user gets the wrong symbols, and has to know where JIS puts them. That isn't
  full use.
- **The cost:** a user with a non-US keyboard sets its layout once, in BlueRosetta. A US user
  changes nothing.
- **What it gives homebrew:** the command's status block (0x7B, version 2) can carry each port's
  layout too. A full-layout program can then read the layout from the adapter instead of asking.
  That's the thought it spares homebrew developers.

**3: yes, it depends on 2.** With translation, AltGr types characters on UK, German and French
keyboards (€, @, {), so it can't stand in for a key. That makes it (b): RCtrl for Kana, RGui for
Henkan and Menu for Muhenkan. Without translation, AltGr is free, and (a) works too.

## Where it stands (2026-10-31)

Built and proven on the dev board in a real N64 (hw1 universal), firmware 4a7bf8e..8f6096b:
- **Config version 4** keeps the keyboard settings as their own array (`kb_cfg`), apart from
  `out_cfg`, because the wired drivers' interrupts index `out_cfg` and the universal build's IRAM
  is full. The BLE characteristic still carries 5 bytes per output. The board upgraded from
  version 3 on its first boot.
- **Classic mode:** the translation tables are generated from xkeyboard-config 2.46
  (`tools/gen_n64_kb_layouts.py`, from `ckbcomp` keymaps).
- **Tested on a real N64** with KeyboardTest-N64 on libdragon f6e95e4a:
  - the 0x2A probe fell back to 0x13 (rollover 3);
  - US PC: Shift+2 gave the RND-001's @ with no Shift, and Shift+9 gave Shift+8, its (;
  - German PC: the Z key gave Z, AltGr+Q gave @, and ö gave nothing;
  - German Mac: Option+L gave @;
  - the stand-ins for かな, 変換 and 無変換 on both arrangements.
- **The serial command** `kb <port> <full|classic> <layout> <arrangement>` sets a port from a host.
- **IRAM:** `led.c` moved to flash, which left the universal build 874 bytes of IRAM.
- **Not yet:** an app reading or writing the settings over BLE against a real adapter, and Mac
  layouts in the keyboard simulator (the Mac cases were typed as raw keys).
