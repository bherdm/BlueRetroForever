/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <esp_timer.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/mouse_mapper.h"
#include "adapter/wired/wired.h"
#include "adapter/wireless/wireless.h"
#include "tests/cmds.h"
#include "bluetooth/mon.h"
#include "n64.h"
#include "n64_kb_layouts.h"

#define N64_AXES_MAX 2
enum {
    N64_LD_RIGHT,
    N64_LD_LEFT,
    N64_LD_DOWN,
    N64_LD_UP,
    N64_START,
    N64_Z,
    N64_B,
    N64_A,
    N64_C_RIGHT,
    N64_C_LEFT,
    N64_C_DOWN,
    N64_C_UP,
    N64_R,
    N64_L,
};

static DRAM_ATTR const uint8_t n64_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,      1
};

static DRAM_ATTR const struct ctrl_meta n64_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54, .abs_min = 0x54},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x54, .abs_min = 0x54},
};

static DRAM_ATTR const struct ctrl_meta n64_mouse_axes_meta[N64_AXES_MAX] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 0x7F, .abs_min = 0x80},
};

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_SENS
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_SENS 0.75f
#endif

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL 1.05f
#endif

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL_PCT
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL_PCT 105
#endif

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_SMOOTH
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_SMOOTH 0.2f
#endif

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_MAX
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_MAX 127
#endif

#ifndef CONFIG_BLUERETRO_N64_MOUSE_PTR_POLL_US
#define CONFIG_BLUERETRO_N64_MOUSE_PTR_POLL_US 16667 /* ~60Hz */
#endif

struct n64_map {
    uint16_t buttons;
    uint8_t axes[2];
} __packed;

