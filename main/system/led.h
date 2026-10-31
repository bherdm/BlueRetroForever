/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>

/* The Bluetooth LED's brightness: 1 the pin's pull-up alone, 2-5 the pad's drive strengths */
#define ERR_LED_LEVEL_MIN 1
#define ERR_LED_LEVEL_MAX 5
#define ERR_LED_LEVEL_DEFAULT 1

void err_led_init(uint32_t package);
void err_led_cfg_update(void);
void err_led_set(void);
void err_led_clear(void);
void err_led_pulse(void);
uint32_t err_led_get_pin(void);
void err_led_flash_prepare(void);
void err_led_restore(void);
uint32_t err_led_get_level(void);
int32_t err_led_set_level(uint32_t level);

#endif /* _LED_H_ */
