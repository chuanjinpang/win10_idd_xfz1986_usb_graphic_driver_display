/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_LCD_H
#define ESP_LCD_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_LCD_H_RES                   (BSP_LCD_H_RES)
#define EXAMPLE_LCD_V_RES                   (BSP_LCD_V_RES)

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define EXAMPLE_LCD_BIT_PER_PIXEL           (16)
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define EXAMPLE_LCD_BIT_PER_PIXEL           (24)
#endif

#define EXAMPLE_LCD_BUF_LEN                 EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_LCD_BIT_PER_PIXEL / 8

/**
 * @brief Initialize the LCD panel.
 *
 * This function initializes the LCD panel with the provided panel handle. It powers on the LCD,
 * installs the LCD driver, configures the bus, and sets up the panel.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failure
 */
esp_err_t app_lcd_init(void);

void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif
