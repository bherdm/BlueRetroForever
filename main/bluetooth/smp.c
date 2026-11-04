/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 *
 * Based on Zephyr's subsys/bluetooth/host/smp.c:
 *   Copyright (c) 2017 Nordic Semiconductor ASA
 *   Copyright (c) 2015-2016 Intel Corporation
 */

#include <stdio.h>
#include <esp_random.h>
#include <mbedtls/cmac.h>
#include <mbedtls/cipher.h>
#include "host.h"
#include "hci.h"
#include "att_hid.h"
#include "smp.h"
#include "zephyr/smp.h"

static void bt_smp_mrand_half(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_mrand_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_confirm_half(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_confirm_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_stk_complete(struct bt_dev *device, uint8_t *data, uint32_t len);
static void bt_smp_key_distribution(struct bt_dev *device);
static void bt_smp_pairing_req(uint16_t handle, struct bt_smp_pairing *pairing_req);
static void bt_smp_pairing_confirm(uint16_t handle, uint8_t val[16]);
static void bt_smp_encrypt_info(uint16_t handle, uint8_t ltk[16]);
static void bt_smp_master_ident(uint16_t handle, uint8_t ediv[2], uint8_t rand[8]);
static void bt_smp_ident_info(uint16_t handle, uint8_t irk[16]);
static void bt_smp_ident_addr_info(uint16_t handle, bt_addr_le_t *le_bdaddr);
static void bt_smp_signing_info(uint16_t handle, uint8_t csrk[16]);
static void bt_smp_cmd(uint16_t handle, uint8_t code, uint16_t len);
static struct bt_dev *sc_device;

enum {
    BT_SC_DHKEY_READY = 1 << 0,
    BT_SC_RANDOM_READY = 1 << 1,
    BT_SC_DHCHECK_SENT = 1 << 2,
};

static void mem_swap_copy(uint8_t *dst, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

static int sc_cmac(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[16]) {
    const mbedtls_cipher_info_t *info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
    return info ? mbedtls_cipher_cmac(info, key, 128, msg, len, out) : -1;
}

static int sc_f4(const uint8_t u[32], const uint8_t v[32], const uint8_t x[16],
        uint8_t z, uint8_t out[16]) {
    uint8_t key[16], msg[65], tmp[16];
    mem_swap_copy(msg, u, 32);
    mem_swap_copy(msg + 32, v, 32);
    msg[64] = z;
    mem_swap_copy(key, x, 16);
    if (sc_cmac(key, msg, sizeof(msg), tmp)) return -1;
    mem_swap_copy(out, tmp, 16);
    return 0;
}

static int sc_f5(const uint8_t w[32], const uint8_t n1[16], const uint8_t n2[16],
        const bt_addr_le_t *a1, const bt_addr_le_t *a2, uint8_t mackey[16], uint8_t ltk[16]) {
    static const uint8_t salt[16] = {0x6c,0x88,0x83,0x91,0xaa,0xf5,0xa5,0x38,
        0x60,0x37,0x0b,0xdb,0x5a,0x60,0x83,0xbe};
    uint8_t ws[32], t[16], msg[53] = {0}, tmp[16];
    msg[1]=0x62; msg[2]=0x74; msg[3]=0x6c; msg[4]=0x65; msg[51]=0x01;
    mem_swap_copy(ws, w, 32);
    if (sc_cmac(salt, ws, sizeof(ws), t)) return -1;
    mem_swap_copy(msg + 5, n1, 16); mem_swap_copy(msg + 21, n2, 16);
    msg[37]=a1->type; mem_swap_copy(msg + 38, a1->a.val, 6);
    msg[44]=a2->type; mem_swap_copy(msg + 45, a2->a.val, 6);
    if (sc_cmac(t, msg, sizeof(msg), tmp)) return -1;
    mem_swap_copy(mackey, tmp, 16);
    msg[0]=1;
    if (sc_cmac(t, msg, sizeof(msg), tmp)) return -1;
    mem_swap_copy(ltk, tmp, 16);
    return 0;
}

static int sc_f6(const uint8_t w[16], const uint8_t n1[16], const uint8_t n2[16],
        const uint8_t r[16], const uint8_t iocap[3], const bt_addr_le_t *a1,
        const bt_addr_le_t *a2, uint8_t out[16]) {
    uint8_t key[16], msg[65], tmp[16];
    mem_swap_copy(msg, n1, 16); mem_swap_copy(msg + 16, n2, 16);
    mem_swap_copy(msg + 32, r, 16); mem_swap_copy(msg + 48, iocap, 3);
    msg[51]=a1->type; mem_swap_copy(msg + 52, a1->a.val, 6);
    msg[58]=a2->type; mem_swap_copy(msg + 59, a2->a.val, 6);
    mem_swap_copy(key, w, 16);
    if (sc_cmac(key, msg, sizeof(msg), tmp)) return -1;
    mem_swap_copy(out, tmp, 16);
    return 0;
}

static void bt_smp_dhkey_check(uint16_t handle, const uint8_t check[16]) {
    memcpy(bt_hci_pkt_tmp.smp_data, check, sizeof(struct bt_smp_dhkey_check));
    bt_smp_cmd(handle, BT_SMP_DHKEY_CHECK, sizeof(struct bt_smp_dhkey_check));
}

static int sc_send_dhcheck(struct bt_dev *device) {
    bt_addr_le_t local;
    uint8_t zero[16] = {0}, check[16];
    bt_hci_get_le_local_addr(&local);
    if (sc_f5(device->sc_dhkey, device->rand, device->rrand, &local,
            &device->le_remote_bdaddr, device->sc_mackey, device->ltk) ||
        sc_f6(device->sc_mackey, device->rand, device->rrand, zero,
            &device->preq[1], &local, &device->le_remote_bdaddr, check)) return -1;
    bt_smp_dhkey_check(device->acl_handle, check);
    device->sc_state |= BT_SC_DHCHECK_SENT;
    return 0;
}

static void bt_smp_public_key(uint16_t handle, const uint8_t key[64]) {
    memcpy(bt_hci_pkt_tmp.smp_data, key, sizeof(struct bt_smp_public_key));
    bt_smp_cmd(handle, BT_SMP_CMD_PUBLIC_KEY, sizeof(struct bt_smp_public_key));
}

/* A Secure Connections pairing that can't go on ends with Pairing Failed, so the
 * controller can retry at once instead of waiting for a timeout. */
static void bt_smp_sc_fail(struct bt_dev *device, uint8_t reason) {
    struct bt_smp_pairing_fail *fail = (struct bt_smp_pairing_fail *)bt_hci_pkt_tmp.smp_data;
    fail->reason = reason;
    bt_smp_cmd(device->acl_handle, BT_SMP_CMD_PAIRING_FAIL, sizeof(*fail));
    sc_device = NULL;
}

static void xor_128(const uint8_t p[16], const uint8_t q[16], uint8_t r[16]) {
    size_t len = 16;

    while (len--) {
        *r++ = *p++ ^ *q++;
    }
}

static int32_t smp_c1_part1(struct bt_dev *device, const uint8_t k[16], const uint8_t r[16],
          const uint8_t preq[7], const uint8_t pres[7],
          const bt_addr_le_t *ia, const bt_addr_le_t *ra,
          uint8_t enc_data[16]) {
    uint8_t p1[16];

    /* pres, preq, rat and iat are concatenated to generate p1 */
    p1[0] = ia->type;
    p1[1] = ra->type;
    memcpy(p1 + 2, preq, 7);
    memcpy(p1 + 9, pres, 7);

    /* c1 = e(k, e(k, r XOR p1) XOR p2) */

    /* Using enc_data as temporary output buffer */
    xor_128(r, p1, enc_data);

    return bt_hci_get_encrypt(device, bt_smp_confirm_half, k, enc_data);
}

static int32_t smp_c1_part2(struct bt_dev *device, const uint8_t k[16], const uint8_t r[16],
          const uint8_t preq[7], const uint8_t pres[7],
          const bt_addr_le_t *ia, const bt_addr_le_t *ra,
          uint8_t enc_data[16]) {
    uint8_t p2[16];

    /* ra is concatenated with ia and padding to generate p2 */
    memcpy(p2, ra->a.val, 6);
    memcpy(p2 + 6, ia->a.val, 6);
    (void)memset(p2 + 12, 0, 4);

    xor_128(enc_data, p2, enc_data);

    return bt_hci_get_encrypt(device, bt_smp_confirm_complete, k, enc_data);
}

static int32_t smp_s1(struct bt_dev *device, const uint8_t k[16], const uint8_t r1[16],
          const uint8_t r2[16], uint8_t out[16])
{
    /* The most significant 64-bits of r1 are discarded to generate
     * r1' and the most significant 64-bits of r2 are discarded to
     * generate r2'.
     * r1' is concatenated with r2' to generate r' which is used as
     * the 128-bit input parameter plaintextData to security function e:
     *
     *    r' = r1' || r2'
     */
    memcpy(out, r2, 8);
    memcpy(out + 8, r1, 8);

    /* s1(k, r1 , r2) = e(k, r') */
    return bt_hci_get_encrypt(device, bt_smp_stk_complete, k, out);
}

static void bt_smp_mrand_half(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->rand, data, len);
}

static void bt_smp_mrand_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    bt_addr_le_t le_local;
    uint8_t tk[16] = {0};

    bt_hci_get_le_local_addr(&le_local);

    memcpy(device->rand + 8, data, len);
    smp_c1_part1(device, tk, device->rand, device->preq, device->pres, &le_local, &device->le_remote_bdaddr, device->ltk);
}

static void bt_smp_confirm_half(struct bt_dev *device, uint8_t *data, uint32_t len) {
    bt_addr_le_t le_local;
    uint8_t tk[16] = {0};

    bt_hci_get_le_local_addr(&le_local);

    memcpy(device->ltk, data, len);
    smp_c1_part2(device, tk, device->rand, device->preq, device->pres, &le_local, &device->le_remote_bdaddr, device->ltk);
}

static void bt_smp_confirm_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->ltk, data, len);
    bt_smp_pairing_confirm(device->acl_handle, device->ltk);
}

