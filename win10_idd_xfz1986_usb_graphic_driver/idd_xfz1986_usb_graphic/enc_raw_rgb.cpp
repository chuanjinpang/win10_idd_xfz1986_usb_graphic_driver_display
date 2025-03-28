
#include "base_type.h"
#include "log.h"
#include "enc_base.h"
#include "enc_raw_rgb.h"

int  rgb888x_encoder_rgb565(uint16_t * pix_msg ,uint32_t * framebuffer ,int x, int y, int right, int bottom, int line_width)
{
int    last_copied_x, last_copied_y;
int pos = 0;

		// locate to the begining...
		framebuffer += (y * line_width + x);

		LOG("%s .\n",__func__);

#if 1
		for (last_copied_y = y; last_copied_y <= bottom; ++last_copied_y) {
	
			for (last_copied_x = x; last_copied_x <= right; ++last_copied_x) {
	

				pixel_type_t pix = *framebuffer;
				uint8_t r, g, b;
				//LOG("fb %p\n",framebuffer);
				r = pix & 0xff;
				g = (pix >> 8) & 0xff;
				b = (pix >> 16) & 0xff;
				uint16_t current_pixel_le = rgb565(b, g, r);
				//current_pixel_le = (current_pixel_le >> 8) | (current_pixel_le << 8);
				*pix_msg = current_pixel_le;
				pix_msg++;	
				++framebuffer;	
			}
			framebuffer += line_width - right - 1 + x;
		}
#endif

		LOG("%s ..\n",__func__);

		return (right - x + 1) * (bottom - y + 1) * 2;
	
}

int enc_rgb565::enc( uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width,int limit)
{
	return rgb888x_encoder_rgb565((uint16_t *)enc,(uint32_t*)src, x, y, right, bottom, line_width);

}

