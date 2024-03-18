/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include <soc/efuse_reg.h>
#include <esp_efuse.h>
#include "system/bare_metal_app_cpu.h"
#include "system/core0_stall.h"
#include "system/delay.h"
#include "system/fs.h"
#include "system/led.h"
#include "adapter/adapter.h"
#include "adapter/adapter_debug.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "wired/detect.h"
#include "wired/wired_common.h"
#include "wired/wired_bare.h"
#include "wired/wired_rtos.h"
#include "adapter/memory_card.h"
#include "system/manager.h"
#include "tests/ws_srv.h"
#include "tests/coverage.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/ets_sys.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#endif

static uint32_t chip_package = 0;

static void wired_init_task(void) {
#ifndef CONFIG_IDF_TARGET_ESP32S2
#ifdef CONFIG_BLUERETRO_SYSTEM_UNIVERSAL
    detect_init();
    while (wired_adapter.system_id <= WIRED_AUTO) {
        if (config.magic == CONFIG_MAGIC && config.global_cfg.system_cfg < WIRED_MAX
            && config.global_cfg.system_cfg != WIRED_AUTO) {
            break;
        }
        delay_us(1000);
    }
    detect_deinit();

    if (wired_adapter.system_id >= 0) {
        ets_printf("# Detected system : %d: %s\n", wired_adapter.system_id, wired_get_sys_name());
    }
#endif

    while (config.magic != CONFIG_MAGIC) {
        delay_us(1000);
    }
#endif /* CONFIG_IDF_TARGET_ESP32S2 */

    if (config.global_cfg.system_cfg < WIRED_MAX && config.global_cfg.system_cfg != WIRED_AUTO) {
        wired_adapter.system_id = config.global_cfg.system_cfg;
        ets_printf("# Config override system : %d: %s\n", wired_adapter.system_id, wired_get_sys_name());
    }

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        adapter_init_buffer(i);
    }

    struct raw_fb fb_data = {0};
    const char *sysname = wired_get_sys_name();
    fb_data.header.wired_id = 0;
    fb_data.header.type = FB_TYPE_SYS_ID;
    fb_data.header.data_len = strlen(sysname);
    memcpy(fb_data.data, sysname, fb_data.header.data_len);
    adapter_q_fb(&fb_data);

#ifndef CONFIG_IDF_TARGET_ESP32S2
    if (wired_adapter.system_id < WIRED_MAX) {
        wired_bare_init(chip_package);
    }
#endif
}

static void wl_init_task(void *arg) {
    uint32_t err = 0;

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_ota_get_state_partition(running, &ota_state);

    chip_package = esp_efuse_get_pkg_ver();

#ifdef CONFIG_BLUERETRO_COVERAGE
    cov_init();
#endif

    err_led_init(chip_package);

#ifdef CONFIG_IDF_TARGET_ESP32
    core0_stall_init();
#endif

#ifndef CONFIG_BLUERETRO_QEMU
    if (fs_init()) {
        err_led_set();
        err = 1;
        printf("FS init fail!\n");
    }
#endif

    /* Initialize NVS for PHY cal and default config */
    int32_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    config_init(DEFAULT_CFG);
    mc_init_mem();

#ifndef CONFIG_BLUERETRO_BT_DISABLE
    if (bt_host_init()) {
        err_led_set();
        err = 1;
        printf("Bluetooth init fail!\n");
    }
#endif

    if (wired_adapter.system_id < WIRED_MAX) {
#ifdef CONFIG_IDF_TARGET_ESP32S2
        wired_init_task();
#endif
        wired_rtos_init();
    }

#ifndef CONFIG_BLUERETRO_QEMU
#ifndef CONFIG_IDF_TARGET_ESP32S2
    mc_init();
#endif

    sys_mgr_init(chip_package);
#endif

#ifdef CONFIG_BLUERETRO_WS_CMDS
    ws_srv_init();
#endif

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (err) {
            printf("Boot error on pending img, rollback to the previous version.");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
        else {
            printf("Pending img valid!");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    vTaskDelete(NULL);
}

void app_main()
{
    adapter_init();

#ifndef CONFIG_BLUERETRO_SYSTEM_UNIVERSAL
    wired_adapter.system_id = HARDCODED_SYS;
    printf("# Hardcoded system : %ld: %s\n", wired_adapter.system_id, wired_get_sys_name());
#endif
#ifndef CONFIG_IDF_TARGET_ESP32S2
    start_app_cpu(wired_init_task);
#endif
    xTaskCreatePinnedToCore(wl_init_task, "wl_init_task", 2560, NULL, 10, NULL, 0);
}
