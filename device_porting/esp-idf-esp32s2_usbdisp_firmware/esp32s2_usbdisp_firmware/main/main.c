#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"

#include "ili9340.h"
#include "decode_jpeg.h"
#include "log.h"

#define	INTERVAL		400
#define WAIT	vTaskDelay(INTERVAL)
#define CONFIG_USB_VENDOR_RX_BUFSIZE 64
#define DISP_DATA_BUF_SIZE  (1024*45)
static const char *TAG = "udisp";

TFT_t  gdev;

// You have to set these CONFIG value using menuconfig.
#if 0
#define CONFIG_WIDTH  240
#define CONFIG_HEIGHT 320
#define CONFIG_CS_GPIO 5
#define CONFIG_DC_GPIO 26
#define CONFIG_RESET_GPIO 2
#define CONFIG_BL_GPIO 2
#endif

#define CONFIG_BL_GPIO 5


TaskHandle_t render_task = 0;




// DESCRIPTION:
// This example contains minimal code to make ESP32-S2 based device
// recognizable by USB-host devices as a USB Serial Device.

#include <stdint.h>
#include "esp_log.h"
#include <stdlib.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "sdkconfig.h"
#include "class/vendor/vendor_device.h"



typedef uint8_t _u8;
typedef uint16_t _u16;
typedef uint32_t _u32;

#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3


typedef struct _udisp_frame_header_t {  //20bytes
	_u16 crc16;//payload crc16
    _u8  type; //raw rgb,yuv,jpg,other
    _u8  cmd;    
    _u16 x;  //32bit
    _u16 y;
    _u16 width;//32bit
    _u16 height;
	_u32 frame_id:10;
    _u32 payload_total:22; //payload max 4MB
} __attribute__((packed)) udisp_frame_header_t;


typedef struct  {
	udisp_frame_header_t frame_hd;
    _u16 frame_id;
    _u16 x;
    _u16 y;
    _u16 x2;
    _u16 y2;
    _u16 y_idx;
    int rx_cnt;
    int disp_cnt;
    int done;

} disp_frame_mgr_t;


typedef void (* msg_handle_cb)(uint8_t * msg, int len);



disp_frame_mgr_t  gblt;

uint8_t lcd_color_line_buf[CONFIG_WIDTH*60*2+64] = {0xff};
uint8_t lcd_color_line_buf_async_io[320*2+64] = {0};
int lcd_color_line_buf_idx = 0;



int64_t esp_timer_get_time(void);

long get_system_us(void)
{
    return esp_timer_get_time();
}


long gta, gtb;

/*********fps***********/
#define FPS_STAT_MAX 8

typedef struct {
    long tb[FPS_STAT_MAX];
    int cur;
    long last_fps;
} fps_mgr_t;


fps_mgr_t fps_mgr = {
    .cur = 0,
    .last_fps = -1,
};
long get_fps(void)
{
    fps_mgr_t * mgr = &fps_mgr;
    if(mgr->cur < FPS_STAT_MAX)//we ignore first loop and also ignore rollback case due to a long period
        return mgr->last_fps;//if <0 ,please ignore it
   else {
	int i=0;
	long b=0;
        long a = mgr->tb[(mgr->cur-1)%FPS_STAT_MAX];//cur
	for(i=2;i<FPS_STAT_MAX;i++){
	
        b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
	if((a-b) > 1000000)
		break;
	}
        b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
        long fps = (a - b) / (i-1);
        fps = (1000000*10 ) / fps;
        mgr->last_fps = fps;
        return fps;
    }

}
void put_fps_data(long t) //us
{
    fps_mgr_t * mgr = &fps_mgr;
    mgr->tb[mgr->cur%FPS_STAT_MAX] = t;
    mgr->cur++;//cur ptr to next

}

/**********fps end***********/

void render_done(void)
{

    gblt.done = 1;
    if(xTaskNotify(render_task, 0, eNoAction) != pdPASS) {
        ESP_LOGI(TAG, "Failed to notify render done!\n");
    }
    //calc fps
    put_fps_data(get_system_us());
    //ESP_LOGI(TAG, "\nfps:%.2f\n", get_fps());
}


#include "freertos/stream_buffer.h"
#include "tjpgd.h"

static StreamBufferHandle_t disp_rx_ring_buf;


