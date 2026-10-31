/*
 * SPDX-License-Identifier: Apache-2.0
 */

/* Commands over the console UART, the board's USB serial port, so a host can do what the boot
 * button does without timing a press: open or close pairing, drop every device and read back
 * what is connected. A press is read by how long it is held, and holding it 3 s too long is a
 * factory reset; a command names what it does. Each command gets one "# cmd" line back.
 *
 * Anything else is ignored without a word. A host tty left in its default mode echoes every line
 * this board prints back into its RX, so a reply to an unknown line would be echoed in, answered
 * and echoed again for as long as the port stays open.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_rom_uart.h"
#include "adapter/adapter.h"
#include "adapter/wired/n64_runtime.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "system/led.h"
#include "serial_cmd.h"

#define SERIAL_CMD_LINE_MAX 32
#define SERIAL_CMD_PORTS 4
#define SERIAL_CMD_LIST "inq start | inq stop | disconnect | status | led <1-5>"

static char line[SERIAL_CMD_LINE_MAX];
static uint32_t line_len;
static bool line_overflow;
static char reply[192];

static const char *report_type_name(uint32_t report_type) {
    switch (report_type) {
        case KB:
            return "kb";
        case MOUSE:
            return "mouse";
        case PAD:
            return "pad";
        default:
            return "other";
    }
}

static const char *dev_mode_name(int32_t dev_mode) {
    switch (dev_mode) {
        case DEV_PAD:
            return "pad";
        case DEV_PAD_ALT:
            return "pad_alt";
        case DEV_KB:
            return "kb";
        case DEV_MOUSE:
            return "mouse";
        default:
            return "?";
    }
}

static void cmd_inquiry(const char *name, uint32_t on) {
    uint32_t was = !!bt_hci_get_inquiry();

    if (was != on) {
        if (on) {
            bt_hci_start_inquiry();
        }
        else {
            bt_hci_stop_inquiry();
        }
    }
    printf("# cmd %s: ok, inquiry %s%s\n", name, on ? "on" : "off", (was == on) ? " already" : "");
}

/* The Bluetooth LED's brightness, saved: 1 the pull-up's glow, 2-5 the pad's drive strengths.
 * The reply names the whole line, "# cmd led 2: ...", as the others do */
static void cmd_led(const char *arg) {
    uint32_t level = (arg[0] >= '0' && arg[0] <= '9' && arg[1] == '\0') ? (uint32_t)(arg[0] - '0') : 0;

    if (err_led_set_level(level) == 0) {
        printf("# cmd %s: ok, level=%lu\n", line, level);
    }
    else {
        printf("# cmd %s: refused, a level is %d-%d\n", line, ERR_LED_LEVEL_MIN, ERR_LED_LEVEL_MAX);
    }
}

static void cmd_disconnect(void) {
    uint32_t cnt = bt_host_get_flag_dev_cnt(BT_DEV_DEVICE_FOUND);

    bt_host_disconnect_all();
    printf("# cmd disconnect: ok, %lu device(s)\n", cnt);
}

/* One line, so another task's output can't land in the middle of it:
 * "# cmd status: inquiry=on devices=1 p1=kb@F0:1F:AF:12:34:56 p2=- p3=- p4=- n64=kb,pad,pad,pad led=2"
 * pN is the device BlueRetro routes to output port N, and n64= is the identity each N64 port
 * answers with now, which changes only when the console next asks (a reset or an identify).
 */
static void cmd_status(void) {
    size_t len = 0;

    len += snprintf(reply + len, sizeof(reply) - len, "# cmd status: inquiry=%s devices=%lu",
        bt_hci_get_inquiry() ? "on" : "off", bt_host_get_flag_dev_cnt(BT_DEV_HID_INIT_DONE));

    for (uint32_t port = 0; port < SERIAL_CMD_PORTS && len < sizeof(reply); port++) {
        struct bt_dev *device = NULL;

        if (bt_host_get_active_dev_from_out_idx(port, &device) >= 0 && device) {
            len += snprintf(reply + len, sizeof(reply) - len, " p%lu=%s@%02X:%02X:%02X:%02X:%02X:%02X",
                port + 1, report_type_name(device->ids.report_type),
                device->remote_bdaddr[5], device->remote_bdaddr[4], device->remote_bdaddr[3],
                device->remote_bdaddr[2], device->remote_bdaddr[1], device->remote_bdaddr[0]);
        }
        else {
            len += snprintf(reply + len, sizeof(reply) - len, " p%lu=-", port + 1);
        }
    }

    if (wired_adapter.system_id == N64) {
        for (uint32_t port = 0; port < SERIAL_CMD_PORTS && len < sizeof(reply); port++) {
            len += snprintf(reply + len, sizeof(reply) - len, "%s%s", port ? "," : " n64=",
                dev_mode_name(n64_runtime_get_active_mode(port, DEV_PAD)));
        }
    }
    if (len < sizeof(reply)) {
        snprintf(reply + len, sizeof(reply) - len, " led=%lu", err_led_get_level());
    }
    printf("%s\n", reply);
}

static void serial_cmd_run(void) {
    if (!strcmp(line, "inq start")) {
        cmd_inquiry(line, 1);
    }
    else if (!strcmp(line, "inq stop")) {
        cmd_inquiry(line, 0);
    }
    else if (!strcmp(line, "disconnect")) {
        cmd_disconnect();
    }
    else if (!strcmp(line, "status")) {
        cmd_status();
    }
    else if (!strncmp(line, "led ", 4)) {
        cmd_led(line + 4);
    }
    else if (!strcmp(line, "help")) {
        printf("# cmd help: %s\n", SERIAL_CMD_LIST);
    }
}

void serial_cmd_poll(void) {
    uint8_t c;

    while (esp_rom_output_rx_one_char(&c) == 0) {
        if (c == '\r' || c == '\n') {
            if (line_len && !line_overflow) {
                line[line_len] = '\0';
                serial_cmd_run();
            }
            line_len = 0;
            line_overflow = false;
        }
        else if (line_len < SERIAL_CMD_LINE_MAX - 1) {
            line[line_len++] = c;
        }
        else {
            line_overflow = true;
        }
    }
}
