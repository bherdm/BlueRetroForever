/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HID_PARSER_H_
#define _HID_PARSER_H_

/* Apple's Fn (Globe) key: usage 0x03 on Apple's vendor page, which Linux's hid-apple finds on any of
 * three pages. Only an Apple keyboard's counts */
#define HID_APPLE_VID 0x05AC
static inline uint32_t hid_usage_is_apple_fn(uint32_t page, uint32_t usage) {
    return (page == 0x00FF || page == 0xFF00 || page == 0xFF01) && usage == 0x03;
}

void hid_parser(struct bt_data *bt_data, uint8_t *data, uint32_t len);
struct hid_report *hid_parser_get_report(int32_t dev_id, uint8_t report_id);
void hid_parser_load_report(struct bt_data *bt_data, uint8_t report_id);
void hid_parser_free_reports(int32_t dev_id);

#endif /* _HID_PARSER_H_ */
