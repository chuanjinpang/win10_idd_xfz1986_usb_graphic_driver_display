
#include "inc/base_type.h"
#include "inc/log.h"
#include "inc/enc_base.h"
#include "inc/enc_jpg.h"
#include"inc/tiny_jpeg.h"


#define JPEG_MAX_SIZE 256*1024

#define PIXEL_32BIT 1
int rgb888a_to_rgb888(uint8_t * pix_msg ,uint32_t * framebuffer ,int x, int y, int right, int bottom, int line_width)
{
	int last_copied_x, last_copied_y;
	int msg_pos = 0;
		for (last_copied_y = y; last_copied_y <= bottom; ++last_copied_y) {
	
			for (last_copied_x = x; last_copied_x <= right; ++last_copied_x) {
	
#ifdef PIXEL_32BIT
				pixel_type_t pix = *framebuffer;
				uint8_t r, g, b;
				b = pix & 0xff;
				g = (pix >> 8) & 0xff;
				r = (pix >> 16) & 0xff;
	
				*pix_msg++ = r;
				*pix_msg++ = g;
				*pix_msg++ = b;
				msg_pos += 3;
	
#else
				pixel_type_t pix = *framebuffer;
				uint8_t r, g, b;
				b = (pix << 3) & 0xf8;
				g = (pix >> 3) & 0xfc;
				r = (pix >> 8) & 0xf8;
	
				*pix_msg++ = r;
				*pix_msg++ = g;
				*pix_msg++ = b;
				msg_pos += 3;
#endif
	
	
				++framebuffer;
	
			}
			framebuffer += line_width - right - 1 + x;
		}
return msg_pos;
}

int enc_jpg_enc( enc_base_t * this,uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width)
{
	stream_mgr_t m_mgr;
	stream_mgr_t * mgr = &m_mgr;
	#if 0
	int len=rgb888a_to_rgb888(enc,(uint32_t*)src,x,y,right,bottom,line_width);
	memcpy(src,enc,len);
	#endif
	//ok we use jpeg transfer data
	mgr->data = enc;
	mgr->max = JPEG_MAX_SIZE;
	mgr->dp = 0;
	if (!tje_encode_to_ctx(mgr, (right - x + 1), (bottom - y + 1), 4, src, this->jpg_quality)) {
		LOGE("Could not encode JPEG\n");
	}

	LOG("jpg: total:%d %d\n", mgr->dp, this->jpg_quality);

	return mgr->dp;
}

int create_enc_jpg(enc_base_t * this,int quality)
{
create_enc_base(this);
this->enc_type=UDISP_TYPE_JPG;
this->enc=enc_jpg_enc;
this->jpg_quality=quality;

return 0;
}




