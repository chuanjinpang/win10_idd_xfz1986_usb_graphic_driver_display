#ifndef ENC_BASE_H
#define ENC_BASE_H

#define cpu_to_le16(x)  (uint16_t)(x)

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))


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

typedef struct  enc_base enc_base_t;


typedef struct  enc_base{
int (* disp_setup_frame_header_cb)(uint8_t * msg, int x, int y, int right, int bottom, uint8_t op_flg ,uint32_t total);
int (*enc)(enc_base_t * this, uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width);
int (*enc_header)(enc_base_t * this,uint8_t * enc,int x, int y, int right, int bottom, int total_bytes);

uint8_t enc_type;
//sub class
int	jpg_quality;

} enc_base_t;

int create_enc_base(enc_base_t * this);


#endif


