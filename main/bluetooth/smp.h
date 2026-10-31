/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BT_SMP_H_
#define _BT_SMP_H_

void bt_smp_pairing_start(struct bt_dev *device);
void bt_smp_p256_complete(uint8_t status, const uint8_t key[64]);
void bt_smp_dhkey_complete(uint8_t status, const uint8_t dhkey[32]);
void bt_smp_dev_reset(struct bt_dev *device);
void bt_smp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len);

#endif /* _BT_SMP_H_ */
