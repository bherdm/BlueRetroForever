/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _XBOX_USB_H_
#define _XBOX_USB_H_

#include <stdint.h>

void xbox_init(void);
void xbox_send_report(void);
void xbox_get_report(void);
void xbox_port_cfg(uint16_t mask);

#endif  /* _XBOX_USB_H_ */
