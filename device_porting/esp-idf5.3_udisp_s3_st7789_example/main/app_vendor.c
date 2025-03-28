/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "app_usb.h"
#include "app_lcd.h"
#include "esp_timer.h"
#include "usb_frame.h"

static const char *TAG = "app_vendor";
static frame_t *current_frame = NULL;

//--------------------------------------------------------------------+
// Vendor callbacks
//--------------------------------------------------------------------+

#define CONFIG_USB_VENDOR_RX_BUFSIZE  VENDOR_BUF_SIZE



#define LOGD(fmt,args...) do {;}while(0)

//#define LOGD(format, ...) ESP_LOGI(TAG,  format, ##__VA_ARGS__)

#define LOGI(format, ...) ESP_LOGI(TAG,  format, ##__VA_ARGS__)



#define UDISP_BUF_SIZE 100*1024 

typedef struct _udisp_frame_header_t {  //20bytes
	uint16_t crc16;//payload crc16
    uint8_t  type; //raw rgb,yuv,jpg,other
    uint8_t  cmd;    
    uint16_t x;  //32bit
    uint16_t y;
    uint16_t width;//32bit
    uint16_t height;
	uint32_t frame_id:10;
    uint32_t payload_total:22; //payload max 4MB
} __attribute__((packed)) udisp_frame_header_t;

typedef struct {
udisp_frame_header_t  hd;
uint8_t  buf[UDISP_BUF_SIZE];
size_t data_len;
size_t max;
int id;
} udisp_frame_t;

#define CONFIG_USB_VENDOR_RX_BUFSIZE 64


#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3



#define JPG_FRAME_MAX 2

typedef struct  {
    uint16_t frame_id;
	QueueHandle_t   jpg_free_list;// free jpg list
	QueueHandle_t   jpg_data_list;// data jpg list
	udisp_frame_t  * cur;
	size_t payload_total;
	size_t rx_len;
} udisp_frame_mgr_t;

udisp_frame_mgr_t g_udisp_mgr;

esp_err_t udisp_frame_create(udisp_frame_mgr_t * mgr)
{
    esp_err_t ret=0;

	LOGI( "%d/%d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    // We will be passing the frame buffers by reference
    mgr->jpg_free_list = xQueueCreate(JPG_FRAME_MAX, sizeof(udisp_frame_t *));
    mgr->jpg_data_list  = xQueueCreate(JPG_FRAME_MAX, sizeof(udisp_frame_t *));
    for (int i = 0; i < JPG_FRAME_MAX; i++) {
          // Add the frame to Queue of empty frames
           udisp_frame_t *this_fb = (udisp_frame_t *)malloc(sizeof(udisp_frame_t));
			this_fb->max=UDISP_BUF_SIZE;
			this_fb->data_len=0;
			this_fb->id=i;
           BaseType_t result = xQueueSend(mgr->jpg_free_list, &this_fb, 0);
      
    }
	LOGI( "%d/%d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL),heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
  
    return ret;
}

void udisp_frame_reset(udisp_frame_t *frame)
{
    assert(frame);
    frame->data_len = 0;
}

esp_err_t udisp_frame_put_free_frame(udisp_frame_mgr_t * mgr,udisp_frame_t *frame)
{
	LOGD("pf%d f%d %d",frame->id, frame->hd.frame_id, frame->data_len);
    udisp_frame_reset(frame);
    BaseType_t result = xQueueSend(mgr->jpg_free_list, &frame, 0);
    return ESP_OK;
}

udisp_frame_t *udisp_frame_get_free_frame(udisp_frame_mgr_t * mgr)
{
    udisp_frame_t *this_fb;
    if (xQueueReceive(mgr->jpg_free_list, &this_fb, 0) == pdPASS) {
		LOGD("gf%d",this_fb->id);
        return this_fb;
    } else {
           LOGI("no free uf");
	    return NULL;
    }
}

esp_err_t udisp_frame_put_data_frame(udisp_frame_mgr_t * mgr,udisp_frame_t *frame)
{
    
	LOGD("pd%d f%d %d\n",frame->id,frame->hd.frame_id, frame->data_len);
    BaseType_t result = xQueueSend(mgr->jpg_data_list, &frame, 0);
//    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory filled_fb_queue");
    return ESP_OK;
}




udisp_frame_t *udisp_frame_get_data_frame(udisp_frame_mgr_t * mgr)
{
    udisp_frame_t *frame;
    if (xQueueReceive(mgr->jpg_data_list, &frame, portMAX_DELAY) == pdPASS) {
		LOGD("gd%d f%d %d\n",frame->id,frame->hd.frame_id,frame->data_len);
        return frame;
    } else {
        return NULL;
    }
}

esp_err_t udisp_frame_push_data(udisp_frame_t *frame, const uint8_t *data, size_t data_len)
{
    //ESP_RETURN_ON_FALSE(frame && data && data_len, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    if (frame->max < frame->data_len + data_len) {
        LOGI( "Frame ovf %d %d max:%d",frame->data_len,data_len,frame->max);
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(frame->buf + frame->data_len, data, data_len);
    frame->data_len += data_len;
	LOGD("push%d %d\n",frame->id,frame->data_len);
    return ESP_OK;
}

#define min(a, b) ({            \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a < _b ? _a : _b;          \
})

