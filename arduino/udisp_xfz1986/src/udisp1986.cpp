// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "udisp1986.h"


#if SOC_USB_OTG_SUPPORTED

#if CONFIG_TINYUSB_VENDOR_ENABLED

#include "esp32-hal-tinyusb.h"


static udisp1986 *_Vendor = NULL;
static uint8_t USB_VENDOR_ENDPOINT_SIZE = 64;

//#define LOGD  log_w
#define LOGD  log_d

#define LOGI  log_w
#define LOGE log_e

uint16_t tusb_vendor_load_descriptor(uint8_t *dst, uint8_t *itf) {
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB vudisp1986");
  uint8_t ep_num = tinyusb_get_free_duplex_endpoint();
  TU_VERIFY(ep_num != 0);
  uint8_t descriptor[TUD_VENDOR_DESC_LEN] = {// Interface number, string index, EP Out & IN address, EP size
                                             TUD_VENDOR_DESCRIPTOR(*itf, str_index, ep_num, (uint8_t)(0x80 | ep_num), USB_VENDOR_ENDPOINT_SIZE)
  };
  *itf += 1;
  memcpy(dst, descriptor, TUD_VENDOR_DESC_LEN);
   LOGI("vudisp load");
  return TUD_VENDOR_DESC_LEN;
}




#define CONFIG_USB_VENDOR_RX_BUFSIZE 64


#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3



#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3


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
    esp_err_t ret;

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
			LOGI("%p %d",this_fb,sizeof(udisp_frame_t));
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
    
	LOGI("pd%d f%d %d\n",frame->id,frame->hd.frame_id, frame->data_len);
    BaseType_t result = xQueueSend(mgr->jpg_data_list, &frame, 0);
//    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory filled_fb_queue");
    return ESP_OK;
}




udisp_frame_t *udisp_frame_get_data_frame(udisp_frame_mgr_t * mgr)
{
    udisp_frame_t *frame;
    if (xQueueReceive(mgr->jpg_data_list, &frame, portMAX_DELAY) == pdPASS) {
		LOGI("gd%d f%d %d\n",frame->id,frame->hd.frame_id,frame->data_len);
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
			
			LOGI("fid%d enc:%x w:%d h:%d total:%d",pfh->frame_id, pfh->type,pfh->width,pfh->height,pfh->payload_total);
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
void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
#define CONFIG_USB_VENDOR_RX_BUFSIZE 64
	uint8_t rx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];
	static int urx_total=0;
	int  end;
	
	LOGD("usb packet:%d\n",bufsize);

	if(bufsize == 0){
		LOGI("zero len usb packet\n");

		}
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


udisp_frame_t * udisp1986::udisp_get_data_frame(void) {
  return udisp_frame_get_data_frame(&g_udisp_mgr);
}
int udisp1986::udisp_put_free_frame(udisp_frame_t * fr ) {
  return udisp_frame_put_free_frame(&g_udisp_mgr,fr);
}



udisp1986::udisp1986(uint8_t endpoint_size,arduino_udisp_task_cb_t cb) : itf(0) ,task_cb(cb) {
  if (!_Vendor) {
    _Vendor = this;
	 udisp_frame_create(&g_udisp_mgr);
    if (endpoint_size <= 64) {
      USB_VENDOR_ENDPOINT_SIZE = endpoint_size;
    }
    tinyusb_enable_interface(USB_INTERFACE_VENDOR, TUD_VENDOR_DESC_LEN, tusb_vendor_load_descriptor);
  } else {
    itf = _Vendor->itf;
   
	 
  }
   LOGI("udisp create");
}


void udisp_draw_task(void *pvParameter)
{
	
    while (1) {
		if(_Vendor->task_cb != NULL){

			_Vendor->task_cb();
		} else {
			
	        vTaskDelay(10);
		}
    }
}


void udisp1986::begin() {
  
	xTaskCreatePinnedToCore(udisp_draw_task, "udisp_draw_task", 4096, NULL, 10, NULL, 0);

}

void udisp1986::end() {
  
}

bool udisp1986::mounted() {
  return tud_vendor_n_mounted(itf);
}




#endif /* CONFIG_TINYUSB_VENDOR_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
