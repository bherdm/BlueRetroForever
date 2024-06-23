/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "hci_uart.h"
#include "host.h"
#include "zephyr/hci.h"

#define HCI_UART_TX_PIN 17
#define HCI_UART_RX_PIN 38
#define HCI_UART_RTS_PIN 21
#define HCI_UART_CTS_PIN 44

static hci_rx_cb_t hci_rx_cb;
static int hci_uart_port;
static struct bt_hci_pkt rx_pkt;

static void IRAM_ATTR hci_uart_rx_task(void *arg) {
    int32_t rx_len;
    uint32_t hdr_len;
    uint32_t total_len;
    while (1) {
        rx_len = uart_read_bytes(hci_uart_port, (uint8_t *)&rx_pkt, 1, portMAX_DELAY);
        if (rx_len > 0) {
            switch (rx_pkt.h4_hdr.type) {
                case BT_HCI_H4_TYPE_ACL:
                    hdr_len = 5;
                    break;
                case BT_HCI_H4_TYPE_EVT:
                    hdr_len = 3;
                    break;
                default:
                    printf("# %s unsupported packet type: 0x%02X\n", __FUNCTION__, rx_pkt.h4_hdr.type);
                    continue;
            }
            rx_len += uart_read_bytes(hci_uart_port, (uint8_t *)&rx_pkt + rx_len, hdr_len - rx_len, portMAX_DELAY);
            if (rx_pkt.h4_hdr.type == BT_HCI_H4_TYPE_ACL) {
                total_len = rx_pkt.acl_hdr.len + hdr_len;
            }
            else {
                total_len = rx_pkt.evt_hdr.len + hdr_len;
            }
            if (total_len > sizeof(struct bt_hci_pkt)) {
                total_len = sizeof(struct bt_hci_pkt);
                printf("# %s Compute total_len bigger than max packet size: %ld\n", __FUNCTION__, total_len);
            }
            while (rx_len < total_len) {
                rx_len += uart_read_bytes(hci_uart_port, (uint8_t *)&rx_pkt + rx_len, total_len - rx_len, portMAX_DELAY);
            }
            if (rx_len == total_len) {
                hci_rx_cb((uint8_t *)&rx_pkt, rx_len);
            }
            else {
                printf("# %s Mismatch rx_len: %ld total_len: %ld\n", __FUNCTION__, rx_len, total_len);
            }
        }
    }
}

int hci_uart_init(int port_num, int32_t baud_rate, uint8_t data_bits, uint8_t stop_bits,
    uart_parity_t parity, uart_hw_flowcontrol_t flow_ctl, hci_rx_cb_t hci_rx_cb_func) {
    uart_config_t uart_cfg = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = flow_ctl,
        .source_clk = UART_SCLK_DEFAULT,
        .rx_flow_ctrl_thresh = UART_HW_FIFO_LEN(port_num) - 1,
    };
    hci_uart_port = port_num;
    hci_rx_cb = hci_rx_cb_func;

    printf("# %s: set uart pin tx:%d, rx:%d.\n", __FUNCTION__, HCI_UART_TX_PIN, HCI_UART_RX_PIN);
    printf("# %s: set rts:%d, cts:%d.\n", __FUNCTION__, HCI_UART_RTS_PIN, HCI_UART_CTS_PIN);
    printf("# %s: set baud_rate:%ld.\n", __FUNCTION__, baud_rate);

    ESP_ERROR_CHECK(uart_driver_delete(port_num));
    ESP_ERROR_CHECK(uart_driver_install(port_num, sizeof(struct bt_hci_pkt) * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(port_num, HCI_UART_TX_PIN, HCI_UART_RX_PIN, HCI_UART_RTS_PIN, HCI_UART_CTS_PIN));

    /* Use same priority as ESP BT controller would use otherwise */
    xTaskCreatePinnedToCore(hci_uart_rx_task, "hci_uart_rx_task", 4096, NULL, 23, NULL, 0);
    return 0;
}

void IRAM_ATTR hci_uart_tx(uint8_t *data, uint16_t len) {
    uart_write_bytes(hci_uart_port, data, len);
}