static void bt_smp_stk_complete(struct bt_dev *device, uint8_t *data, uint32_t len) {
    memcpy(device->ltk, data, len);
    bt_hci_start_encryption(device->acl_handle, 0, 0, device->ltk);
}

static void bt_smp_key_distribution(struct bt_dev *device) {
    /* Unused keys IFAICT, so just use ESP random function rather than BT ctrl one */
    uint8_t tmp[16] = {0};

    if (device->ldist & BT_SMP_DIST_ENC_KEY) {
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_encrypt_info(device->acl_handle, tmp);
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_master_ident(device->acl_handle, tmp, tmp + 2);
    }
    if (device->ldist & BT_SMP_DIST_ID_KEY) {
        bt_addr_le_t le_local;
        bt_hci_get_le_local_addr(&le_local);
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_ident_info(device->acl_handle, tmp);
        bt_smp_ident_addr_info(device->acl_handle, &le_local);
    }
    if (device->ldist & BT_SMP_DIST_SIGN) {
        esp_fill_random(tmp, sizeof(tmp));
        bt_smp_signing_info(device->acl_handle, tmp);
    }

    bt_att_hid_init(device);
}

static void bt_smp_cmd(uint16_t handle, uint8_t code, uint16_t len) {
    uint16_t packet_len = (BT_HCI_H4_HDR_SIZE + BT_HCI_ACL_HDR_SIZE
        + sizeof(struct bt_l2cap_hdr) + sizeof(struct bt_smp_hdr) + len);

    bt_hci_pkt_tmp.h4_hdr.type = BT_HCI_H4_TYPE_ACL;

    bt_hci_pkt_tmp.acl_hdr.handle = bt_acl_handle_pack(handle, BT_ACL_START_NO_FLUSH);
    bt_hci_pkt_tmp.acl_hdr.len = packet_len - BT_HCI_H4_HDR_SIZE - BT_HCI_ACL_HDR_SIZE;

    bt_hci_pkt_tmp.l2cap_hdr.len = bt_hci_pkt_tmp.acl_hdr.len - sizeof(bt_hci_pkt_tmp.l2cap_hdr);
    bt_hci_pkt_tmp.l2cap_hdr.cid = BT_L2CAP_CID_SMP;

    bt_hci_pkt_tmp.smp_hdr.code = code;

    bt_host_txq_add((uint8_t *)&bt_hci_pkt_tmp, packet_len);
}

