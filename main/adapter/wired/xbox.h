/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _XBOX_H_
#define _XBOX_H_
#include "adapter/adapter.h"

void xbox_meta_init(struct wired_ctrl *ctrl_data);
void xbox_init_buffer(int32_t dev_mode, struct wired_data *wired_data);
void xbox_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data);
void xbox_fb_to_generic(int32_t dev_mode, struct raw_fb *raw_fb_data, struct generic_fb *fb_data);
void xbox_gen_turbo_mask(struct wired_data *wired_data);

#endif /* _XBOX_H_ */