void jpeg_bitblt(pixel_jpeg * pix_buf, JRECT *rect)
{

    lcd_bitblt_dma(&gdev, gblt.x + rect->left, gblt.y + rect->top, gblt.x + rect->right + 1, gblt.y + rect->bottom + 1, (uint8_t *)pix_buf);

}
void dump_memory_bytes(char *prefix, uint8_t * hash,int bytes)
{
    char out[256] = { 0 };
    int len = 0, i = 0;
        for(i = 0; i < bytes; i++)
        {
            if(i && (i%16==0)) {
            ESP_LOGI(TAG,"[%04d]%s %s\n",i,  prefix, out);
            len=0;
            }
            len += sprintf(out + len, "0x%02x,", hash[i] & 0xff);
        }
          ESP_LOGI(TAG,"[%04d]%s %s\n",i,  prefix, out);
}
uint16_t g_crc16=0;
uint16_t crc16_calc_multi(uint16_t crc_reg, unsigned char *puchMsg, unsigned int usDataLen ) 
{ 
uint16_t i,j,check; 
	for(i=0;i<usDataLen;i++) 
	{ 
	crc_reg = (crc_reg>>8) ^ puchMsg[i]; 
		for(j=0;j<8;j++) 
		{ 
			check = crc_reg & 0x0001; 
			crc_reg >>= 1; 
			if(check==0x0001){ 
				crc_reg ^= 0xA001; 
			} 
		} 
	} 
return crc_reg; 
}
uint16_t crc16_calc(unsigned char *puchMsg, unsigned int usDataLen ) 
{ 
return crc16_calc_multi(0xFFFF,puchMsg,usDataLen);
}
void bitblt_msg_handle(uint8_t * msg, int len)
{
    uint16_t line_size = (gblt.x2 - gblt.x) * 2;

    memcpy(&lcd_color_line_buf[lcd_color_line_buf_idx], msg, len);
    lcd_color_line_buf_idx += len;

   while(lcd_color_line_buf_idx >= line_size) { //got a line data

		memcpy(lcd_color_line_buf_async_io,lcd_color_line_buf,line_size);//cpy for async dma
        lcd_bitblt_dma(&gdev, gblt.x, gblt.y_idx, gblt.x2, gblt.y_idx + 1, (uint8_t *)lcd_color_line_buf_async_io);
        gblt.y_idx++;
        lcd_color_line_buf_idx -= line_size;
        gblt.disp_cnt += line_size;
        memcpy(lcd_color_line_buf, &lcd_color_line_buf[line_size], lcd_color_line_buf_idx);
        if(gblt.y_idx >= gblt.y2) {
                //ESP_LOGI(TAG, "line:%d :%d buf:%d  %d %d %d ",gblt.y_idx,line_size,lcd_color_line_buf_idx,gblt.disp_cnt,gblt.cnt,gblt.total);
                //ESP_LOGI(TAG, "draw tcost %lu %lu #:%lu %d crc:%x %x",gta,gtb,gtb-gta,gblt.done,gblt.fid,  g_crc16);
				//fflush(stdout);
            render_done();

        }

    }

}


size_t jpeg_stream_read(uint8_t * msg, int len, int no_less_flg)
{
    size_t xReceivedBytes = 0;
    int try_cnt = no_less_flg;



    if(msg) {
        if(1 == len) {
            xReceivedBytes = xStreamBufferReceive(disp_rx_ring_buf, msg, len,  portMAX_DELAY); // wait forever
        } else {
            do {
                xReceivedBytes += xStreamBufferReceive(disp_rx_ring_buf, &msg[xReceivedBytes], len - xReceivedBytes,  pdMS_TO_TICKS(10));
                if(try_cnt > 0)
                    try_cnt--;
            } while(try_cnt && (xReceivedBytes < len));
            //the len is just max size for buffer, it may return less than len bytes
        }
        if(xReceivedBytes) {
            gblt.disp_cnt += xReceivedBytes;

        }
        return xReceivedBytes;
    } else {
        uint8_t tmp[8];
        int i = 0;

        while(i < len) {
            xReceivedBytes = xStreamBufferReceive(disp_rx_ring_buf, tmp, 1,  pdMS_TO_TICKS(10));
            i += xReceivedBytes;
        }
        gblt.disp_cnt += len;
        return len;

    }

}

#if 1
static int g_start=1;