static void bt_smp_pairing_req(uint16_t handle, struct bt_smp_pairing *pairing_req) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, (uint8_t *)pairing_req, sizeof(*pairing_req));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_REQ, sizeof(*pairing_req));
}

static void bt_smp_pairing_confirm(uint16_t handle, uint8_t val[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, val, sizeof(struct bt_smp_pairing_confirm));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_CONFIRM, sizeof(struct bt_smp_pairing_confirm));
}

static void bt_smp_pairing_random(uint16_t handle, uint8_t val[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, val, sizeof(struct bt_smp_pairing_random));

    bt_smp_cmd(handle, BT_SMP_CMD_PAIRING_RANDOM, sizeof(struct bt_smp_pairing_random));
}

static void bt_smp_encrypt_info(uint16_t handle, uint8_t ltk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, ltk, sizeof(struct bt_smp_encrypt_info));

    bt_smp_cmd(handle, BT_SMP_CMD_ENCRYPT_INFO, sizeof(struct bt_smp_pairing_random));
}

static void bt_smp_master_ident(uint16_t handle, uint8_t ediv[2], uint8_t rand[8]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, ediv, 2);
    memcpy(bt_hci_pkt_tmp.smp_data + 2, rand, 8);

    bt_smp_cmd(handle, BT_SMP_CMD_MASTER_IDENT, sizeof(struct bt_smp_master_ident));
}

