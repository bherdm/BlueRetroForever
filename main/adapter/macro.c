/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/types.h"
#include "tools/util.h"
#include "system/manager.h"
#include "config.h"
#include "macro.h"
#include "adapter.h"
#include "bluetooth/host.h"

#define BIT_32(n) (1UL << ((n) & 0x1F))

#define MACRO_BASE (BIT_32(BR_COMBO_BASE_1))

enum {
    MENU_STATE_INACTIVE = 0,
    MENU_STATE_WAIT_OPTIONS,
    MENU_STATE_WAIT_MAP_SRC,
    MENU_STATE_WAIT_MAP_DEST,
    MENU_STATE_WAIT_TURBO,
};

static int32_t menu_state = MENU_STATE_INACTIVE;
static int32_t map_src = BTN_NONE;
static int32_t map_dst = BTN_NONE;

static void update_mapping(uint32_t index) {
    uint32_t idx = index + config.global_cfg.banksel;
    struct map_cfg *mapping = config_get_first_map_for_src(idx, map_src);
    if (mapping) {
        mapping->dst_btn = map_dst;
        printf("# %s: idx: %ld Change mapping src: %ld dst: %ld\n", __FUNCTION__, idx, map_src, map_dst);
        sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
    }
}

static void update_turbo(uint32_t index, int32_t btn_id) {
    uint32_t idx = index + config.global_cfg.banksel;
    struct map_cfg *mapping = config_get_first_map_for_src(idx, btn_id);
    if (mapping) {
        switch (mapping->turbo) {
            case 0: /* Disable */
                mapping->turbo = 8;
                sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_SLOW);
                break;
            case 8: /* 4/8 frames */
                mapping->turbo = 4;
                sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_FAST);
                break;
            case 4: /* 2/4 frames */
                mapping->turbo = 2;
                sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_FASTER);
                break;
            default:
                mapping->turbo = 0;
                sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_ON);
                break;
        }
        printf("# %s: idx: %ld Set turbo on btn: %ld val: %d\n", __FUNCTION__, idx, btn_id, mapping->turbo);
    }
}

static void reset_active_mapping(uint32_t index) {
    uint32_t idx = index + config.global_cfg.banksel;
    config_init_mapping_bank(&config, idx);
    printf("# %s: idx: %ld Reset mapping config %d to default\n", __FUNCTION__, idx, config.global_cfg.banksel);
    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
}

static void update_cfg_acc(uint32_t index) {
    if (config.out_cfg[index].acc_mode == ACC_NONE) {
        config.out_cfg[index].acc_mode = ACC_RUMBLE;
        printf("# %s: Set rumble feedback\n", __FUNCTION__);
    }
    else {
        config.out_cfg[index].acc_mode = ACC_NONE;
        printf("# %s: Clear rumble feedback\n", __FUNCTION__);
    }
    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
}

static void update_calib(uint32_t index) {
    struct bt_dev *device = NULL;
    struct bt_data *bt_data = NULL;

    bt_host_get_active_dev_from_out_idx(index, &device);
    if (device) {
        bt_data = &bt_adapter.data[device->ids.id];
        if (bt_data) {
            atomic_set_bit(&bt_data->base.flags[PAD], BT_AXES_CALIB_EN);
            atomic_clear_bit(&bt_data->base.flags[PAD], BT_INIT);
            printf("# %s: Axes calib\n", __FUNCTION__);
        }
    }
}