int pop_msg_data(uint8_t * rx_buf, int len)
{
    size_t xTxBytes = 0;


    if(0 == len) {
        return 0;
    }

    xTxBytes = xStreamBufferSend(disp_rx_ring_buf, rx_buf, len, pdMS_TO_TICKS(10)); //portMAX_DELAY

    if(xTxBytes != len) {
        ESP_LOGE(TAG, "!!!send data NG:%d but:%d", len, xTxBytes);
    } else {
        if(xStreamBufferBytesAvailable(disp_rx_ring_buf) > (DISP_DATA_BUF_SIZE - 4096)) {
            ESP_LOGI(TAG, "rbf avi:%d", xStreamBufferBytesAvailable(disp_rx_ring_buf));
            if(xStreamBufferBytesAvailable(disp_rx_ring_buf) > (DISP_DATA_BUF_SIZE - 2048)) {
                vTaskDelay(1);
                ESP_LOGI(TAG, "rbf avi:%d with delay", xStreamBufferBytesAvailable(disp_rx_ring_buf));
            }

        }

    }

    if(gblt.rx_cnt >= gblt.frame_hd.payload_total) { //ok we get all data ,so wait the lcd render done.
        int max_wait_cnt = 20;

        while(max_wait_cnt-- && (0 == gblt.done)) {
            if(xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(200)) != pdPASS) {
                ESP_LOGI(TAG, "Failed to wait for done %d %d\n",max_wait_cnt,gblt.done);
            }
        }

        //printf("fps:%.2f\n",get_fps());
        ESP_LOGI(TAG, "%d fps:%lu\n", gblt.frame_hd.payload_total, get_fps());
		g_start=1;


    }
    return 0;
}
#endif

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
uint16_t pix=(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
	return (pix >> 8) | (pix << 8);
}
/* Invoked when CDC interface received data from host */
void fill_color(uint16_t *buf,int len,uint16_t color)
{
int i=0;
    for(i=0;i<=len;i++) {			
			buf[i]= color;
		}
}
void fill_color_bar(uint16_t *buf,int len){
#if 0
    fill_color(buf,len/3,TEST_COLOR_RED);
#else
	fill_color(buf,len/3,rgb565(0xff, 0,0));
    fill_color(&buf[len/3],len/3,rgb565(0,0xff, 0));
    fill_color(&buf[(len*2)/3],len/3,rgb565(0,0,0xff));
#endif
}

#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3

void tud_vendor_rx_cb(uint8_t itf)
{
    static   uint8_t rx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];

    while(tud_vendor_n_available(itf)) {
        int read_res = tud_vendor_n_read(itf,
                                         rx_buf,
                                         CONFIG_USB_VENDOR_RX_BUFSIZE);
	//ESP_LOGI(TAG,"urx:%d %d",read_res, gblt.rx_cnt);
	#if 1
        if(read_res > 0) {
            if(g_start) {
				udisp_frame_header_t * pblt = (udisp_frame_header_t *)rx_buf;
                if(0 == render_task)
                    render_task = xTaskGetCurrentTaskHandle();
				g_start=0;
				switch(pblt->type){
				case UDISP_TYPE_RGB565:
                case UDISP_TYPE_RGB888:
				case UDISP_TYPE_YUV420:
				case UDISP_TYPE_JPG:
                 {
  
                    gta = get_system_us();
					
                    gblt.frame_hd = *pblt;
                    gblt.x = pblt->x;
                    gblt.y = pblt->y;
                    gblt.x2 = pblt->x + pblt->width;
                    gblt.y2 = pblt->y + pblt->height;
                    gblt.y_idx = pblt->y;
                    gblt.rx_cnt = read_res - sizeof(udisp_frame_header_t); //sub header size
                   
                    gblt.done = 0;
					xStreamBufferReset(disp_rx_ring_buf);
					lcd_color_line_buf_idx=0;//maybe this cause idx bug? when data is transfer done
					g_crc16=0xffff;
                    //ESP_LOGI(TAG, "rx bblt x:%d y:%d w:%d h:%d %d",pblt->x,pblt->y,pblt->width,pblt->height,pblt->payload_total);
                    pop_msg_data(&rx_buf[sizeof(udisp_frame_header_t)], read_res - sizeof(udisp_frame_header_t));
                    break;
                }
                default:
                    ESP_LOGI(TAG, "error cmd");
					g_start=1;
                    break;
                }

            } else {

                gblt.rx_cnt += read_res;
                pop_msg_data(rx_buf , read_res );

            }
			if(read_res < CONFIG_USB_VENDOR_RX_BUFSIZE){
				g_start=1;//end packet.
				}

        }
	#endif

    }
}


