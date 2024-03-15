/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_HCI_UART_H_
#define _BT_HCI_UART_H_

#include "driver/uart.h"

typedef int (*hci_rx_cb_t)(uint8_t *data, uint16_t len);

int hci_uart_init(int port_num, int32_t baud_rate, uint8_t data_bits,
    uint8_t stop_bits, uart_parity_t parity, uart_hw_flowcontrol_t flow_ctl,
    hci_rx_cb_t hci_rx_cb_func);
void hci_uart_tx(uint8_t *data, uint16_t len);

#endif /* _BT_HCI_UART_H_ */
