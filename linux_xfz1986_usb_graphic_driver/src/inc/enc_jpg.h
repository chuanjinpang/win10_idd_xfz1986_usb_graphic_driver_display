
#include"enc_base.h"
#include"tiny_jpeg.h"


int create_enc_jpg(enc_base_t * this,int quality);

/* Convert XRGB8888 framebuffer (with stride) to tightly-packed RGB888.
 * Returns number of bytes written (width * height * 3). */
int rgb888a_to_rgb888(uint8_t *pix_msg, uint32_t *framebuffer,
		      int x, int y, int right, int bottom, int line_width);