void usb_main(void)
{
    ESP_LOGI(TAG, "USB initialization");

    // Setting of descriptor. You can use descriptor_tinyusb and
    // descriptor_str_tinyusb as a reference
    tusb_desc_device_t my_descriptor = {
        .bLength = sizeof(my_descriptor),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200, // USB version. 0x0200 means version 2.0
        .bDeviceClass = TUSB_CLASS_UNSPECIFIED,
        .bMaxPacketSize0 = CFG_TUD_ENDOINT0_SIZE,

        .idVendor = 0x303A,
        .idProduct = 0x2986,
        .bcdDevice = 0x0101, // Device FW version

        .iManufacturer = 0x01, // see string_descriptor[1] bellow
        .iProduct = 0x02,      // see string_descriptor[2] bellow
        .iSerialNumber = 0x03, // see string_descriptor[3] bellow

        .bNumConfigurations = 0x01
    };

    tusb_desc_strarray_device_t my_string_descriptor = {
        // array of pointer to string descriptors
        (char[]) {
            0x09, 0x04
        }, // 0: is supported language is English (0x0409)
        "xfz1986",                  // 1: Manufacturer
        "esp32udisp0_R240x240_Ejpg4",   // 2: Product
        "012-2021",            // 3: Serials, should use chip ID
        "NewCreate",            // 4: vendor
    };

    tinyusb_config_t tusb_cfg = {
        .descriptor = &my_descriptor,
        .string_descriptor = my_string_descriptor,
        .external_phy = false // In the most cases you need to use a `false` value
    };


    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");
}



void  inject_jpg_file(uint8_t * buf, int len)
{
    ESP_LOGI(TAG, "logo inject in");
    fflush(stdout);
    int	xTxBytes = xStreamBufferSend(disp_rx_ring_buf, buf, len, pdMS_TO_TICKS(10));
    if(xTxBytes != len) {
        ESP_LOGI(TAG, "logo inject NG");
    }
    ESP_LOGI(TAG, "logo inject done");
    fflush(stdout);
}
void display_logo(void);

void test_color_bar(void){

#if 1  
			{
	
			int x=0,y=0;
				fill_color_bar(lcd_color_line_buf,CONFIG_WIDTH*60);    
				lcd_bitblt(&gdev,x,y,x+CONFIG_WIDTH,y+60,lcd_color_line_buf);
			
	 ESP_LOGI(__FUNCTION__,"%d %d\n",x,y);	  
	
			 x=60,y=120;
				fill_color_bar(lcd_color_line_buf,CONFIG_WIDTH*60);    
				lcd_bitblt(&gdev,x,y,x+CONFIG_WIDTH,y+60,lcd_color_line_buf);
	
	 ESP_LOGI(__FUNCTION__,"%d %d\n",x,y);	  
			}
#endif

}

void ILI9341(void *pvParameters)
{
    uint8_t mrx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];

    spi_master_init(&gdev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);


#if CONFIG_ILI9340
    uint16_t model = 0x9340;
#endif
#if CONFIG_ILI9341
    uint16_t model = 0x9341;
#endif

#if CONFIG_ST7789
		uint16_t model = 0x7789;
#endif
    lcdInit(&gdev, model, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

#if CONFIG_INVERSION
    ESP_LOGI(TAG, "Enable Display Inversion");
    lcdInversionOn(&gdev);
#endif

//#if CONFIG_RGB_COLOR
#if 1
    ESP_LOGI(TAG, "Change LCD row colmn rgb order");
    lcdBGRFilter(&gdev);
#endif
    display_logo();
	//test_color_bar

    while(1) {

#if 1
        if(xStreamBufferBytesAvailable(disp_rx_ring_buf) >= 1) {
#if 0
            if(gblt.cmd == RPUSBDISP_DISPCMD_BITBLT) {
				{
                size_t xReceivedBytes = xStreamBufferReceive(disp_rx_ring_buf, mrx_buf, CONFIG_USB_VENDOR_RX_BUFSIZE,  pdMS_TO_TICKS(200));
                if(xReceivedBytes)
                    bitblt_msg_handle(mrx_buf, xReceivedBytes);
				}

            } else
#endif
            {

                uint16_t imageWidth = 1;
                uint16_t imageHeight = 1;

                esp_err_t err = decode_jpeg(&imageWidth, &imageHeight);

                if(!err) {
                    render_done();
                } else {
                    render_done();
                    ESP_LOGI(TAG, "draw jpeg done err:%d", err);
                }

            }

        }

#endif
    }

}

void app_main(void)
{
    ESP_LOGI(TAG, "xfz1986 udisp go 240710");
    disp_rx_ring_buf = xStreamBufferCreate(DISP_DATA_BUF_SIZE, 1);

    usb_main();
    xTaskCreate(ILI9341, "ILI9341", 1024 * 8, NULL, 2, NULL);

}
