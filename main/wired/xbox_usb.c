/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_mac.h"
#include "tinyusb.h"
#include "device/usbd_pvt.h"
#include "adapter/adapter.h"
#include "adapter/wired/xbox.h"
#include "xbox_usb.h"

#define XBOX_INTERFACE_CLASS 88
#define XBOX_INTERFACE_SUBCLASS 66
#define XBOX_INTERFACE_PROTOCOL 0

#define XBOX_REPORT_IN_SIZE 20
#define XBOX_REPORT_OUT_SIZE 6
#define XBOX_REPORT_MAX_SIZE 64

static uint8_t ep_out = 0;
static uint8_t ep_in = 0;
static uint8_t ep_out_buf[XBOX_REPORT_MAX_SIZE];

static char serial[13] = {0};

static const tusb_desc_device_t device_desc = {
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x0110,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = XBOX_REPORT_MAX_SIZE,
    .idVendor = 0x303a,
    .idProduct = 0x81eb,
    .bcdDevice = 0x0100,
    .iManufacturer = 0,
    .iProduct = 1,
    .iSerialNumber = 2,
    .bNumConfigurations = 1,
};

static const uint8_t xbox_config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 0x0020, 0x80, 500),
    9, TUSB_DESC_INTERFACE, 0, 0, 2, XBOX_INTERFACE_CLASS, XBOX_INTERFACE_SUBCLASS, XBOX_INTERFACE_PROTOCOL, 0,
    7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(0x0020), 4,
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(0x0020), 4,
};

static const char* hid_string_desc[] = {
    (char[]){0x09, 0x04},
    "BlueRetro X",
    serial,
};

static const uint8_t xbox_vendor_desc[] = {
    0x10, 0x42, 0x00, 0x01, 0x01, 0x02, 0x14, 0x06,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const uint8_t xbox_vendor_out_desc[] = {
    0x00, 0x06, 0xff, 0xff, 0xff, 0xff,
};

static const uint8_t xbox_vendor_in_desc[] = {
    0x00, 0x14, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
};

static void xboxd_init(void) {
}

static void xboxd_reset(uint8_t rhport) {
}

static uint16_t xboxd_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len) {
    TU_VERIFY(
        XBOX_INTERFACE_CLASS == desc_itf->bInterfaceClass &&
        XBOX_INTERFACE_SUBCLASS == desc_itf->bInterfaceSubClass &&
        XBOX_INTERFACE_PROTOCOL == desc_itf->bInterfaceProtocol,
    0);

    uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) +
        desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);


    uint8_t const *p_desc = (uint8_t const *)desc_itf;
    p_desc = tu_desc_next(p_desc);
    TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &ep_out, &ep_in), 0);

    return drv_len;
}

static bool xboxd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage == CONTROL_STAGE_SETUP) {
        switch (request->bmRequestType) {
            case 0x21:
                tud_control_xfer(rhport, request, ep_out_buf, request->wLength);
                break;
            case 0xa1:
            default:
                tud_control_xfer(rhport, request, wired_adapter.data[0].output, request->wLength);
                break;
        }
    }
    else if (stage == CONTROL_STAGE_ACK) {
        switch (request->bmRequestType) {
            case 0x21:
            {
                // printf("%s: Got rumble fb: %02X%02X%02X%02X\n", __FUNCTION__,
                //     ep_out_buf[2], ep_out_buf[3], ep_out_buf[4], ep_out_buf[5]);
                struct raw_fb fb_data = {0};
                fb_data.data[0] = ep_out_buf[3];
                fb_data.data[1] = ep_out_buf[5];
                fb_data.header.wired_id = 0;
                fb_data.header.type = FB_TYPE_RUMBLE;
                fb_data.header.data_len = 2;
                adapter_q_fb(&fb_data);
                break;
            }
        }
    }
    return true;
}

static bool xboxd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    if (ep_addr == ep_out) {
        printf("%s: Got rumble fb: %02X%02X%02X%02X\n", __FUNCTION__,
                    ep_out_buf[2], ep_out_buf[3], ep_out_buf[4], ep_out_buf[5]);
    }

    return true;
}

static usbd_class_driver_t const xbox_driver =
{
#if CFG_TUSB_DEBUG >= 2
    .name = "XBOX",
#endif
    .init             = xboxd_init,
    .reset            = xboxd_reset,
    .open             = xboxd_open,
    .control_xfer_cb  = xboxd_control_xfer_cb,
    .xfer_cb          = xboxd_xfer_cb,
    .sof              = NULL
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    *driver_count = 1;
    return &xbox_driver;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage == CONTROL_STAGE_SETUP) {
        switch (request->bmRequestType) {
            case 0xc1:
                switch (request->bRequest) {
                    case 0x01:
                        if (request->wValue == 0x0100) {
                            tud_control_xfer(rhport, request, (void *)xbox_vendor_in_desc, request->wLength);
                        }
                        else {
                            tud_control_xfer(rhport, request, (void *)xbox_vendor_out_desc, request->wLength);
                        }
                        break;
                    case 0x06:
                    default:
                        tud_control_xfer(rhport, request, (void *)xbox_vendor_desc, request->wLength);
                        break;
                }
                break;
            case 0x01:
            default:
                tud_control_xfer(rhport, request, wired_adapter.data[0].output, request->wLength);
                break;
        }
    }
    return true;
}

void xbox_init(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &device_desc,
        .string_descriptor = hid_string_desc,
        .string_descriptor_count = sizeof(hid_string_desc) / sizeof(hid_string_desc[0]),
        .external_phy = false,
        .configuration_descriptor = xbox_config_desc,
        .self_powered = false,
    };

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X",
        mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

    tinyusb_driver_install(&tusb_cfg);
}

void xbox_send_report(void) {
    if (tud_ready() && !usbd_edpt_busy(0, ep_in)) {
        usbd_edpt_xfer(0, ep_in, wired_adapter.data[0].output, XBOX_REPORT_IN_SIZE);
    }
}

void xbox_port_cfg(uint16_t mask) {

}
