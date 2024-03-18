/*
 * Copyright (c) 2021-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WIRED_BARE_H_
#define _WIRED_BARE_H_

#include <soc/spi_periph.h>

struct spi_cfg {
    spi_dev_t *hw;
    uint32_t write_bit_order;
    uint32_t read_bit_order;
    uint32_t clk_idle_edge;
    uint32_t clk_i_edge;
    uint32_t miso_delay_mode;
    uint32_t miso_delay_num;
    uint32_t mosi_delay_mode;
    uint32_t mosi_delay_num;
    uint32_t write_bit_len;
    uint32_t read_bit_len;
    uint32_t inten;
};

void wired_bare_init(uint32_t package);
void wired_bare_port_cfg(uint16_t mask);
void spi_init(struct spi_cfg *cfg);

#endif /* _WIRED_BARE_H_ */
