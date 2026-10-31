/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

/* The Bluetooth LED is lit without PWM, so it never strobes: it's on (an error), blinking (pairing)
 * or off. hw_config.led_level sets how bright on is: level 1 is the pin's pull-up alone, a faint
 * glow, and levels 2-5 are the pad's drive strengths, weakest to strongest. LEDC channel 0 blinks
 * in step, on or off with no fade, for the port LED that shows pairing on boards that have them. */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/efuse_reg.h>
#include <nvs_flash.h>
#include "zephyr/atomic.h"
#include "system/gpio.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
#include "adapter/config.h"
#include "led.h"

#ifdef CONFIG_BLUERETRO_SYSTEM_SEA_BOARD
#define ERR_LED_PIN 32
#else
#define ERR_LED_PIN 17
#endif
#define PICO_ERR_LED_PIN 20

#define ERR_LED_BLINK_MS 500

/* LED flags */
enum {
    ERR_LED_SET = 0,
    ERR_LED_LIT,
};

static atomic_t led_flags = 0;
static TaskHandle_t err_led_task_hdl;
static uint8_t err_led_pin = ERR_LED_PIN;

static uint32_t err_led_level(void) {
    if (hw_config.led_level < ERR_LED_LEVEL_MIN || hw_config.led_level > ERR_LED_LEVEL_MAX) {
        return ERR_LED_LEVEL_DEFAULT;
    }
    return hw_config.led_level;
}

/* The pad as a driven output, at the level's drive strength (the glow's flashes at the weakest) */
static void err_led_pad_output(void) {
    uint32_t level = err_led_level();

    gpio_set_pull_mode(err_led_pin, GPIO_FLOATING);
    gpio_set_drive_capability(err_led_pin, (gpio_drive_cap_t)((level > ERR_LED_LEVEL_MIN) ? level - 2 : 0));
    gpio_set_direction(err_led_pin, GPIO_MODE_OUTPUT);
}

static void err_led_drive(uint32_t on) {
    if (on && err_led_level() == ERR_LED_LEVEL_MIN) {
        /* The pull-up alone: tens of microamps through the LED */
        gpio_set_direction(err_led_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(err_led_pin, GPIO_PULLUP_ONLY);
    }
    else {
        err_led_pad_output();
        gpio_set_level(err_led_pin, on);
    }
    if (on) {
        atomic_set_bit(&led_flags, ERR_LED_LIT);
    }
    else {
        atomic_clear_bit(&led_flags, ERR_LED_LIT);
    }
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0,
        on ? hw_config.led_pulse_on_duty_cycle : hw_config.led_pulse_off_duty_cycle, 0);
}

static void err_led_task(void *param) {
    while (1) {
        if (!atomic_test_bit(&led_flags, ERR_LED_SET)) {
            err_led_drive(1);
            vTaskDelay(ERR_LED_BLINK_MS / portTICK_PERIOD_MS);
            err_led_drive(0);
            vTaskDelay(ERR_LED_BLINK_MS / portTICK_PERIOD_MS);
        }
        else {
            vTaskSuspend(err_led_task_hdl);
        }
    }
}

void err_led_init(uint32_t package) {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = hw_config.led_pulse_hz,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = hw_config.led_pulse_off_duty_cycle,
        .gpio_num   = ERR_LED_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0,
    };

    if (package == EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302) {
        ledc_channel.gpio_num = PICO_ERR_LED_PIN;
        err_led_pin = PICO_ERR_LED_PIN;
    }

    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);
    /* ledc_set_duty_and_update needs the fade service, though nothing fades now */
    ledc_fade_func_install(0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_off_duty_cycle, 0);

    /* The LED's own pin leaves channel 0 for the GPIO output register */
    esp_rom_gpio_connect_out_signal(err_led_pin, SIG_GPIO_OUT_IDX, 0, 0);

    /* The GPIO calls take more stack than the fades did */
    xTaskCreatePinnedToCore(&err_led_task, "err_led_task", 1024, NULL, 5, &err_led_task_hdl, 0);
    err_led_clear();
}

void err_led_cfg_update(void) {
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, hw_config.led_pulse_hz);
}

void err_led_set(void) {
    vTaskSuspend(err_led_task_hdl);
    err_led_drive(1);
    atomic_set_bit(&led_flags, ERR_LED_SET);
}

void err_led_clear(void) {
    /* When error is set it stay on until power cycle */
    if (!atomic_test_bit(&led_flags, ERR_LED_SET)) {
        vTaskSuspend(err_led_task_hdl);
        err_led_drive(0);
    }
}

void err_led_pulse(void) {
    vTaskResume(err_led_task_hdl);
}

uint32_t err_led_get_pin(void) {
    return err_led_pin;
}

void err_led_flash_prepare(void) {
    err_led_pad_output();
}

void err_led_restore(void) {
    esp_rom_gpio_connect_out_signal(err_led_pin, SIG_GPIO_OUT_IDX, 0, 0);
    err_led_drive(atomic_test_bit(&led_flags, ERR_LED_LIT));
}

uint32_t err_led_get_level(void) {
    return err_led_level();
}

int32_t err_led_set_level(uint32_t level) {
    nvs_handle_t nvs;

    if (level < ERR_LED_LEVEL_MIN || level > ERR_LED_LEVEL_MAX) {
        return -1;
    }
    hw_config.led_level = level;
    if (atomic_test_bit(&led_flags, ERR_LED_LIT)) {
        err_led_drive(1);
    }
    /* Saved as a hw_config override, which hw_config_patch loads at boot */
    if (nvs_open("hw", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, "led_level", level);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    return 0;
}