struct n64_mouse_map {
    uint16_t buttons;
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

#define N64_KB_STATUS_HOME 0x01
#define N64_KB_STATUS_EXCESS_KEYS 0x10
#define N64_KB_EXT_MARKER 0x42
#define N64_KB_EXT_VERSION 1
#define N64_KB_CODE_HOME 0x1010
#define N64_KB_CODE_POWER 0x2481 /* then Sleep and Wake: libdragon's Generic Desktop page */

/* The keyboard mode's replies, big-endian on the wire: the RANDnet read (command 0x13), then the
 * extended read (command 0x2A, libdragon's), each sent as it stands */
struct n64_kb_map {
    uint16_t key_codes[3];
    uint8_t bitfield;
    uint8_t ext_marker;
    uint8_t ext_version;
    uint8_t ext_status;
    uint16_t ext_key_codes[10];
} __packed;

_Static_assert(offsetof(struct n64_kb_map, ext_marker) == N64_KB_EXT_READ_OFFSET, "0x2A reply offset");
_Static_assert(sizeof(struct n64_kb_map) - N64_KB_EXT_READ_OFFSET == N64_KB_EXT_READ_LEN, "0x2A reply length");
_Static_assert(sizeof(struct n64_kb_map) <= N64_KB_LEDS_OFFSET, "the LED byte follows the replies");
_Static_assert(N64_KB_CLASSIC_OFFSET < sizeof(((struct wired_data *)0)->output), "the classic flag fits");

/* The modifier byte's bits, USB order */
#define N64_KB_LCTRL BIT(0)
#define N64_KB_LSHIFT BIT(1)
#define N64_KB_LALT BIT(2)
#define N64_KB_LGUI BIT(3)
#define N64_KB_RCTRL BIT(4)
#define N64_KB_RSHIFT BIT(5)
#define N64_KB_RALT BIT(6)
#define N64_KB_RGUI BIT(7)
/* The RND-001 keys a keyboard without them gets stand-ins for, and Caps Lock, where JIS has 英数 */
#define N64_KB_CODE_KANA 0x1006
#define N64_KB_CODE_HENKAN 0x0E02
#define N64_KB_CODE_MUHENKAN 0x1002
#define N64_KB_CODE_CAPS 0x0F05

static struct mouse_mapper_cfg mouse_cfg[WIRED_MAX_DEV];
static struct mouse_mapper_state mouse_state[WIRED_MAX_DEV];

static inline void n64_mouse_mapper_load_cfg(uint8_t port) {
    mouse_mapper_default_cfg(&mouse_cfg[port]);
    mouse_cfg[port].pointer_mode = 1;
    /* Map existing percent-based knob to pointer pipeline (1200% -> 1.2x). */
    const float sens_percent = ((float)CONFIG_BLUERETRO_N64_MOUSE_SENS) / 1000.0f;
    const float sens_percent_base = 1.2f;
    mouse_cfg[port].sensitivity = CONFIG_BLUERETRO_N64_MOUSE_PTR_SENS * (sens_percent / sens_percent_base);
    /* Allow dialing accel via percent knob; fallback to float default. */
    mouse_cfg[port].accel = ((float)CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL_PCT) / 100.0f;
    if (mouse_cfg[port].accel < 1.0f) {
        mouse_cfg[port].accel = CONFIG_BLUERETRO_N64_MOUSE_PTR_ACCEL;
    }
    mouse_cfg[port].smoothing_alpha = CONFIG_BLUERETRO_N64_MOUSE_PTR_SMOOTH;
    mouse_cfg[port].max_per_frame = CONFIG_BLUERETRO_N64_MOUSE_PTR_MAX;
    mouse_cfg[port].poll_interval_s = ((float)CONFIG_BLUERETRO_N64_MOUSE_PTR_POLL_US) / 1000000.0f;
    mouse_mapper_init(&mouse_state[port], &mouse_cfg[port]);
}

static const uint32_t n64_mask[4] = {0x77DF0FFF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t n64_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t n64_btns_mask[32] = {
    0, 0, 0, 0,
    BIT(N64_C_LEFT), BIT(N64_C_RIGHT), BIT(N64_C_DOWN), BIT(N64_C_UP),
    BIT(N64_LD_LEFT), BIT(N64_LD_RIGHT), BIT(N64_LD_DOWN), BIT(N64_LD_UP),
    0, 0, 0, 0,
    BIT(N64_B), BIT(N64_C_DOWN), BIT(N64_A), BIT(N64_C_LEFT),
    BIT(N64_START), 0, 0, 0,
    BIT(N64_Z), BIT(N64_L), BIT(N64_C_UP), 0,
    BIT(N64_Z), BIT(N64_R), BIT(N64_C_RIGHT), 0,
};

static const uint32_t n64_mouse_mask[4] = {0x110000F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t n64_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t n64_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(N64_B), 0, 0, 0,
    BIT(N64_A), 0, 0, 0,
};

static const uint32_t n64_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0x2D7FFFFF, 0x003FC000 | BR_COMBO_MASK};
static const uint32_t n64_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const uint16_t n64_kb_scancode[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x070D, 0x0705, 0x070C, 0x0105, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x0502, 0x0504, 0x0503, 0x0402, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x010C, 0x0107, 0x0106, 0x0706, 0x080A, 0x040D, 0x070F, 0x050D,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x0000, 0x080D, 0x0711, 0x0000, 0x0000, 0x080C, 0x010E, 0x0206,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x0807, 0x0805, 0x0707, 0x0708, 0x0408, 0x0709, 0x0309, 0x0308,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x0809, 0x0808, 0x0407, 0x0406, 0x0108, 0x0409, 0x0806, 0x0109,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x050C, 0x0505, 0x0506, 0x0507, 0x0508, 0x0509, 0x0609, 0x0608,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
     0x0607, 0x0606, 0x060D, 0x010D, 0x0410, 0x060C, 0x040C, 0x0604,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    /* BACKSLASH: 0x0605, not 0x0410 — this fork's KB_MINUS (above) was moved onto 0x0410, so both
     * keys sent the same scancode and '\'/'|' typed as '-'/'_'. 0x0605 is free here (it is upstream's
     * minus slot); the pc64 ROM KMAP decodes its wire form 0x0506 -> '\'/'|'. */
    0x0605, 0x0307, 0x0405, 0x050D, 0x0209, 0x0208, 0x0207, 0x050F,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x010B, 0x010A, 0x080B, 0x070A, 0x070B, 0x020A, 0x020B, 0x030A,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x030B, 0x040A, 0x0302, 0x060B, 0x050B, 0x0802, 0x0702, 0x0000,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x0000, 0x0000, 0x0511, 0x0602, 0x0000, 0x050A, 0x0000, 0x0000,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0810, 0x0610,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x060E, 0x020E, 0x0210,
 /* KB_102ND, KB_RO, KB_YEN: a keyboard's come from n64_kb_usage_code; this table is only for a
  * device with no keyboard report on a keyboard output */
    0x0000, 0x0000, 0x0000,
};

/* libdragon's code for each RND-001 key, by the HID usage of the PC key in its place on a Japanese
 * keyboard (libdragon's src/joybus/keyboard_usage.c, inverted); every other usage's code is
 * 0x2000 | usage */
static const uint16_t n64_kb_usage_code[256] = {
    [0x04] = 0x0D07, /* A */
    [0x05] = 0x0708, /* B */
    [0x06] = 0x0508, /* C */
    [0x07] = 0x0507, /* D */
    [0x08] = 0x0601, /* E */
    [0x09] = 0x0607, /* F */
    [0x0A] = 0x0707, /* G */
    [0x0B] = 0x0807, /* H */
    [0x0C] = 0x0804, /* I */
    [0x0D] = 0x0907, /* J */
    [0x0E] = 0x0903, /* K */
    [0x0F] = 0x0803, /* L */
    [0x10] = 0x0908, /* M */
    [0x11] = 0x0808, /* N */
    [0x12] = 0x0704, /* O */
    [0x13] = 0x0604, /* P */
    [0x14] = 0x0C01, /* Q */
    [0x15] = 0x0701, /* R */
    [0x16] = 0x0C07, /* S */
    [0x17] = 0x0801, /* T */
    [0x18] = 0x0904, /* U */
    [0x19] = 0x0608, /* V */
    [0x1A] = 0x0501, /* W */
    [0x1B] = 0x0C08, /* X */
    [0x1C] = 0x0901, /* Y */
    [0x1D] = 0x0D08, /* Z */
    [0x1E] = 0x0C05, /* 1 */
    [0x1F] = 0x0505, /* 2 */
    [0x20] = 0x0605, /* 3 */
    [0x21] = 0x0705, /* 4 */
    [0x22] = 0x0805, /* 5 */
    [0x23] = 0x0905, /* 6 */
    [0x24] = 0x0906, /* 7 */
    [0x25] = 0x0806, /* 8 */
    [0x26] = 0x0706, /* 9 */
    [0x27] = 0x0606, /* 0 */
    [0x28] = 0x0D04, /* RETURN */
    [0x29] = 0x0A08, /* ESCAPE */
    [0x2A] = 0x0D06, /* BACKSPACE */
    [0x2B] = 0x0D01, /* TAB */
    [0x2C] = 0x0602, /* SPACE */
    [0x2D] = 0x0506, /* MINUS */
    [0x2E] = 0x0C06, /* BACKQUOTE */
    [0x2F] = 0x0504, /* QUOTELEFT */
    [0x30] = 0x0C04, /* BRACKETLEFT */
    [0x32] = 0x0406, /* BRACKETRIGHT */
    [0x33] = 0x0703, /* PLUS */
    [0x34] = 0x0603, /* ASTERISK */
    [0x35] = 0x0D05, /* ZENKAKU_HANKAKU */
    [0x36] = 0x0902, /* LESS */
    [0x37] = 0x0802, /* GREATER */
    [0x38] = 0x0702, /* QUESTIONMARK */
    [0x39] = 0x0F05, /* CAPS_LOCK */
    [0x3A] = 0x0B01, /* F1 */
    [0x3B] = 0x0A01, /* F2 */
    [0x3C] = 0x0B08, /* F3 */
    [0x3D] = 0x0A07, /* F4 */
    [0x3E] = 0x0B07, /* F5 */
    [0x3F] = 0x0A02, /* F6 */
    [0x40] = 0x0B02, /* F7 */
    [0x41] = 0x0A03, /* F8 */
    [0x42] = 0x0B03, /* F9 */
    [0x43] = 0x0A04, /* F10 */
    [0x44] = 0x0203, /* F11 */
    [0x45] = 0x0B06, /* F12 */
    [0x4A] = 0x1010, /* HOME */
    [0x4B] = 0x0208, /* PRIOR */
    [0x4D] = 0x0206, /* END */
    [0x4E] = 0x0207, /* NEXT */
    [0x4F] = 0x0405, /* RIGHT */
    [0x50] = 0x0205, /* LEFT */
    [0x51] = 0x0305, /* DOWN */
    [0x52] = 0x0204, /* UP */
    [0x53] = 0x0A05, /* NUM_LOCK */
    [0x65] = 0x0B05, /* MENU */
    [0x87] = 0x1004, /* BACKSLASH */
    [0x88] = 0x1006, /* KANA_LOCK */
    [0x89] = 0x1105, /* BAR */
    [0x8A] = 0x0E02, /* HENKAN */
    [0x8B] = 0x1002, /* MUHENKAN */
    [0xE0] = 0x1107, /* CONTROL_L */
    [0xE1] = 0x0E01, /* SHIFT_L */
    [0xE2] = 0x1008, /* ALT_L */
    [0xE3] = 0x0F07, /* META_L */
    [0xE5] = 0x0E06, /* SHIFT_R */
};

static inline uint16_t n64_kb_code(uint8_t usage) {
    return n64_kb_usage_code[usage] ? n64_kb_usage_code[usage] : 0x2000 | usage;
}

void IRAM_ATTR n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
        {
            struct n64_kb_map *map = (struct n64_kb_map *)wired_data->output;

            memset(wired_data->output, 0, sizeof(struct n64_kb_map));
            /* libdragon's probe on connect finds the extended read before any key is pressed */
            map->ext_marker = N64_KB_EXT_MARKER;
            map->ext_version = N64_KB_EXT_VERSION;
            wired_data->output[N64_KB_CLASSIC_OFFSET] = config.kb_cfg[wired_data->index].mode == KB_MODE_CLASSIC;
            /* A keyboard just connected takes the console's LEDs at its next read */
            wired_data->output[N64_KB_LEDS_OFFSET] = 0xFF;
            break;
        }
        case DEV_MOUSE:
        {
            struct n64_mouse_map *map = (struct n64_mouse_map *)wired_data->output;

            for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            map->buttons = 0x0000;
            n64_mouse_mapper_load_cfg(wired_data->index);
            break;
        }
        default:
        {
            struct n64_map *map = (struct n64_map *)wired_data->output;

            map->buttons = 0x0000;
            for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
                map->axes[n64_axes_idx[i]] = n64_axes_meta[i].neutral;
            }
            memset(wired_data->output_mask, 0xFF, sizeof(struct n64_map));
            break;
        }
    }
}