static void bt_smp_ident_info(uint16_t handle, uint8_t irk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, irk, sizeof(struct bt_smp_ident_info));

    bt_smp_cmd(handle, BT_SMP_CMD_IDENT_INFO, sizeof(struct bt_smp_ident_info));
}

static void bt_smp_ident_addr_info(uint16_t handle, bt_addr_le_t *le_bdaddr) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, le_bdaddr, sizeof(struct bt_smp_ident_addr_info));

    bt_smp_cmd(handle, BT_SMP_CMD_IDENT_ADDR_INFO, sizeof(struct bt_smp_ident_addr_info));
}

static void bt_smp_signing_info(uint16_t handle, uint8_t csrk[16]) {
    printf("# %s\n", __FUNCTION__);

    memcpy(bt_hci_pkt_tmp.smp_data, csrk, sizeof(struct bt_smp_signing_info));

    bt_smp_cmd(handle, BT_SMP_CMD_SIGNING_INFO, sizeof(struct bt_smp_signing_info));
}

void bt_smp_pairing_start(struct bt_dev *device) {
    struct bt_smp_pairing pairing_req = {
        .io_capability = BT_SMP_IO_NO_INPUT_OUTPUT,
        .oob_flag = BT_SMP_OOB_NOT_PRESENT,
        .auth_req = BT_SMP_AUTH_BONDING,
        .max_key_size = BT_SMP_MAX_ENC_KEY_SIZE,
        .init_key_dist = BT_SMP_DIST_ID_KEY,
        .resp_key_dist = BT_SMP_DIST_ENC_KEY,
    };

    if (atomic_test_bit(&device->flags, BT_DEV_LE_SC_REQUIRED)) {
        pairing_req.auth_req |= BT_SMP_AUTH_SC;
        /* The SC LTK is derived, not distributed with Encryption Information. */
        pairing_req.init_key_dist &= ~BT_SMP_DIST_ENC_KEY;
        pairing_req.resp_key_dist &= ~BT_SMP_DIST_ENC_KEY;
    }
    device->preq[0] = BT_SMP_CMD_PAIRING_REQ;
    memcpy(&device->preq[1], (uint8_t *)&pairing_req, sizeof(pairing_req));
    bt_smp_pairing_req(device->acl_handle, &pairing_req);
}

