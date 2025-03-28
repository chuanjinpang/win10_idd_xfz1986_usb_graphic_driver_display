/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "app_lcd.h"
#include "app_usb.h"
#include "bsp.h"
#include "display.h"

#include "esp_vfs_fat.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/i2c.h"


#include "esp_timer.h"
#include "decode_jpeg.h"
#include "log.h"
#define LCD_BUFFER_NUMS 1

static const char *TAG = "app_lcd";

static esp_lcd_panel_handle_t       display_handle;
static esp_lcd_panel_io_handle_t    ret_io;

static void *lcd_buffer[LCD_BUFFER_NUMS];
static uint8_t buf_index = 0;

uint8_t * cur_uframe=NULL;
int cur_uframe_pos=0;
pixel_jpeg * lcd_frame=NULL;

size_t jpeg_stream_read(uint8_t * msg, int len, int no_less_flg)
{
	//LOGI("ofs:%d len:%d msg:%p %d\n",cur_uframe_pos,len,msg,no_less_flg);
    if(msg) {
		memcpy(msg,&cur_uframe[cur_uframe_pos],len);
		cur_uframe_pos+=len;
		return len;

    } else {
		cur_uframe_pos+=len;//just drop;
				return len;
    }
}

void jpeg_bitblt(pixel_jpeg * pix_buf, JRECT *rect)
{
//LOGI("ofs:%p %d %d %d %d\n",&lcd_frame[rect->top*BSP_LCD_H_RES], rect->left,  rect->top, rect->right + 1, rect->bottom + 1);
memcpy(&lcd_frame[rect->top*BSP_LCD_H_RES],pix_buf,BSP_LCD_H_RES*(rect->bottom-rect->top)*2);

}

#if 0
static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
	esp_err_t ret = ESP_OK;

   
	lcd_frame=(pixel_jpeg *)output_buf;
	cur_uframe=input_buf;
	cur_uframe_pos=0;
	decode_jpeg(NULL,NULL);



    return ret;
}
#else


#include "esp_jpeg_dec.h"

static int esp_jpeg_decoder_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf)
{
	esp_err_t ret = ESP_OK;

	//LOGI( "%s %d %p len:%d %p",__func__,__LINE__,input_buf,len,output_buf);

    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }
	

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        return ESP_FAIL;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        return ESP_FAIL;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;
	//LOGI( "%s %d \n",__func__,__LINE__);

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        goto _exit;
    }


    jpeg_io->outbuf = output_buf;
	//LOGI( "%s %d \n",__func__,__LINE__);

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        goto _exit;
    }


_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
 
    return ret;
}
#endif


void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height)
{
    static int fps_count = 0;
    static int64_t start_time = 0;
	int64_t e=0,d=0;
	int64_t s=esp_timer_get_time();
    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }

    esp_jpeg_decoder_one_picture((uint8_t *)buf, len, lcd_buffer[buf_index]);
	e=esp_timer_get_time();
	ESP_LOGI(TAG,"dec:%d\n",(int)(e-s));
    esp_lcd_panel_draw_bitmap(display_handle, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, lcd_buffer[buf_index]);
	d=esp_timer_get_time();
	ESP_LOGI(TAG,"dec:%d disp:%d t:%d\n",(int)(e-s),(int)(d-e),(int)(d-s));
    buf_index = (buf_index + 1) == LCD_BUFFER_NUMS ? 0 : (buf_index + 1);
}

#include "sx2.h"




#define LCD_CMD_BITS         (8)
#define LCD_PARAM_BITS       (8)


esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

    //ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->max_transfer_sz,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 2,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");


    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);
    //esp_lcd_panel_invert_color(*ret_panel, true);
    esp_lcd_panel_invert_color(*ret_panel, false);
    return ret;

err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}
esp_err_t lcd_clear_fast(esp_lcd_panel_handle_t *panel, uint16_t color)
{
    uint16_t fact = BSP_LCD_V_RES/16;
	int bcnt=0;
    // uint64_t s = esp_timer_get_time();
    	LOGI( "%s %d\n",__func__,__LINE__);
	
    uint16_t *buffer =lcd_buffer[0];
    if (NULL == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return ESP_FAIL;
    } else {

        for (int y = 0; y < BSP_LCD_V_RES; y += fact) {
			for (uint16_t i = 0; i < BSP_LCD_H_RES * fact; i++) {
            	buffer[i] = 1<<bcnt;
       		}
			bcnt++;
			LOGI( "%s %d\n",__func__,__LINE__);
			fflush(stdout);
            esp_lcd_panel_draw_bitmap(panel, 0, y, BSP_LCD_H_RES, y + fact, buffer);
        }
       
    }
    // uint64_t t1 = (esp_timer_get_time() - s);
    // float time_per_frame = t1 / 1000;   
    // float fps = 1000000.f / (float)t1;
    // ESP_LOGI(TAG, "@resolution %ux%u  [time per frame=%.2fMS, fps=%.2f]", BSP_LCD_H_RES, LCD_HEIGHT, time_per_frame, fps);
    return ESP_OK;
}



esp_err_t app_lcd_init(void)
{
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * 10 * sizeof(uint16_t),
    };
		//int7 kk;
	LOGI( "%s %d %d/%d",__func__,__LINE__,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
	  
	fflush(stdout);
	#if 1
    bsp_display_new(&bsp_disp_cfg, &display_handle, &ret_io);
    esp_lcd_panel_disp_on_off(display_handle, true);
    //bsp_display_backlight_on();
	#endif
	LOGI( "%s %d %d/%d",__func__,__LINE__,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
	  

	int image_size=EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;
	retry:
    lcd_buffer[buf_index] = (void *)calloc(1, image_size);

	if(!lcd_buffer[buf_index]) {
	  LOGI("alloc rgb img fail %d\n",image_size);
	  image_size-=256;
	  goto retry;
	}

	LOGI( "%s %d lcdbuf:%p %d/%d",__func__,__LINE__,lcd_buffer[buf_index],heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
	  

	app_lcd_draw(sx2_jpg,  sizeof(sx2_jpg), BSP_LCD_H_RES, BSP_LCD_H_RES);
			lcd_clear_fast(display_handle, 0xffff);

    return ESP_OK;
}