void n64_apply_meta(int32_t dev_mode, struct wired_ctrl *ctrl_data) {
    switch (dev_mode) {
        case DEV_KB:
            ctrl_data->mask = n64_kb_mask;
            ctrl_data->desc = n64_kb_desc;
            break;
        case DEV_MOUSE:
            ctrl_data->mask = n64_mouse_mask;
            ctrl_data->desc = n64_mouse_desc;
            for (uint32_t j = 0; j < N64_AXES_MAX; j++) {
                ctrl_data->axes[j + 2].meta = &n64_mouse_axes_meta[j];
            }
            break;
        case DEV_PAD:
        default:
            ctrl_data->mask = n64_mask;
            ctrl_data->desc = n64_desc;
            for (uint32_t j = 0; j < N64_AXES_MAX; j++) {
                ctrl_data->axes[j].meta = &n64_axes_meta[j];
            }
            break;
    }
}

void n64_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*4);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        n64_apply_meta(config.out_cfg[i].dev_mode, &ctrl_data[i]);
    }
}

static void n64_ctrl_special_action(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    /* Memory / Rumble toggle */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MT]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MT]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE);

                /* Change config directly but do not update file */
                if (config.out_cfg[ctrl_data->index].acc_mode == ACC_MEM) {
                    config.out_cfg[ctrl_data->index].acc_mode = ACC_RUMBLE;
                    adapter_toggle_fb(ctrl_data->index, 300000, 0xFF, 0xFF);
                    printf("# %s: Set rumble pak\n", __FUNCTION__);
                }
                else {
                    config.out_cfg[ctrl_data->index].acc_mode = ACC_MEM;
                    adapter_toggle_fb(ctrl_data->index, 150000, 0xFF, 0xFF);
                    printf("# %s: Set ctrl pak\n", __FUNCTION__);
                }
            }
        }
    }

    /* Bank rotation */
    if (ctrl_data->map_mask[0] & generic_btns_mask[PAD_MQ]) {
        if (ctrl_data->btns[0].value & generic_btns_mask[PAD_MQ]) {
            if (!atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_set_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);
            }
        }
        else {
            if (atomic_test_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2)) {
                atomic_clear_bit(&wired_data->flags, WIRED_WAITING_FOR_RELEASE2);

                config.global_cfg.banksel = (config.global_cfg.banksel + 1) & 0x3;
                printf("# %s: Set ctrl pak bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
            }
        }
    }
}

