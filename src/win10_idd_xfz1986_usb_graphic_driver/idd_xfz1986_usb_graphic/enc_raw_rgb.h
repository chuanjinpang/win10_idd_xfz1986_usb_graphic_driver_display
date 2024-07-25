

#include <windows.h>
class enc_rgb565 :public enc_base {
public:
	enc_rgb565(){
			enc_type=UDISP_TYPE_RGB565;
		}
	int enc(uint8_t* enc, uint8_t * src, int x, int y, int right, int bottom, int line_width);

};


class enc_rgb888a :public enc_base {
public:
	enc_rgb888a(){
			enc_type=UDISP_TYPE_RGB888;
		}

	int enc(uint8_t* enc, uint8_t * src, int x, int y, int right, int bottom, int line_width) {
		int total_bytes = (right - x + 1) * (bottom - y + 1) * 2;
		memcpy(enc, src, total_bytes);
		return 0;
	}

};
