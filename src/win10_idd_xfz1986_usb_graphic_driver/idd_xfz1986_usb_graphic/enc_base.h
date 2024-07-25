#pragma once

#include "base_type.h"
#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif


#if 0
PACK(



);
#endif

#define cpu_to_le16(x)  (uint16_t)(x)

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define PIXEL_32BIT

#define FB_DISP_DEFAULT_PIXEL_BITS  32


#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3


typedef struct _udisp_frame_header_t {  //16bytes
	_u16 crc16;//payload crc16
    _u8  type; //raw rgb,yuv,jpg,other
    _u8  cmd;    
    _u16 x;  //32bit
    _u16 y;
    _u16 width;//32bit
    _u16 height;
	_u32 frame_id:10;
    _u32 payload_total:22; //payload max 4MB
}  udisp_frame_header_t;


class enc_base{
	
public:

	int disp_setup_frame_header(uint8_t * msg, int x, int y, int right, int bottom, uint8_t op_flg ,uint32_t total);
	int enc_header(uint8_t * enc,int x, int y, int right, int bottom, int total_bytes){
			
		disp_setup_frame_header(enc, x, y, right, bottom, this->enc_type, total_bytes);
		//disp_setup_frame_header(enc, x, y, right, bottom, 5, total_bytes);
		return 0;
		return 0;
		};
	virtual int enc( uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width)=0;

protected :
		uint8_t enc_type;
	
};



