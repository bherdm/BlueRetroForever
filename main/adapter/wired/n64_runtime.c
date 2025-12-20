/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <esp_attr.h>
#include "zephyr/atomic.h"
#include "adapter/wired/n64_runtime.h"

#define N64_PORT_MAX 4

static atomic_t s_n64_desired_mode[N64_PORT_MAX] = {
    DEV_PAD, DEV_PAD, DEV_PAD, DEV_PAD,
};

static atomic_t s_n64_active_mode[N64_PORT_MAX] = {
    DEV_PAD, DEV_PAD, DEV_PAD, DEV_PAD,
};

static atomic_t s_n64_reinit_mask = 0;

void n64_runtime_set_desired_mode(uint8_t port, int32_t dev_mode) {
    if (port >= N64_PORT_MAX) {
        return;
    }
    atomic_set(&s_n64_desired_mode[port], dev_mode);
}

int32_t n64_runtime_get_active_mode(uint8_t port, int32_t fallback_dev_mode) {
    if (port >= N64_PORT_MAX) {
        return fallback_dev_mode;
    }
    return atomic_get(&s_n64_active_mode[port]);
}

void IRAM_ATTR n64_runtime_on_identify_cmd(uint8_t port, uint8_t cmd) {
    if (port >= N64_PORT_MAX) {
        return;
    }

    if (cmd != 0x00 && cmd != 0xFF) {
        return;
    }

    int32_t desired = atomic_get(&s_n64_desired_mode[port]);
    int32_t active = atomic_get(&s_n64_active_mode[port]);

    if (active != desired) {
        atomic_set(&s_n64_active_mode[port], desired);
        atomic_set_bit(&s_n64_reinit_mask, port);
    }
}

bool n64_runtime_take_reinit(uint8_t port) {
    if (port >= N64_PORT_MAX) {
        return false;
    }
    return atomic_test_and_clear_bit(&s_n64_reinit_mask, port);
}
