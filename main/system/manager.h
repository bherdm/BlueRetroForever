/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYS_MANAGER_H_
#define _SYS_MANAGER_H_ 

#include <stdint.h>

enum {
    SYS_MGR_CMD_NONE = -1,
    SYS_MGR_CMD_RST,
    SYS_MGR_CMD_PWR_ON,
    SYS_MGR_CMD_PWR_OFF,
    SYS_MGR_CMD_INQ_TOOGLE,
    SYS_MGR_CMD_FACTORY_RST,
    SYS_MGR_CMD_DEEP_SLEEP,
    SYS_MGR_CMD_ADAPTER_RST,
    SYS_MGR_CMD_WIRED_RST,
    SYS_MGR_CMD_FLASH_LED_ON,
    SYS_MGR_CMD_FLASH_LED_OFF,
    SYS_MGR_CMD_FLASH_LED_SLOW,
    SYS_MGR_CMD_FLASH_LED_FAST,
    SYS_MGR_CMD_FLASH_LED_FASTER,
};

void sys_mgr_cmd(int8_t cmd);
void sys_mgr_init(uint32_t package);

#endif /* _SYS_MANAGER_H_ */
