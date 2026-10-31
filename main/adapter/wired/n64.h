/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _N64_H_
#define _N64_H_
#include "adapter/adapter.h"

/* The keyboard mode's output: the RANDnet read's reply (command 0x13), then the extended read's
 * (command 0x2A, libdragon's: marker, version, status, the port's settings and ten key codes) */
#define N64_KB_READ_LEN 7
#define N64_KB_EXT_READ_CMD 0x2A
#define N64_KB_EXT_READ_OFFSET 7
#define N64_KB_EXT_READ_LEN 24
/* After the replies, the LED byte the console last sent (0xFF until it sends one), so a change goes
 * to the Bluetooth keyboard once */
#define N64_KB_LEDS_OFFSET 31
/* Then 1 when the port's keyboard is in classic mode: an RND-001, which has no extended read, so the
 * driver answers 0x2A as it answers what it doesn't know */
#define N64_KB_CLASSIC_OFFSET 32
/* Then 1 while the port is silent after its mode changed, so a program sees its keyboard leave and
 * come back in the new mode */
#define N64_KB_SILENT_OFFSET 33
/* The LEDs each read's byte lights: the RND-001's Num and Caps (its Power LED would be a keyboard's
 * Scroll Lock, so it stays on the console), and the extended read's HID five, Num to Kana */
#define N64_KB_LEDS_RND 0x03
#define N64_KB_LEDS_HID 0x1F

void n64_meta_init(struct wired_ctrl *ctrl_data);
void n64_apply_meta(int32_t dev_mode, struct wired_ctrl *ctrl_data);
void n64_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void n64_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void n64_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);
void n64_gen_turbo_mask(struct wired_data *wired_data);
void n64_kb_cfg_apply(void);

#endif /* _N64_H_ */
