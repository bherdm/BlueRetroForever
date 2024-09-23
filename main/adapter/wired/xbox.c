/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/wired/wired.h"
#include "wired/xbox_usb.h"
#include "xbox.h"

#define XBOX_JOYSTICK_AXES_CNT 4

enum {
    XBOX_D_UP = 0,
    XBOX_D_DOWN,
    XBOX_D_LEFT,
    XBOX_D_RIGHT,
    XBOX_START,
    XBOX_BACK,
    XBOX_L3,
    XBOX_R3,
};

static DRAM_ATTR const uint8_t xbox_axes_idx[ADAPTER_PS2_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       2,       3,       6,      7,
/*  TRIG_LS, TRIG_RS, DPAD_L,  DPAD_R,  DPAD_D, DPAD_U  */
    5,       4,       -1,       -1,     -1,     -1,
/*  BTN_L,   BTN_R,   BTN_D,   BTN_U  */
    2,       1,       0,       3
};

static DRAM_ATTR const struct ctrl_meta xbox_axes_meta[ADAPTER_PS2_MAX_AXES] =
{
    {.size_min = -32768, .size_max = 32767, .neutral = 0x0000, .abs_max = 32768, .abs_min = 32768},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x0000, .abs_max = 32768, .abs_min = 32768},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x0000, .abs_max = 32768, .abs_min = 32768},
    {.size_min = -32768, .size_max = 32767, .neutral = 0x0000, .abs_max = 32768, .abs_min = 32768},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
    {.size_min = 0, .size_max = 255, .neutral = 0x00, .abs_max = 0x100, .abs_min = 0x00},
};

struct xbox_map {
    uint8_t tbd;
    uint8_t size;
    uint8_t buttons;
    uint8_t tbd2;
    union {
        uint8_t axes[16];
        struct {
            uint8_t pressure[8];
            uint16_t sticks[4];
        };
    };
} __packed;

static const uint32_t xbox_mask[4] = {0xBB3F0FFF, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t xbox_desc[4] = {0x330F00FF, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t xbox_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(XBOX_D_LEFT), BIT(XBOX_D_RIGHT), BIT(XBOX_D_DOWN), BIT(XBOX_D_UP),
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(XBOX_START), BIT(XBOX_BACK), 0, 0,
    0, 0, 0, BIT(XBOX_L3),
    0, 0, 0, BIT(XBOX_R3),
};

void IRAM_ATTR xbox_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        default:
        {
            struct xbox_map *map = (struct xbox_map *)wired_data->output;
            struct xbox_map *map_mask = (struct xbox_map *)wired_data->output_mask;

            memset((void *)map, 0, sizeof(*map));
            memset((void *)map_mask, 0xFF, sizeof(*map_mask));
            map->size = 0x14;
            break;
        }
    }
}

void xbox_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_PS2_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                default:
                    ctrl_data[i].mask = xbox_mask;
                    ctrl_data[i].desc = xbox_desc;
                    ctrl_data[i].axes[j].meta = &xbox_axes_meta[j];
                    break;
            }
        }
    }
}

static void xbox_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct xbox_map *map = (struct xbox_map *)wired_data->output;

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map->buttons |= xbox_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else {
                map->buttons &= ~xbox_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    /* Joystick */
    for (uint32_t i = 0; i < XBOX_JOYSTICK_AXES_CNT; i++) {
        uint32_t btn_id = axis_to_btn_id(i);
        uint32_t btn_mask = axis_to_btn_mask(i);
        if (ctrl_data->map_mask[0] & (btn_mask & xbox_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map->sticks[xbox_axes_idx[i]] = 32767;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map->sticks[xbox_axes_idx[i]] = -32768;
            }
            else {
                map->sticks[xbox_axes_idx[i]] = (uint16_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[btn_id] = ctrl_data->axes[i].cnt_mask;
    }

    /* Analog buttons */
    for (uint32_t i = XBOX_JOYSTICK_AXES_CNT; i < ADAPTER_PS2_MAX_AXES; i++) {
        uint32_t btn_id = axis_to_btn_id(i);
        uint32_t btn_mask = axis_to_btn_mask(i);
        if (ctrl_data->map_mask[0] & (btn_mask & xbox_desc[0])) {
            if (ctrl_data->axes[i].value > ctrl_data->axes[i].meta->size_max) {
                map->pressure[xbox_axes_idx[i]] = 255;
            }
            else if (ctrl_data->axes[i].value < ctrl_data->axes[i].meta->size_min) {
                map->pressure[xbox_axes_idx[i]] = 0;
            }
            else {
                map->pressure[xbox_axes_idx[i]] = (uint8_t)(ctrl_data->axes[i].value + ctrl_data->axes[i].meta->neutral);
            }
        }
        wired_data->cnt_mask[btn_id] = ctrl_data->axes[i].cnt_mask;
    }

#ifdef CONFIG_IDF_TARGET_ESP32S2
    xbox_send_report();
#endif

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d], \"btns\": %d}\n",
        map->sticks[xbox_axes_idx[0]], map->sticks[xbox_axes_idx[1]],
        map->sticks[xbox_axes_idx[2]], map->sticks[xbox_axes_idx[3]],
        map->pressure[xbox_axes_idx[4]], map->pressure[xbox_axes_idx[5]],
        map->pressure[xbox_axes_idx[6]], map->pressure[xbox_axes_idx[7]],
        map->pressure[xbox_axes_idx[12]], map->pressure[xbox_axes_idx[13]],
        map->pressure[xbox_axes_idx[14]], map->pressure[xbox_axes_idx[15]],
        map->buttons);
#endif
}

void xbox_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (ctrl_data->index < 1) {
        switch (dev_mode) {
            default:
                xbox_ctrl_from_generic(ctrl_data, wired_data);
                break;
        }
    }
}

void xbox_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data) {
    fb_data->wired_id = raw_fb_data->header.wired_id;
    fb_data->type = raw_fb_data->header.type;

    switch (fb_data->type) {
        case FB_TYPE_RUMBLE:
            /* This stop rumble when BR timeout trigger */
            if (raw_fb_data->header.data_len == 0) {
                printf("# %s: RX Stop rumble packet\n", __FUNCTION__);
                fb_data->state = 0;
                fb_data->lf_pwr = fb_data->hf_pwr = 0;
            }
            else {
                printf("# %s: Set RX rumble\n", __FUNCTION__);
                fb_data->state = (raw_fb_data->data[0] || raw_fb_data->data[1] ? 1 : 0);
                fb_data->lf_pwr = raw_fb_data->data[0];
                fb_data->hf_pwr = raw_fb_data->data[1];
            }
            break;
    }
}

void IRAM_ATTR xbox_gen_turbo_mask(struct wired_data *wired_data) {
    struct xbox_map *map_mask = (struct xbox_map *)wired_data->output_mask;

    memset(map_mask, 0xFF, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(xbox_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (xbox_btns_mask[i] && mask) {
            if (wired_data->cnt_mask[i] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    map_mask->buttons &= ~xbox_btns_mask[i];
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    map_mask->buttons &= ~xbox_btns_mask[i];
                }
            }
        }
    }

    wired_gen_turbo_mask_axes8(wired_data, map_mask->axes, ADAPTER_PS2_MAX_AXES,
        xbox_axes_idx, xbox_axes_meta);
    wired_gen_turbo_mask_axes16(wired_data, map_mask->sticks,
        XBOX_JOYSTICK_AXES_CNT, xbox_axes_idx, xbox_axes_meta);
}
