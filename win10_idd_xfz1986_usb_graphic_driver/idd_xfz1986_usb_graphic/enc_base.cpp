#include "enc_base.h"
#include "log.h"
int enc_base::disp_setup_frame_header(uint8_t * msg, int x, int y, int right, int bottom, uint8_t op_flg ,uint32_t total)
{
	udisp_frame_header_t * pfh;
	static int fid=0;

	pfh = (udisp_frame_header_t *)msg;
	pfh->type = op_flg;
	pfh->crc16 = 0;
	pfh->x = cpu_to_le16(x);
	pfh->y = cpu_to_le16(y);
	pfh->y = cpu_to_le16(y);
	pfh->width = cpu_to_le16(right + 1 - x);
	pfh->height = cpu_to_le16(bottom + 1 - y);
	pfh->payload_total = total;
	pfh->frame_id=fid++;

	LOGD("fid:%d enc:%d %d\n",pfh->frame_id, pfh->type,pfh->payload_total);

	return sizeof(udisp_frame_header_t);

}









#if 0
int SwapChainProcessor::usb_send_image(urb_itm_t * urb,WDFUSBPIPE pipeHandle, uint8_t * msg, uint8_t * urb_msg, uint32_t * framebuffer, int x, int y, int right, int bottom, int line_width)
{
	
	uint16_t crc;
	uint32_t total_bytes = 0;
	
#if 0
#define FB_MAX_WIDTH 159
#define FB_MAX_HEIGHT 119

	if ((right - x) > FB_MAX_WIDTH) {
		LOG("width over %d\n", FB_MAX_WIDTH);
		right = FB_MAX_WIDTH + x;
		//return -1;
	}
	if ((bottom - y) > FB_MAX_HEIGHT) {
		LOG("bottom over %d\n", FB_MAX_HEIGHT);
		bottom = FB_MAX_HEIGHT + y;
		//return -1;
	}
#endif
		

	

	//we only calc playload crc16, or it easy cause confuse
	//crc = crc16_calc(&msg[sizeof(udisp_frame_header_t)], total_bytes - sizeof(udisp_frame_header_t));
	//disp_setup_frame_header_total_bytes(msg, total_bytes, crc);
	crc = 0;

	LOG("12345---img:%d %d %d %d crc:%x\n", x, y, right, bottom, crc);
	#if  1
		total_bytes=(right - x + 1) * (bottom - y + 1) * 2;
		rgb888x_encoder_rgb565(framebuffer, (uint16_t*)(&urb_msg[sizeof(udisp_frame_header_t)]), x, y, right, bottom, line_width);
		//memcpy(&urb_msg[sizeof(udisp_frame_header_t)],framebuffer,total_bytes);
		disp_setup_frame_header(urb_msg, x, y, right, bottom, 0, total_bytes);
	#else
		//total_bytes=(right - x + 1) * (bottom - y + 1) * 2;
		total_bytes=480 * 120 * 2;
		
		rgb888x_encoder_rgb565(framebuffer, (uint16_t*)(&urb_msg[sizeof(udisp_frame_header_t)]), x, y, right, 120, line_width);
		disp_setup_frame_header(urb_msg, x, y, right, bottom, 1, total_bytes);
	#endif
		
	//return usb_send_msg(pipeHandle, NULL, urb_msg, total_bytes+ sizeof(udisp_frame_header_t));
		{
		int urb_len=total_bytes+ sizeof(udisp_frame_header_t);
		NTSTATUS ret = usb_send_msg_async(urb, pipeHandle, urb->Request, urb_msg, urb_len);
			  put_fps_data(get_system_us());
			  LOGI("%p issue usb async: total:%d fps:%d(%d)\n", pipeHandle, urb_len,get_fps()/10,get_fps());
			  return ret;
		}

}
#endif

#if 0

int SwapChainProcessor::usb_send_jpeg_image(urb_itm_t * urb, WDFUSBPIPE pipeHandle, uint8_t * msg, uint8_t * urb_msg, uint32_t * framebuffer, int x, int y, int right, int bottom, int line_width)
{
	
	int ret = 0;
	int pos = 0;
	uint8_t * pix_msg;
	stream_mgr_t m_mgr;
	stream_mgr_t * mgr = &m_mgr;
	uint32_t total_bytes = 0; //ovf bug 
	int msg_pos = 0;


	// estimate how many tickets are needed
	size_t image_size = (right - x + 1) * (bottom - y + 1) * (FB_DISP_DEFAULT_PIXEL_BITS / 8);

	// do not transmit zero size image
	if (!image_size) return -1;


	msg_pos = 0;

	pix_msg = &msg[msg_pos];
	// locate to the begining...
	framebuffer += (y * line_width + x);

	long fps = get_fps();
	int low_quality = -1;;
	int high_quality = -1;

redo: {
	//ok we use jpeg transfer data

	mgr->data = &urb_msg[sizeof(udisp_frame_header_t)];
	mgr->max = JPEG_MAX_SIZE;
	mgr->dp = 0;
	if (!tje_encode_to_ctx(mgr, (right - x + 1), (bottom - y + 1), 3, &msg[0], jpg_quality)) {
		LOG("Could not encode JPEG\n");
	}
}
	  LOG("%p jpg: total:%d %d\n", pipeHandle, mgr->dp, jpg_quality);
	  goto next;
	  if (fps >= 60) { //we think this may video case
#define ABS(x) ((x) < 0 ? -(x) : (x))


		  if (ABS(mgr->dp - target_quaility_size) < 2 * 1024)
			  goto next;

		  if (high_quality > 0 && low_quality > 0)
			  goto next;

		  if (mgr->dp > target_quaility_size) {
			  high_quality = jpg_quality;
			  if (jpg_quality >= 2) {
				  jpg_quality--;
				  goto next;
			  }

		  }
		  else {
			  low_quality = jpg_quality;
			  if (jpg_quality <= 5) {
				  jpg_quality++;
				  goto next;
			  }
		  }
	  }
	  else {
		  jpg_quality = 5;
	  }
  next:
	  
	  total_bytes = sizeof(udisp_frame_header_t) + mgr->dp;

	  //encode to urb protocol
	  //int urb_len = encode_urb_msg(pipeHandle, msg, urb_msg, total_bytes,urb->max_ep_out_size);
	  disp_setup_frame_header(urb_msg, x, y, right, bottom, 0, mgr->dp);
	   {
		  gfid++;
		  NTSTATUS ret = usb_send_msg_async(urb, pipeHandle, urb->Request, urb_msg, total_bytes);
		  put_fps_data(get_system_us());
		  LOGI("%p jpg: total:%d fps:%d(x10) %d\n", pipeHandle, total_bytes, fps, jpg_quality);
		  return ret;
	  }


}
#endif


