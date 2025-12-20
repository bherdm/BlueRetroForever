/*
 * Copyright (c) 2019-2025, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _N64_RUNTIME_H_
#define _N64_RUNTIME_H_

#include <stdint.h>
#include <stdbool.h>
#include <esp_attr.h>
#include "adapter/adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

void n64_runtime_set_desired_mode(uint8_t port, int32_t dev_mode);
int32_t n64_runtime_get_active_mode(uint8_t port, int32_t fallback_dev_mode);

/* Call from ISR with the first command byte (buf[0]).
 * If cmd is identify/reset (0x00/0xFF) and desired != active, active is updated
 * and a reinit is requested for task context.
 */
void IRAM_ATTR n64_runtime_on_identify_cmd(uint8_t port, uint8_t cmd);

/* Task-context: returns true once per apply to let a task reinit buffers safely. */
bool n64_runtime_take_reinit(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* _N64_RUNTIME_H_ */
