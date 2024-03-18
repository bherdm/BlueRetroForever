/*
 * Copyright (c) 2021-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stddef.h>
#include "adapter/adapter.h"
#include "wired_common.h"

static const char *sys_name[WIRED_MAX] = {
    "AUTO",
    "PARALLEL_1P_PP",
    "PARALLEL_2P_PP",
    "NES",
    "PCE",
    "MD-GENESIS",
    "SNES",
    "CD-i",
    "CD32",
    "3DO",
    "JAGUAR",
    "PSX",
    "SATURN",
    "PC-FX",
    "JVS",
    "N64",
    "DC",
    "PS2",
    "GC",
    "Wii-EXT",
    "VB",
    "PARALLEL_1P_OD",
    "PARALLEL_2P_OD",
    "SEA Board",
    "SPI",
    "XBOX",
};

const char *wired_get_sys_name(void) {
    return sys_name[wired_adapter.system_id];
}
