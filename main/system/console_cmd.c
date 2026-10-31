/*
 * SPDX-License-Identifier: Apache-2.0
 */

/* Commands a console's program sends: open and close pairing, read what's connected, disconnect,
 * and set the Bluetooth LED's level. A wired driver's interrupt answers at once from a status
 * block built ahead (the console allows no time to compute one on the wire) and queues the verb;
 * the system task runs it, so the reply shows the state before it, and a program reads status
 * again to see the change. Nothing here resets the board or writes its config. */

#include <stdio.h>
#include <esp_attr.h>
#include "adapter/adapter.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "system/led.h"
#include "console_cmd.h"

#define CONSOLE_CMD_QUEUE 4
#define CONSOLE_CMD_PORTS 4

/* Read by interrupts that may run while the flash cache is off: DRAM, and IRAM code below that
 * uses if/else, not a switch, whose jump table GCC would put in flash */
static DRAM_ATTR uint8_t status_block[CONSOLE_CMD_REPLY_LEN] = {CONSOLE_CMD_MARKER, CONSOLE_CMD_VERSION};
static DRAM_ATTR volatile uint32_t queue[CONSOLE_CMD_QUEUE];
static DRAM_ATTR volatile uint32_t queue_head, queue_tail;

void IRAM_ATTR console_cmd_answer(const uint8_t *cmd, uint8_t *reply) {
    uint32_t verb = cmd[1], arg = cmd[2], result = CONSOLE_CMD_OK;

    for (uint32_t i = 0; i < CONSOLE_CMD_REPLY_LEN; i++) {
        reply[i] = status_block[i];
    }
    if (verb == CONSOLE_CMD_DISCONNECT) {
        if (arg >= CONSOLE_CMD_PORTS && arg != CONSOLE_CMD_ALL_PORTS) {
            result = CONSOLE_CMD_BAD_ARG;
        }
    }
    else if (verb == CONSOLE_CMD_LED_LEVEL) {
        if (arg < ERR_LED_LEVEL_MIN || arg > ERR_LED_LEVEL_MAX) {
            result = CONSOLE_CMD_BAD_ARG;
        }
    }
    else if (verb != CONSOLE_CMD_STATUS && verb != CONSOLE_CMD_PAIRING_OPEN && verb != CONSOLE_CMD_PAIRING_CLOSE) {
        result = CONSOLE_CMD_UNKNOWN_VERB;
    }
    reply[2] = verb;
    reply[3] = result;

    if (result == CONSOLE_CMD_OK && verb != CONSOLE_CMD_STATUS) {
        uint32_t next = (queue_head + 1) % CONSOLE_CMD_QUEUE;

        /* A full queue lets the verb go: the program reads status and sends it again */
        if (next != queue_tail) {
            queue[queue_head] = verb | (arg << 8) | ((uint32_t)cmd[3] << 16);
            queue_head = next;
        }
    }
}

static void console_cmd_run(uint32_t entry) {
    uint32_t verb = entry & 0xFF, arg = (entry >> 8) & 0xFF;

    switch (verb) {
        case CONSOLE_CMD_PAIRING_OPEN:
            if (!bt_hci_get_inquiry()) {
                bt_hci_start_inquiry();
            }
            break;
        case CONSOLE_CMD_PAIRING_CLOSE:
            if (bt_hci_get_inquiry()) {
                bt_hci_stop_inquiry();
            }
            break;
        case CONSOLE_CMD_DISCONNECT:
            if (arg == CONSOLE_CMD_ALL_PORTS) {
                bt_host_disconnect_all();
            }
            else {
                struct bt_dev *device = NULL;

                if (bt_host_get_active_dev_from_out_idx(arg, &device) >= 0 && device) {
                    bt_hci_disconnect(device);
                }
            }
            break;
        case CONSOLE_CMD_LED_LEVEL:
            err_led_set_level(arg);
            break;
    }
    printf("# %s: verb %lu arg %lu\n", __FUNCTION__, verb, arg);
}

static uint8_t console_cmd_dev(uint32_t port) {
    struct bt_dev *device = NULL;

    if (bt_host_get_active_dev_from_out_idx(port, &device) < 0 || device == NULL) {
        return CONSOLE_CMD_DEV_NONE;
    }
    /* Only a generic HID device's report type is kept; the others are controllers */
    if (device->ids.type != BT_HID_GENERIC) {
        return CONSOLE_CMD_DEV_PAD;
    }
    switch (device->ids.report_type) {
        case KB:
        case KB_EXT:
            return CONSOLE_CMD_DEV_KB;
        case MOUSE:
            return CONSOLE_CMD_DEV_MOUSE;
        default:
            return CONSOLE_CMD_DEV_PAD;
    }
}

void console_cmd_poll(void) {
    while (queue_tail != queue_head) {
        uint32_t entry = queue[queue_tail];

        queue_tail = (queue_tail + 1) % CONSOLE_CMD_QUEUE;
        console_cmd_run(entry);
    }

    status_block[4] = bt_hci_get_inquiry() ? CONSOLE_CMD_FLAG_PAIRING : 0;
    status_block[5] = err_led_get_level();
    for (uint32_t port = 0; port < CONSOLE_CMD_PORTS; port++) {
        status_block[6 + port] = console_cmd_dev(port);
    }
}
