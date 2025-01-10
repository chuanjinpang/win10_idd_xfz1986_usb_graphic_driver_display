
#include "inc/base_type.h"
#include "inc/enc_base.h"
#include "inc/log.h"


int enc_base_disp_setup_frame_header(uint8_t * msg, int x, int y, int right, int bottom, uint8_t op_flg ,uint32_t total)
{
	udisp_frame_header_t * pfh;


	pfh = (udisp_frame_header_t *)msg;
	pfh->type = op_flg;
	pfh->crc16 = 0;
	pfh->x = cpu_to_le16(x);
	pfh->y = cpu_to_le16(y);
	pfh->width = cpu_to_le16(right + 1 - x);
	pfh->height = cpu_to_le16(bottom + 1 - y);
	pfh->payload_total = total;

	LOGD("%s type:%d（%d %d %d %d) %d\n",__func__,pfh->type,pfh->x,pfh->y, pfh->width,
		pfh->height,pfh->payload_total);
	return sizeof(udisp_frame_header_t);

}

int enc_base_enc_header(enc_base_t * this,uint8_t * enc,int x, int y, int right, int bottom, int total_bytes)
{
        
    enc_base_disp_setup_frame_header(enc, x, y, right, bottom, this->enc_type, total_bytes);

    return 0;

};


int create_enc_base(enc_base_t * this){
this->enc_header=enc_base_enc_header;
return 0;
}