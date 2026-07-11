# N64 port assignment, power, and keyboard notes (as-verified 2026-07-10; updated 2026-07-11)

Operational findings from putting a Bluetooth keyboard (8BitDo Retro Keyboard, BLE,
VID `0x2DC8` PID `0x5200`) on a real N64's **port 1** through this firmware (hw1, `n64` config),
driven by the Port fleet. Duplicated in the fleet workspace
(`BlueRetroForever Build-Debug Hand — Design.md` §9.3); this is the firmware-side home.

## Which N64 port does a device land on?

**Output port = connection-order slot.** `bt_host_reset_dev()` sets `out_idx = dev_id`
(`main/bluetooth/host.c:563`), and `bt_host_get_new_dev()` hands out the lowest free slot. There is
**no config/browser remap of a device's output port in this fork** — and the N64 keyboard path
(`DEV_KB`) deliberately bypasses the `in_cfg` mapping tables (`main/adapter/adapter.c`, raw
scancodes must pass through).

Consequences:

* A **combo keyboard that enumerates as two HID interfaces/bonds** can occupy two slots: the
  non-typing interface (media/consumer) squats slot 0 → N64 port 1 (identifies as a keyboard but
  sends no key reports), pushing the real keys to port 2.
* The fix is **bond hygiene, not firmware**: a clean power-cycle deletes stored link keys on boot
  (`bt_hci_cmd_delete_stored_link_key` in the boot sequence), so re-pairing lands the real keyboard
  interface on slot 0 → port 1.
* `CONFIG_BLUERETRO_N64_AUTO_ID_SWITCHING` (default `y`) then makes port 1 identify as a RandNET
  keyboard (`0x0002`) while the keyboard is connected — presence-driven, per
  `main/bluetooth/host.c:299-329` + `n64_runtime`.

## Power sensitivity (console-powered boards)

A dev board whose N64 side is console-powered can be **crashed by cutting console power** —
observed once after rapid automated deploy/power-cycle loops: solid LEDs, unresponsive buttons,
and the console reading `0xFFFF` (disconnected) on **all four ports**. Not a blanket rule (normal
power-cycles are usually fine); the suspected sensitive window is mid-flash/busy states.

* **Recovery:** console off → unplug/replug the board's USB → console on.
* **Prevention when the board must stay paired:** load ROMs without cutting power — stage to the
  flashcart and use the console's RESET button (soft NMI) instead of a power-cycle.
* **State discriminator:** all four ports `0xFFFF` = the firmware isn't driving the joybus at all
  (crashed, or no BT devices connected — the N64 side idles dark with zero devices); a live idle
  adapter reads `Controller (0x0500)`.

### The board is two half-independent failure domains (verified 2026-07-11)

A single console power-cycle (the "usually fine" case above) latched the **CH340 USB bridge**
(powered by the host PC's USB 5 V) while the **ESP32** (powered by the N64 rail) later recovered on
its own with a console outlet cycle — the halves crash and recover **separately**, and their
health signals differ: the console's keyboard-identify says how the ESP32 half is doing; the host's
serial-device presence says how the CH340 half is doing. They can legitimately disagree (keyboard
typing fine while the board is unflashable, and vice versa).

**Recovery ladder, in cost order:**
1. Host-side USB re-enumeration (`echo 0/1 > /sys/bus/usb/devices/usbN/authorized`) — only helps a
   device still willing to enumerate; a latched CH340 is absent from `lsusb` entirely.
2. Console outlet off/on — resets the ESP32 half (its rail), does **nothing** for a latched CH340.
3. **Physical USB cable replug — the only thing that cuts a latched CH340's VBUS.** Expect the
   device node to renumber afterwards.

Diagnose at the bus (`lsusb` for `1a86:7523`, `dmesg | grep ch34`), not from a derived
"board present" signal.

## RandNET keycode endianness (for anyone decoding captures)

`n64_kb_scancode[]` (`main/adapter/wired/n64.c`) stores codes **byteswapped** so the little-endian
ESP32 puts them on the joybus wire big-endian-correct. A big-endian reader on the console (e.g. the
KeyboardTest-N64 ROM, or Port's `pc64_kbd`) therefore prints `byteswap(table value)`:
A = table `0x070D` → wire `0x0D07`; Caps Lock = `0x050F` → `0x0F05`. Decode by byteswapping the
observed value and looking it up in the table. A capture that decodes to readable English is
itself proof the table + endianness handling are right.

**The table is half a contract (2026-07-11):** a console-side decoder maps the wire values back to
characters, so an edit here must ship with the matching decoder row or the key silently types the
wrong character. The endianness rework (`975f38dc`) hand-moved `KB_MINUS` onto `KB_BACKSLASH`'s
code (`0x0410`) and `|\` typed as `-_` until `592ff2c` gave backslash its own `0x0605`
(wire `0x0506`). **Known latent twin, deliberately left:** `KB_GRAVE ≡ KB_HASH` (both `0x050D`) —
harmless on ANSI keyboards (`KB_HASH` is the non-US `#`/`~` key, HID usage `0x32`), wrong on
ISO/UK layouts. Note the QEMU pytest suite does **not** assert N64 wired scancodes (only HID
descriptor parsing), so it stays green across this bug class — validate table edits with a decoded
on-console capture or typed text.

## USB-serial identity (host side)

This dev board's UART bridge is a **CH340** (`1a86:7523`) — don't grep for FTDI (the SC64 on the
same host is the FTDI `0403:6014`). The node **renumbers on replug** (`/dev/ttyUSB0` →
`/dev/ttyUSB1`): re-detect before flashing rather than trusting a pinned path. Console UART logs
at **115200** (`sdkconfig.defaults`).

Corollary (2026-07-11): any host-side "is the board attached?" check must verify the **vendor id**,
not just glob `/dev/ttyUSB*` — when the SC64's `ftdi_sio` driver happens to hold a node, a name
glob "finds the board" on the flashcart's port, and an esptool auto-detect flash would aim at the
wrong device. Match `1a86` (or read `idVendor` via sysfs) before flashing.