static void n64_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_map map_tmp;
    uint32_t map_mask = 0xFFFF;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & generic_btns_mask[i]) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_btns_mask[i];
                map_mask &= ~n64_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else if (map_mask & n64_btns_mask[i]) {
                map_tmp.buttons &= ~n64_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    n64_ctrl_special_action(ctrl_data, wired_data);

    for (uint32_t i = 0; i < N64_AXES_MAX; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & n64_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map_tmp.axes[n64_axes_idx[i]] = 127;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map_tmp.axes[n64_axes_idx[i]] = -128;
            }
            else {
                map_tmp.axes[n64_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));

    TESTS_CMDS_LOG("\"wired_output\": {\"axes\": [%d, %d], \"btns\": %d},\n",
        map_tmp.axes[n64_axes_idx[0]], map_tmp.axes[n64_axes_idx[1]], map_tmp.buttons);
    BT_MON_LOG("\"wired_output\": {\"axes\": [%02X, %02X], \"btns\": %04X},\n",
        map_tmp.axes[n64_axes_idx[0]], map_tmp.axes[n64_axes_idx[1]], map_tmp.buttons);
}

static void n64_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_mouse_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 4);
    const uint8_t port = ctrl_data->index;

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= n64_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~n64_mouse_btns_mask[i];
            }
        }
    }

    const bool map_rx = ctrl_data->map_mask[0] & (axis_to_btn_mask(AXIS_RX) & n64_mouse_desc[0]);
    const bool map_ry = ctrl_data->map_mask[0] & (axis_to_btn_mask(AXIS_RY) & n64_mouse_desc[0]);
    if ((ctrl_data->axes[AXIS_RX].relative || ctrl_data->axes[AXIS_RY].relative) && (map_rx || map_ry)) {
        int32_t ox = 0;
        int32_t oy = 0;
        int32_t dx = map_rx ? ctrl_data->axes[AXIS_RX].value : 0;
        int32_t dy = map_ry ? ctrl_data->axes[AXIS_RY].value : 0;

        mouse_mapper_process(&mouse_cfg[port], &mouse_state[port], dx, dy, esp_timer_get_time(), &ox, &oy);

        map_tmp.relative[0] = 1;
        map_tmp.relative[1] = 1;

        if (ox) {
            atomic_add(&raw_axes[0], ox);
        }
        if (oy) {
            atomic_add(&raw_axes[1], oy);
        }
    }
    else {
        for (uint32_t i = 2; i < 4; i++) {
            if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & n64_mouse_desc[0])) {
                if (ctrl_data->axes[i].relative) {
                    map_tmp.relative[n64_axes_idx[i]] = 1;
                    atomic_add(&raw_axes[n64_axes_idx[i]], ctrl_data->axes[i].value);
                }
                else {
                    map_tmp.relative[n64_axes_idx[i]] = 0;
                    raw_axes[n64_axes_idx[i]] = ctrl_data->axes[i].value;
                }
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void n64_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct n64_kb_map map_tmp = {0};
    uint32_t code_idx = 0;

    for (uint32_t i = 0; i < KBM_MAX; i++) {
        if (ctrl_data->map_mask[i / 32] & BIT(i & 0x1F)) {
            if (ctrl_data->btns[i / 32].value & BIT(i & 0x1F)) {
                if (n64_kb_scancode[i]) {
                    if (code_idx < ARRAY_SIZE(map_tmp.key_codes)) {
                        map_tmp.key_codes[code_idx] = n64_kb_scancode[i];
                    }
                    code_idx++;
                }
            }
        }
    }
    /* A fourth key held is the keyboard's own "too many keys": the status bit says so, and the
     * three slots keep the first three, as before */
    if (code_idx > ARRAY_SIZE(map_tmp.key_codes)) {
        map_tmp.bitfield |= 0x10;
    }

    /* map_mask is in IRAM but somehow GCC emit an l8ui which crash the ESP32 */
    /* volatile var force it to use a l32 */
    volatile uint32_t mask = ctrl_data->map_mask[2];
    if (mask & BIT(KB_HOME & 0x1F)) {
        if (ctrl_data->btns[2].value & BIT(KB_HOME & 0x1F)) {
            map_tmp.bitfield |= 0x01;
        }
    }

    TESTS_CMDS_LOG("\"wired_output\": {\"keys\": [%d, %d, %d], \"bitfield\": %d},\n",
        map_tmp.key_codes[0], map_tmp.key_codes[1], map_tmp.key_codes[2], map_tmp.bitfield);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

/* The keys that type characters: letters, digits, space and punctuation, with the ISO and JIS ones.
 * Enter, Escape, Backspace and Tab type none */
static uint32_t n64_kb_is_printing(uint8_t usage) {
    return (usage >= 0x04 && usage <= 0x38 && (usage < 0x28 || usage > 0x2B))
        || usage == 0x64 || usage == 0x87 || usage == 0x89;
}

/* The characters each printing key types on a layout and arrangement, or NULL for Japanese, whose
 * keys are the RND-001's own */
static const uint8_t (*n64_kb_layout_chars(uint32_t layout, uint32_t arrangement))[4] {
    uint32_t mac = (arrangement == KB_ARRANGEMENT_MAC);

    switch (layout) {
        case KB_LAYOUT_UK:
            return mac ? n64_kb_chars_gb_mac : n64_kb_chars_gb_pc;
        case KB_LAYOUT_DE:
            return mac ? n64_kb_chars_de_mac : n64_kb_chars_de_pc;
        case KB_LAYOUT_FR:
            return mac ? n64_kb_chars_fr_mac : n64_kb_chars_fr_pc;
        case KB_LAYOUT_JP:
            return NULL;
        default:
            return mac ? n64_kb_chars_us_mac : n64_kb_chars_us_pc;
    }
}

/* Classic mode (docs/classic-keyboard-design.md): the keyboard plays the RND-001 for retail
 * software. A printing key is read as the character it types on the port's layout and sent as the
 * RND-001 key that types it, with Shift as that key needs; a character the RND-001 can't type is
 * dropped. The RND-001's かな, 変換 and 無変換 come from stand-ins where a keyboard lacks them, and
 * media and system keys go, as the RND-001 has none. The list is modifiers, stand-ins, then keys */
static uint32_t n64_kb_classic_list(const struct kb_state *kb, const struct kb_cfg *cfg, uint16_t *list) {
    const uint8_t (*chars)[4] = n64_kb_layout_chars(cfg->layout, cfg->arrangement);
    uint32_t mac = (cfg->arrangement == KB_ARRANGEMENT_MAC);
    uint32_t mods = kb->mods;
    uint16_t stand_ins[4], keys[ARRAY_SIZE(kb->keys)];
    uint32_t stand_in_cnt = 0, key_cnt = 0, cnt = 0;
    int32_t shift = -1; /* the Shift the latest translated key needs, or -1 for the user's own */
    /* AltGr on a PC, left Option on a Mac (right Option stands in for かな) */
    uint32_t level3 = mac ? N64_KB_LALT : N64_KB_RALT;
    uint32_t level;

    if (mac) {
        if (chars && (mods & N64_KB_RALT)) {
            stand_ins[stand_in_cnt++] = N64_KB_CODE_KANA;
            mods &= ~N64_KB_RALT;
        }
        if (mods & N64_KB_RGUI) {
            stand_ins[stand_in_cnt++] = N64_KB_CODE_HENKAN;
            mods &= ~N64_KB_RGUI;
        }
        if (mods & N64_KB_LGUI) {
            stand_ins[stand_in_cnt++] = N64_KB_CODE_MUHENKAN;
            mods &= ~N64_KB_LGUI;
        }
    }
    else if (chars) {
        if (mods & N64_KB_RCTRL) {
            stand_ins[stand_in_cnt++] = N64_KB_CODE_KANA;
            mods &= ~N64_KB_RCTRL;
        }
        if (mods & N64_KB_RGUI) {
            stand_ins[stand_in_cnt++] = N64_KB_CODE_HENKAN;
            mods &= ~N64_KB_RGUI;
        }
    }

    level = ((mods & (N64_KB_LSHIFT | N64_KB_RSHIFT)) ? 1 : 0) | ((chars && (mods & level3)) ? 2 : 0);
    for (uint32_t i = 0; i < kb->key_cnt; i++) {
        uint8_t usage = kb->keys[i];

        if (chars && !mac && usage == 0x65) {
            /* Menu */
            stand_ins[stand_in_cnt++] = N64_KB_CODE_MUHENKAN;
        }
        else if (!chars && mac && (usage == 0x90 || usage == 0x91)) {
            /* Apple's Japanese keyboards: かな is かな, and 英数 is JIS's Caps Lock */
            stand_ins[stand_in_cnt++] = (usage == 0x90) ? N64_KB_CODE_KANA : N64_KB_CODE_CAPS;
        }
        else if (chars && n64_kb_is_printing(usage)) {
            uint8_t c = chars[usage][level];
            uint16_t jis = c ? n64_kb_jis_keys[c] : 0;

            if (jis) {
                keys[key_cnt++] = n64_kb_code(jis & 0xFF);
                shift = !!(jis & N64_KB_JIS_SHIFT);
            }
        }
        else {
            keys[key_cnt++] = n64_kb_code(usage);
        }
    }

    /* A translated key brings its own Shift; level 3 is spent on characters */
    if (shift >= 0) {
        mods &= ~(N64_KB_LSHIFT | N64_KB_RSHIFT);
        mods |= shift ? N64_KB_LSHIFT : 0;
    }
    if (chars) {
        mods &= ~level3;
    }
    for (uint32_t i = 0; i < 8; i++) {
        if (mods & BIT(i)) {
            list[cnt++] = n64_kb_code(0xE0 + i);
        }
    }
    for (uint32_t i = 0; i < stand_in_cnt; i++) {
        list[cnt++] = stand_ins[i];
    }
    for (uint32_t i = 0; i < key_cnt; i++) {
        list[cnt++] = keys[i];
    }
    return cnt;
}

/* Both reads are cut from one list: the held modifiers, the keys in press order, then the media and
 * system keys. The extended read sends its first ten, Home among them; the RANDnet read takes Home
 * out as its status bit and sends the first three. In classic mode the list is the RND-001's */
static void n64_kb_from_state(const struct kb_state *kb, struct wired_data *wired_data) {
    const struct kb_cfg *cfg = &config.kb_cfg[wired_data->index];
    uint32_t classic = (cfg->mode == KB_MODE_CLASSIC);
    struct n64_kb_map map_tmp = {
        .ext_marker = classic ? 0 : N64_KB_EXT_MARKER,
        .ext_version = classic ? 0 : N64_KB_EXT_VERSION,
    };
    uint16_t list[8 + ARRAY_SIZE(kb->keys) + ARRAY_SIZE(kb->consumer) + 3];
    uint32_t list_cnt = 0;
    uint32_t read_cnt = 0;

    wired_data->output[N64_KB_CLASSIC_OFFSET] = classic;
    if (classic) {
        list_cnt = n64_kb_classic_list(kb, cfg, list);
    }
    for (uint32_t i = 0; !classic && i < 8; i++) {
        if (kb->mods & BIT(i)) {
            list[list_cnt++] = n64_kb_code(0xE0 + i);
        }
    }
    for (uint32_t i = 0; !classic && i < kb->key_cnt; i++) {
        list[list_cnt++] = n64_kb_code(kb->keys[i]);
    }
    for (uint32_t i = 0; !classic && i < kb->consumer_cnt; i++) {
        /* libdragon's Consumer pages, 0x21 to 0x23, carry usages 0x0001 to 0x02FF: 0x0100 and
         * 0x0200 aren't keys and would make a code with a zero byte, and from 0x0300 the codes
         * would be the Generic Desktop page's */
        if ((kb->consumer[i] & 0xFF) && kb->consumer[i] < 0x300) {
            list[list_cnt++] = ((0x21 + (kb->consumer[i] >> 8)) << 8) | (kb->consumer[i] & 0xFF);
        }
    }
    for (uint32_t i = 0; !classic && i < 3; i++) {
        if (kb->system & BIT(i)) {
            list[list_cnt++] = N64_KB_CODE_POWER + i;
        }
    }

    for (uint32_t i = 0; i < list_cnt && i < ARRAY_SIZE(map_tmp.ext_key_codes); i++) {
        map_tmp.ext_key_codes[i] = sys_cpu_to_be16(list[i]);
    }
    if (list_cnt > ARRAY_SIZE(map_tmp.ext_key_codes) || kb->overflow) {
        map_tmp.ext_status |= N64_KB_STATUS_EXCESS_KEYS;
    }

    for (uint32_t i = 0; i < list_cnt; i++) {
        if (list[i] == N64_KB_CODE_HOME) {
            map_tmp.bitfield |= N64_KB_STATUS_HOME;
            continue;
        }
        if (read_cnt < ARRAY_SIZE(map_tmp.key_codes)) {
            map_tmp.key_codes[read_cnt] = sys_cpu_to_be16(list[i]);
        }
        read_cnt++;
    }
    if (read_cnt > ARRAY_SIZE(map_tmp.key_codes) || kb->overflow) {
        map_tmp.bitfield |= N64_KB_STATUS_EXCESS_KEYS;
    }

    TESTS_CMDS_LOG("\"wired_output\": {\"keys\": [%d, %d, %d], \"bitfield\": %d, \"ext_keys\": [",
        sys_be16_to_cpu(map_tmp.key_codes[0]), sys_be16_to_cpu(map_tmp.key_codes[1]),
        sys_be16_to_cpu(map_tmp.key_codes[2]), map_tmp.bitfield);
    for (uint32_t i = 0; i < ARRAY_SIZE(map_tmp.ext_key_codes); i++) {
        TESTS_CMDS_LOG("%s%d", i ? ", " : "", sys_be16_to_cpu(map_tmp.ext_key_codes[i]));
    }
    TESTS_CMDS_LOG("], \"ext_status\": %d, \"ext_marker\": %d},\n", map_tmp.ext_status, map_tmp.ext_marker);

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp));
}

