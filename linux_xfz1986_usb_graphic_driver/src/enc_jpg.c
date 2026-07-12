
#include "inc/base_type.h"
#include "inc/log.h"
#include "inc/enc_base.h"
#include "inc/enc_jpg.h"
#include"inc/tiny_jpeg.h"
#include <linux/slab.h>


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
	int w = right - x + 1;
	int h = bottom - y + 1;
	int quality = this->jpg_quality;
	int ret;
	int conv_bytes = w * h * 3;  /* tightly-packed RGB888 */

	mgr->data = enc;
	mgr->max = JPEG_MAX_SIZE;

	/* Ensure conversion buffer is large enough */
	if (!this->conv_buf || this->conv_buf_size < conv_bytes) {
		uint8_t *new_buf = krealloc(this->conv_buf, conv_bytes, GFP_KERNEL);
		if (!new_buf) {
			LOGE("jpg: failed to alloc conv_buf %d bytes\n", conv_bytes);
			return 0;
		}
		this->conv_buf = new_buf;
		this->conv_buf_size = conv_bytes;
	}

	/* Convert XRGB8888 framebuffer (with stride) to tightly-packed RGB888 */
	rgb888a_to_rgb888(this->conv_buf, (uint32_t *)src,
			   x, y, right, bottom, line_width);

	/* Auto-lower quality on overflow: retry until fits or hit quality=0 */
	do {
		mgr->dp = 0;
		mgr->overflow = 0;
		ret = tje_encode_to_ctx(mgr, w, h, 3, this->conv_buf, quality);
		if (!ret) {
			LOGE("Could not encode JPEG at quality %d\n", quality);
			return 0;
		}
		if (mgr->overflow && quality > 0) {
			LOGW("jpg: overflow at quality %d (%d bytes), retry quality %d\n",
			     quality, mgr->dp, quality - 1);
			quality--;
		} else {
			break;
		}
	} while (1);

	if (quality != this->jpg_quality) {
		LOGD("jpg: auto-lowered quality %d -> %d, final size %d\n",
		     this->jpg_quality, quality, mgr->dp);
	} else {
		LOGD("jpg: total:%d quality:%d\n", mgr->dp, quality);
	}

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




