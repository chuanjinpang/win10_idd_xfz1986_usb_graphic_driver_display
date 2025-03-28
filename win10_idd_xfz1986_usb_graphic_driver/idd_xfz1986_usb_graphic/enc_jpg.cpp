#include "enc_jpg.h"
#include "log.h"
#include <windows.h>

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



#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>

#pragma warning(disable:4996)  //

void
enc_turbo(uint8_t** out, unsigned long * size, int quality, JSAMPLE* image_buffer, int image_width, int image_height, int row_stride)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
                   /* physical row width in image buffer */

	//LOGD("%s %d %d %d %d\n",__func__,__LINE__,image_width,image_height,row_stride);

    /* Step 1: allocate and initialize JPEG compression object */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    /* Step 2: specify data destination (eg, a file) */
    jpeg_mem_dest(&cinfo, out, size);

    /* Step 3: set parameters for compression */
    cinfo.image_width = image_width;      /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = 4;           /* # of color components per pixel */
    //cinfo.in_color_space = JCS_EXT_RGBX;       /* colorspace of input image */
	cinfo.in_color_space = JCS_EXT_BGRX;	   /* colorspace of input image */

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

	//LOGD("%s %d\n",__func__,__LINE__);

    /* Step 4: Start compressor */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */


    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
		//LOGD("%s %d %d %d\n",__func__,__LINE__,cinfo.next_scanline,row_stride);
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
	//LOGD("%s %d\n",__func__,__LINE__);

    /* Step 6: Finish compression */
    jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	
}
#if 0
int enc_jpg::enc( uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width,int limit)
{
	stream_mgr_t m_mgr;
	stream_mgr_t * mgr = &m_mgr;
	int jpg_quality=this->jpg_quality;
#if 0
	int len=rgb888a_to_rgb888(enc,(uint32_t*)src,x,y,right,bottom,line_width);
	memcpy(src,enc,len);
#endif
	//ok we use jpeg transfer data
	retry:
	mgr->data = enc;
	mgr->max = JPEG_MAX_SIZE;
	mgr->dp = 0;
	if (!tje_encode_to_ctx(mgr, (right - x + 1), (bottom - y + 1), 4, src, jpg_quality)) {
		LOGE("Could not encode JPEG\n");
	}

	LOG("jpg: total:%d q:%d\n", mgr->dp, jpg_quality);
	if(mgr->dp > limit && jpg_quality>1){
			jpg_quality--;
			goto retry;
	}
	return mgr->dp;
}

#else

#include <stdlib.h>


int enc_jpg::enc( uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width,int limit)
{
	int quality=this->jpg_quality;
	if(quality<10){
		quality*=10;
	}
	//ok we use jpeg transfer data
	retry:
	uint8_t *out=NULL;
	unsigned long out_size=1;
	LOGD("start turbo\n");
	enc_turbo(&out,&out_size, quality,src,right+1,bottom +1,(right+1)*4);
		

	LOG("jpg: total:%d/%d q:%d\n", out_size, limit,quality);
	if(out_size > limit && quality >5){
		quality-=5;
			if(out)
				free(out);
			goto retry;
	}
	#if 1
	memcpy(enc,out,out_size);
	free(out);
	#endif
	
	return out_size;
}


#endif


