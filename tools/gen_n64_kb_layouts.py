"""Turn ckbcomp console keymaps (xkeyboard-config 2.46) into BlueRetroForever's classic-mode
tables: per layout, the character each key types at each level, and for JIS, the key and Shift
that type each character. Only characters the RND-001 can type are kept."""
import re, sys
# usage: gen_n64_kb_layouts.py DIR > main/adapter/wired/n64_kb_layouts.h's tables, where DIR holds
# <layout>_<pc|mac>.map from "ckbcomp -layout <layout> [-variant mac]" (console-setup) for us, gb,
# de, fr and jp
D = sys.argv[1]
# Linux keycodes of the printing keys, to HID Keyboard/Keypad usages
KC = {**{2 + i: 0x1E + i for i in range(10)}, 12: 0x2D, 13: 0x2E,
      16: 0x14, 17: 0x1A, 18: 0x08, 19: 0x15, 20: 0x17, 21: 0x1C, 22: 0x18, 23: 0x0C, 24: 0x12, 25: 0x13,
      26: 0x2F, 27: 0x30, 30: 0x04, 31: 0x16, 32: 0x07, 33: 0x09, 34: 0x0A, 35: 0x0B, 36: 0x0D, 37: 0x0E,
      38: 0x0F, 39: 0x33, 40: 0x34, 41: 0x35, 43: 0x31, 44: 0x1D, 45: 0x1B, 46: 0x06, 47: 0x19, 48: 0x05,
      49: 0x11, 50: 0x10, 51: 0x36, 52: 0x37, 53: 0x38, 57: 0x2C, 86: 0x64, 89: 0x87, 124: 0x89}
DEAD = {"dead_circumflex": "^", "dead_grave": "`", "dead_tilde": "~"}
YEN = 0x7F  # the table's code for ¥, the JIS Yen key's character

def char(sym):
    sym = sym.lstrip("+")
    if sym in DEAD:
        return DEAD[sym]
    m = re.fullmatch(r"U\+([0-9a-fA-F]{4})", sym)
    if not m:
        return None
    c = chr(int(m.group(1), 16))
    return c if (" " <= c <= "~" or c == "¥") else None

def keymap(name):
    out = {}
    for line in open(f"{D}/{name}.map"):
        m = re.match(r"keycode +(\d+) = (.*)", line)
        if m and int(m.group(1)) in KC:
            syms = m.group(2).split()
            out[KC[int(m.group(1))]] = [char(s) if i < len(syms) else None for i, s in enumerate(syms[:4])]
    return out

def code(c):
    return 0 if c is None else (YEN if c == "¥" else ord(c))

def c_char(v):
    return "0" if v == 0 else ("KB_YEN_CHAR" if v == YEN else ("'\\\\'" if v == 0x5C else ("'\\''" if v == 0x27 else f"'{chr(v)}'")))

layouts = [("us", "pc"), ("us", "mac"), ("gb", "pc"), ("gb", "mac"), ("de", "pc"), ("de", "mac"), ("fr", "pc"), ("fr", "mac")]
usages = sorted(KC.values())
print("/* Generated from xkeyboard-config 2.46 (ckbcomp) by gen.py: each printing key's character")
print(" * at plain, Shift, AltGr or Option, and Shift with it, kept when the RND-001 can type it */")
for l, a in layouts:
    km = keymap(f"{l}_{a}")
    print(f"static const uint8_t n64_kb_chars_{l}_{a}[N64_KB_CHAR_USAGES][4] = {{")
    for u in usages:
        lv = km.get(u, [None] * 4)
        if any(code(x) for x in lv):
            print(f"    [0x{u:02X}] = {{{', '.join(c_char(code(x)) for x in lv)}}},")
    print("};\n")
# JIS: each character to its key, Shift in the top bit
jp = keymap("jp_pc")
inv = {}
for u in usages:
    for lvl in (0, 1):
        c = code(jp.get(u, [None] * 4)[lvl])
        if c and c not in inv:
            inv[c] = (u, lvl)
print("/* The RND-001's characters, JIS positions: each character's key, with N64_KB_JIS_SHIFT when typed with Shift */")
print("static const uint16_t n64_kb_jis_keys[N64_KB_CHARS] = {")
for c in sorted(inv):
    u, lvl = inv[c]
    print(f"    [{c_char(c)}] = 0x{u:02X}{' | N64_KB_JIS_SHIFT' if lvl else ''},")
print("};")
print("\n/* usages: " + ", ".join(f"0x{u:02X}" for u in usages) + " */", file=sys.stderr)
missing = [c for c in [chr(x) for x in range(0x21, 0x7F)] if ord(c) not in inv]
print("JIS can't type: " + repr(missing), file=sys.stderr)
