// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "sdkconfig.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>


#if CONFIG_TINYUSB_VENDOR_ENABLED

#if 1
//26k ok
//27k ok
//27.5k ok
//27.75k ok
//#define UDISP_BUF_SIZE 27*1024 +512+256// for some data case , 64kB is small
#define UDISP_BUF_SIZE 18*1024 // for some data case , 64kB is small

#else
#define UDISP_BUF_SIZE 95*1024

#endif
#define JPG_FRAME_MAX 2

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

typedef void (*arduino_udisp_task_cb_t)(void);


class udisp1986   {
private:
  uint8_t itf;
  

public:
	arduino_udisp_task_cb_t task_cb;
  udisp1986(uint8_t endpoint_size = 64 , arduino_udisp_task_cb_t cb=NULL);
  void begin(void);
  void end(void);
  bool mounted(void);
  void set_task_cb(arduino_udisp_task_cb_t cb){
	 task_cb=cb;
	}
  udisp_frame_t * udisp_get_data_frame      (void);
  int  udisp_put_free_frame(udisp_frame_t *);


};

#endif /* CONFIG_TINYUSB_VENDOR_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