void bt_smp_hdlr(struct bt_dev *device, struct bt_hci_pkt *bt_hci_acl_pkt, uint32_t len) {
    switch (bt_hci_acl_pkt->smp_hdr.code) {
        case BT_SMP_CMD_PAIRING_RSP:
        {
            struct bt_smp_pairing *pairing_rsp = (struct bt_smp_pairing *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_RSP\n");

            device->ldist = pairing_rsp->init_key_dist;
            device->rdist = pairing_rsp->resp_key_dist;
            device->pres[0] = BT_SMP_CMD_PAIRING_RSP;
            memcpy(&device->pres[1], (uint8_t *)pairing_rsp, sizeof(*pairing_rsp));

            if (atomic_test_bit(&device->flags, BT_DEV_LE_SC_REQUIRED)
                    && (pairing_rsp->auth_req & BT_SMP_AUTH_SC)) {
                sc_device = device;
                device->sc_state = 0;
                esp_fill_random(device->rand, sizeof(device->rand));
                bt_hci_le_read_public_key();
            }
            else {
                bt_hci_get_random(device, bt_smp_mrand_half);
                bt_hci_get_random(device, bt_smp_mrand_complete);
            }
            break;
        }
        case BT_SMP_CMD_PUBLIC_KEY:
            if (device == sc_device) {
                memcpy(device->sc_peer_public_key, bt_hci_acl_pkt->smp_data,
                    sizeof(device->sc_peer_public_key));
                bt_hci_le_generate_dhkey(device->sc_peer_public_key);
            }
            break;
        case BT_SMP_CMD_PAIRING_CONFIRM:
        {
            printf("# BT_SMP_CMD_PAIRING_CONFIRM\n");
            if (device == sc_device) {
                memcpy(device->sc_peer_confirm, bt_hci_acl_pkt->smp_data,
                    sizeof(device->sc_peer_confirm));
            }
            bt_smp_pairing_random(device->acl_handle, device->rand);
            break;
        }
        case BT_SMP_CMD_PAIRING_RANDOM:
        {
            struct bt_smp_pairing_random *pairing_random = (struct bt_smp_pairing_random *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_RANDOM\n");

            if (device == sc_device) {
                uint8_t expected[16];
                memcpy(device->rrand, pairing_random->val, sizeof(device->rrand));
                if (sc_f4(device->sc_peer_public_key, device->sc_public_key,
                        device->rrand, 0, expected) ||
                        memcmp(expected, device->sc_peer_confirm, sizeof(expected))) {
                    printf("# LE Secure Connections confirm validation failed\n");
                    bt_smp_sc_fail(device, BT_SMP_ERR_CONFIRM_FAILED);
                    break;
                }
                device->sc_state |= BT_SC_RANDOM_READY;
                if ((device->sc_state & BT_SC_DHKEY_READY) && sc_send_dhcheck(device)) {
                    printf("# LE Secure Connections key derivation failed\n");
                    bt_smp_sc_fail(device, BT_SMP_ERR_UNSPECIFIED);
                }
                break;
            }

            uint8_t tk[16] = {0};

            /* We should validate remote confirm here, but we are lazy */

            memcpy(device->rrand, pairing_random->val, sizeof(device->rrand));
            smp_s1(device, tk, device->rrand, device->rand, device->ltk);

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        }
        case BT_SMP_CMD_PAIRING_FAIL:
        {
            struct bt_smp_pairing_fail *pairing_fail = (struct bt_smp_pairing_fail *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_PAIRING_FAIL reason: %02X\n", pairing_fail->reason);
            if (device == sc_device) sc_device = NULL;
            break;
        }
        case BT_SMP_DHKEY_CHECK:
        {
            bt_addr_le_t local;
            struct bt_smp_dhkey_check *remote = (void *)bt_hci_acl_pkt->smp_data;
            uint8_t zero[16] = {0}, expected[16];
            struct bt_smp_encrypt_info info = {0};
            struct bt_smp_master_ident ident = {0};
            if (device != sc_device || !(device->sc_state & BT_SC_DHCHECK_SENT)) break;
            bt_hci_get_le_local_addr(&local);
            if (sc_f6(device->sc_mackey, device->rrand, device->rand, zero,
                    &device->pres[1], &device->le_remote_bdaddr, &local, expected) ||
                    memcmp(expected, remote->e, sizeof(expected))) {
                printf("# LE Secure Connections DHKey check failed\n");
                bt_smp_sc_fail(device, BT_SMP_ERR_DHKEY_CHECK_FAILED);
                break;
            }
            memcpy(info.ltk, device->ltk, sizeof(info.ltk));
            bt_host_store_le_ltk(&device->le_remote_bdaddr, &info);
            bt_host_store_le_ident(&device->le_remote_bdaddr, &ident);
            bt_hci_add_to_accept_list(&device->le_remote_bdaddr);
            bt_hci_start_encryption(device->acl_handle, 0, 0, device->ltk);
            sc_device = NULL;
            break;
        }
        case BT_SMP_CMD_ENCRYPT_INFO:
        {
            struct bt_smp_encrypt_info *encrypt_info = (struct bt_smp_encrypt_info *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_ENCRYPT_INFO\n");

            bt_host_store_le_ltk(&device->le_remote_bdaddr, encrypt_info);
            break;
        }
        case BT_SMP_CMD_MASTER_IDENT:
        {
            struct bt_smp_master_ident *master_ident = (struct bt_smp_master_ident *)bt_hci_acl_pkt->smp_data;
            printf("# BT_SMP_CMD_MASTER_IDENT\n");

            device->rdist &= ~BT_SMP_DIST_ENC_KEY;

            bt_host_store_le_ident(&device->le_remote_bdaddr, master_ident);

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            
            bt_hci_add_to_accept_list(&device->le_remote_bdaddr);
            break;
        }
        case BT_SMP_CMD_IDENT_INFO:
            printf("# BT_SMP_CMD_IDENT_INFO\n");
            break;
        case BT_SMP_CMD_IDENT_ADDR_INFO:
            printf("# BT_SMP_CMD_IDENT_ADDR_INFO\n");

            device->rdist &= ~BT_SMP_DIST_ID_KEY;

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        case BT_SMP_CMD_SIGNING_INFO:
            printf("# BT_SMP_CMD_SIGNING_INFO\n");

            device->rdist &= ~BT_SMP_DIST_SIGN;

            if (!device->rdist) {
                bt_smp_key_distribution(device);
            }
            break;
        default:
            printf("# Unsupported OPCODE: 0x%02X\n", bt_hci_acl_pkt->smp_hdr.code);
    }
}

void bt_smp_p256_complete(uint8_t status, const uint8_t key[64]) {
    if (!sc_device) return;
    if (status) {
        printf("# LE P-256 public key generation failed: 0x%02X\n", status);
        bt_smp_sc_fail(sc_device, BT_SMP_ERR_UNSPECIFIED);
        return;
    }
    memcpy(sc_device->sc_public_key, key, sizeof(sc_device->sc_public_key));
    bt_smp_public_key(sc_device->acl_handle, sc_device->sc_public_key);
}

void bt_smp_dhkey_complete(uint8_t status, const uint8_t dhkey[32]) {
    if (!sc_device) return;
    if (status) {
        printf("# LE DHKey generation failed: 0x%02X\n", status);
        bt_smp_sc_fail(sc_device, BT_SMP_ERR_DHKEY_CHECK_FAILED);
        return;
    }
    memcpy(sc_device->sc_dhkey, dhkey, sizeof(sc_device->sc_dhkey));
    sc_device->sc_state |= BT_SC_DHKEY_READY;
    if ((sc_device->sc_state & BT_SC_RANDOM_READY) && sc_send_dhcheck(sc_device)) {
        printf("# LE Secure Connections key derivation failed\n");
        bt_smp_sc_fail(sc_device, BT_SMP_ERR_UNSPECIFIED);
    }
}

/* A device that disconnects mid-pairing takes its Secure Connections state with it,
 * so a late P-256 or DHKey event can't act on a reused device slot. */
void bt_smp_dev_reset(struct bt_dev *device) {
    if (device == sc_device) sc_device = NULL;
}