void n64_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            if (ctrl_data->kb) {
                n64_kb_from_state(ctrl_data->kb, wired_data);
            }
            else {
                n64_kb_from_generic(ctrl_data, wired_data);
            }
            break;
        case DEV_MOUSE:
            n64_mouse_from_generic(ctrl_data, wired_data);
            break;
        case DEV_PAD:
        default:
            n64_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void n64_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data->header.wired_id;
    fb_data->type = raw_fb_data->header.type;

    if (fb_data->type == FB_TYPE_KB_LEDS) {
        /* RANDnet's Num (0x01) and Caps (0x02) are HID's Num and Caps; its Power (0x04) would light
         * a keyboard's Scroll Lock, and libdragon lights it by default, so it stays on the console */
        fb_data->led = raw_fb_data->data[0] & 0x03;
        return;
    }

    /* This stop rumble when BR timeout trigger */
    if (raw_fb_data->header.data_len == 0) {
        fb_data->state = 0;
        fb_data->lf_pwr = fb_data->hf_pwr = 0;
    }
    else {
        fb_data->state = raw_fb_data->data[0];
        fb_data->lf_pwr = (fb_data->state) ? 0xFF : 0x00;
        fb_data->hf_pwr = (fb_data->state) ? 0xFF : 0x00;
    }
}

void IRAM_ATTR n64_gen_turbo_mask(struct wired_data *wired_data) {
    struct n64_map *map_mask = (struct n64_map *)wired_data->output_mask;

    memset(map_mask, 0xFF, sizeof(*map_mask));

    wired_gen_turbo_mask_btns16_pos(wired_data, &map_mask->buttons, n64_btns_mask);
    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, N64_AXES_MAX, n64_axes_idx, n64_axes_meta);
}
