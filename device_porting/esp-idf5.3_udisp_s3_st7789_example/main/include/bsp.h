/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_BSP_H
#define ESP_BSP_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"


#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES 240
#define BSP_LCD_V_RES  240

#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI3_HOST)

	/* Display */
#if CONFIG_IDF_TARGET_ESP32S2
#define BSP_LCD_SPI_MOSI      (GPIO_NUM_34)
#define BSP_LCD_SPI_CLK       (GPIO_NUM_33)
#define BSP_LCD_SPI_CS        (GPIO_NUM_37)
#define BSP_LCD_DC            (GPIO_NUM_38)
#define BSP_LCD_RST           (GPIO_NUM_1)
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_5)
	
#else
	
#define BSP_LCD_SPI_MOSI      (GPIO_NUM_6)
#define BSP_LCD_SPI_CLK       (GPIO_NUM_5)
#define BSP_LCD_SPI_CS        (GPIO_NUM_7)
#define BSP_LCD_DC            (GPIO_NUM_8)
#define BSP_LCD_RST           (GPIO_NUM_1)
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_4)
#endif


#ifdef __cplusplus
}
#endif

#endif
