/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "tusb.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_IDF_TARGET_ESP32P4
#define JPEG_BUFFER_SIZE (300*1024)
#elif CONFIG_IDF_TARGET_ESP32S3
#define JPEG_BUFFER_SIZE (120 * 1024)
#elif CONFIG_IDF_TARGET_ESP32S2
#define JPEG_BUFFER_SIZE (25 * 1024)

#endif

/**
 * @brief Initialize tinyusb device.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: No memory
 */
esp_err_t app_usb_init(void);

#if CFG_TUD_HID
/**
 * @brief Report key press in the keyboard, using array here
 *
 * @param report hid report data
 */
void tinyusb_hid_keyboard_report(hid_report_t report);

esp_err_t app_hid_init(void);
#endif

#if CFG_TUD_VENDOR
esp_err_t app_vendor_init(void);
#endif

#if CFG_TUD_AUDIO
esp_err_t app_uac_init(void);
#endif

#ifdef __cplusplus
}
#endif