int udisp_data_handler(udisp_frame_mgr_t      *mgr,uint8_t *req_buf, size_t len,int start , int end)
{
uint8_t *rx_buf;
int cur=0,read_res=0;
int remain=len;


#if 1
	if(start || end)
		LOGD("%s %d s:%d e:%d\n",__func__,len,start,end);
#endif
	do {
        read_res = min(remain, CONFIG_USB_VENDOR_RX_BUFSIZE);
		rx_buf=&req_buf[cur];
        if(0 == read_res)
		    break;
        if(start) {        
			udisp_frame_header_t * pfh = (udisp_frame_header_t *)rx_buf;
			start=0;
			
			LOGI("fid%d enc:%x crc:%x x:%d y:%d w:%d h:%d total:%d",pfh->frame_id, pfh->type,pfh->crc16,pfh->x,pfh->y,pfh->width,pfh->height,pfh->payload_total);
			mgr->cur = udisp_frame_get_free_frame(mgr);
			switch(pfh->type) {
			case UDISP_TYPE_RGB565:
            case UDISP_TYPE_RGB888:
			case UDISP_TYPE_YUV420:
			case UDISP_TYPE_JPG:
	        {
	        	mgr->rx_len = read_res - sizeof(udisp_frame_header_t);
				mgr->payload_total = pfh->payload_total;
	            if(!mgr->cur){
					LOGD("no free frame\n");
				} else {
	                mgr->cur->hd = *pfh;
	                udisp_frame_push_data(mgr->cur,&rx_buf[sizeof(udisp_frame_header_t)],read_res - sizeof(udisp_frame_header_t));
				}
             }
			break;
            default:
                LOGI("error cmd:%x",pfh->type);
                break;
            }

        } else {
        	mgr->rx_len+=read_res;
        	if(!mgr->cur){
				LOGD("no free frame\n");
			} else {
            	udisp_frame_push_data(mgr->cur,rx_buf, read_res);
			}
        }
		remain-=read_res;
		cur+=read_res;
		LOGD("%s rd:%d cur:%d remain:%d len:%d (%d|%d) end:%d\n",__func__,read_res,cur,remain,len,mgr->rx_len,mgr->payload_total,end);
		//for esp32sx seem no support zero-packet
		if(!end && mgr->rx_len >= mgr->payload_total ){
			LOGI("--no zero end case %d\n",mgr->payload_total);
			end=1;
		}


    }while(read_res);



	//end 	
	if(end) {
		if(!mgr->cur){
			LOGD("no free frame for end\n");
			goto  exit;
		}
		LOGD("%s end %d s:%d e:%d\n",__func__,len,start,end);
		if(mgr->cur->data_len >= mgr->cur->hd.payload_total){
			udisp_frame_put_data_frame(mgr,mgr->cur);
			mgr->cur=NULL;
		}
		else {//invalid case, so drop
			udisp_frame_put_free_frame(mgr,mgr->cur);
			mgr->cur=NULL;
		}
		
	}
	exit:
	return end;

}


#if 1
void tud_vendor_rx_cb(uint8_t itf)
{
#define CONFIG_USB_VENDOR_RX_BUFSIZE 64
	uint8_t rx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];
	static int urx_total=0;
	int  end;
	
	LOGD("usb packet:%d\n",bufsize);

    do {
        int rx = tud_vendor_n_read(itf,
                                         rx_buf,
                                         CONFIG_USB_VENDOR_RX_BUFSIZE);
		
		end=udisp_data_handler(&g_udisp_mgr,rx_buf, rx,urx_total==0,rx < CONFIG_USB_VENDOR_RX_BUFSIZE);//means end of file
		if(end){			
			urx_total=0;
		} else {
			urx_total +=rx;
		}
    }while(tud_vendor_n_available(itf));

}
#endif

int64_t esp_timer_get_time(void);

long get_system_us(void)
{
    return esp_timer_get_time();
}

/*********fps***********/
#define FPS_STAT_MAX 5
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
        return fps/10;
    }
}
void put_fps_data(long t) //us
{
    fps_mgr_t * mgr = &fps_mgr;
    mgr->tb[mgr->cur%FPS_STAT_MAX] = t;
    mgr->cur++;//cur ptr to next
}

/**********fps end***********/

void transfer_task(void *pvParameter)
{
	udisp_frame_create(&g_udisp_mgr);
    while (1) {
		
	long s,e;
	
	udisp_frame_t * uf= udisp_frame_get_data_frame(&g_udisp_mgr);; 
	//ESP_LOGI(TAG,"get%d fid%d %d\n",uf->id,uf->hd.frame_id,uf->data_len); 			 
	
	s=get_system_us();
	 app_lcd_draw(uf->buf,uf->data_len,uf->hd.width,uf->hd.height);

	e=get_system_us();		
	put_fps_data(get_system_us());
	ESP_LOGI(TAG,"[%d]%d %d %d fps:%d\n", e-s,uf->id,uf->hd.frame_id,uf->data_len,get_fps());
	udisp_frame_put_free_frame(&g_udisp_mgr,uf);
    }
}

esp_err_t app_vendor_init(void)
{
    xTaskCreatePinnedToCore(transfer_task, "transfer_task", 4096, NULL, CONFIG_VENDOR_TASK_PRIORITY, NULL, 0);
    return ESP_OK;
}