int32_t sys_macro_hdl(struct wired_ctrl *ctrl_data, atomic_t *flags) {
    /* Wait for all buttons to release before running state machine again */
    if (atomic_test_bit(flags, BT_WAITING_FOR_RELEASE_MACRO1)) {
        for (uint32_t i = 0; i < 4; i++) {
            if (ctrl_data->btns[i].value) {
                return 1;
            }
        }
        atomic_clear_bit(flags, BT_WAITING_FOR_RELEASE_MACRO1);
        if (menu_state == MENU_STATE_INACTIVE) {
            printf("# %s: Exiting adapter menu\n", __FUNCTION__);
            adapter_toggle_fb(ctrl_data->index, 300000, 0xFF, 0xFF);
            sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_OFF);
        }
        else {
            adapter_toggle_fb(ctrl_data->index, 150000, 0xFF, 0xFF);
        }
    }

    bool menu_btn = (ctrl_data->btns[BR_COMBO_IDX].value == MACRO_BASE);
    if (menu_btn) {
        if (menu_state == MENU_STATE_INACTIVE) {
            menu_state = MENU_STATE_WAIT_OPTIONS;
            printf("# %s: Entering adapter menu\n", __FUNCTION__);
            sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_ON);
        }
        else {
            if (menu_state == MENU_STATE_WAIT_TURBO) {
                sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
            }
            menu_state = MENU_STATE_INACTIVE;
        }
        atomic_set_bit(flags, BT_WAITING_FOR_RELEASE_MACRO1);
        return 1;
    }

    int32_t btn_id = __builtin_ffs(ctrl_data->btns[0].value);
    btn_id--;
    if (btn_id > BTN_NONE && menu_state != MENU_STATE_INACTIVE) {
        atomic_set_bit(flags, BT_WAITING_FOR_RELEASE_MACRO1);
    }

    switch (menu_state) {
        case MENU_STATE_WAIT_OPTIONS:
            switch (btn_id) {
                case PAD_LD_UP:
                    config.global_cfg.banksel = 0;
                    menu_state = MENU_STATE_INACTIVE;
                    printf("# %s: Set mapping bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
                    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
                    break;
                case PAD_LD_LEFT:
                    config.global_cfg.banksel = 1;
                    menu_state = MENU_STATE_INACTIVE;
                    printf("# %s: Set mapping bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
                    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
                    break;
                case PAD_LD_RIGHT:
                    config.global_cfg.banksel = 2;
                    menu_state = MENU_STATE_INACTIVE;
                    printf("# %s: Set mapping bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
                    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
                    break;
                case PAD_LD_DOWN:
                    config.global_cfg.banksel = 3;
                    menu_state = MENU_STATE_INACTIVE;
                    printf("# %s: Set mapping bank to %d\n", __FUNCTION__, config.global_cfg.banksel);
                    sys_mgr_cmd(SYS_MGR_CMD_SAVE_CONFIG);
                    break;
                case PAD_MM:
                    menu_state = MENU_STATE_WAIT_MAP_SRC;
                    printf("# %s: Waiting for mapping source btn\n", __FUNCTION__);
                    break;
                case PAD_MS:
                    menu_state = MENU_STATE_WAIT_TURBO;
                    printf("# %s: Waiting for turbo config\n", __FUNCTION__);
                    break;
                case PAD_RB_UP:
                    update_calib(ctrl_data->index);
                    menu_state = MENU_STATE_INACTIVE;
                    break;
                case PAD_RB_LEFT:
                    update_cfg_acc(ctrl_data->index);
                    menu_state = MENU_STATE_INACTIVE;
                    break;
                case PAD_RB_RIGHT:
                    sys_mgr_cmd(SYS_MGR_CMD_INQ_TOOGLE);
                    menu_state = MENU_STATE_INACTIVE;
                    break;
                case PAD_RB_DOWN:
                    sys_mgr_cmd(SYS_MGR_CMD_PWR_OFF);
                    menu_state = MENU_STATE_INACTIVE;
                    sys_mgr_cmd(SYS_MGR_CMD_FLASH_LED_OFF);
                    break;
                case PAD_LM:
                case PAD_LS:
                case PAD_RM:
                case PAD_RS:
                    reset_active_mapping(ctrl_data->index);
                    menu_state = MENU_STATE_INACTIVE;
                    break;
            }
            break;
        case MENU_STATE_WAIT_MAP_SRC:
            if (btn_id > BTN_NONE) {
                map_src = btn_id;
                menu_state = MENU_STATE_WAIT_MAP_DEST;
                printf("# %s: Map src: %ld\n", __FUNCTION__, map_src);
            }
            break;
        case MENU_STATE_WAIT_MAP_DEST:
            if (btn_id > BTN_NONE) {
                map_dst = btn_id;
                menu_state = MENU_STATE_INACTIVE;
                update_mapping(ctrl_data->index);
            }
            break;
        case MENU_STATE_WAIT_TURBO:
            if (btn_id > BTN_NONE) {
                update_turbo(ctrl_data->index, btn_id);
            }
            break;
        default:
            return 0;
    }
    return 1;
}
