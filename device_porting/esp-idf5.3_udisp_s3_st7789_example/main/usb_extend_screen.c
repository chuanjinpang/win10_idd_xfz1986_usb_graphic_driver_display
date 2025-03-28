/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "bsp.h"
#include "app_usb.h"
#include "usb_descriptors.h"
#include "esp_log.h"
#include "app_lcd.h"
#include "log.h"


static const char *TAG = "ud";

void app_main(void)
{
    ESP_LOGI(TAG, "pcj");
		LOGI( "%s %d %d/%d",__func__,__LINE__,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
	  
    app_usb_init();
		LOGI( "%s %d %d/%d",__func__,__LINE__,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
	  
    app_lcd_init();

    /* The ESP-SparkBot does not support touch functionality. 
     * To enable touch features, consider upgrading to a screen that supports touch input.
     */
    // app_touch_init();
}
