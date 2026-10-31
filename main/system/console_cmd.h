/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CONSOLE_CMD_H_
#define _CONSOLE_CMD_H_

#include <stdint.h>

/* BlueRetroForever's own command, sent by a console's program on any port in any mode
 * (docs/n64-commands-design.md): [CONSOLE_CMD_ID, verb, arg0, arg1], answered with the status block
 * [0x42, version, verb, result, flags, LED level, port 1-4's devices] */
#define CONSOLE_CMD_ID 0x7B
#define CONSOLE_CMD_SEND_LEN 4
#define CONSOLE_CMD_REPLY_LEN 10
#define CONSOLE_CMD_MARKER 0x42
#define CONSOLE_CMD_VERSION 1

enum {
    CONSOLE_CMD_STATUS = 0x00,
    CONSOLE_CMD_PAIRING_OPEN,
    CONSOLE_CMD_PAIRING_CLOSE,
    CONSOLE_CMD_DISCONNECT,     /* arg0: port 0-3, or CONSOLE_CMD_ALL_PORTS */
    CONSOLE_CMD_LED_LEVEL,      /* arg0: system/led.h's level; there is no off */
    CONSOLE_CMD_KB_SETTINGS,    /* the reply's bytes 6-9: each port's keyboard settings byte */
    CONSOLE_CMD_KB_SET,         /* arg0: port 0-3, or CONSOLE_CMD_ALL_PORTS; arg1: a settings byte */
};

/* The settings byte's bits that aren't settings: config.h's kb_cfg_byte has the others */
#define CONSOLE_CMD_KB_LAYOUT_MASK 0x07
#define CONSOLE_CMD_KB_RESERVED 0x60

#define CONSOLE_CMD_ALL_PORTS 0xFF

enum {
    CONSOLE_CMD_OK = 0,
    CONSOLE_CMD_UNKNOWN_VERB,
    CONSOLE_CMD_BAD_ARG,
};

/* The status block's flags and devices */
#define CONSOLE_CMD_FLAG_PAIRING 0x01
enum {
    CONSOLE_CMD_DEV_NONE = 0,
    CONSOLE_CMD_DEV_PAD,
    CONSOLE_CMD_DEV_KB,
    CONSOLE_CMD_DEV_MOUSE,
};

/* From a wired driver's interrupt: the reply to cmd, with the verb queued for console_cmd_poll */
void console_cmd_answer(const uint8_t *cmd, uint8_t *reply);
/* From the system task: run the verbs queued, then bring the status block up to date */
void console_cmd_poll(void);

#endif /* _CONSOLE_CMD_H_ */
