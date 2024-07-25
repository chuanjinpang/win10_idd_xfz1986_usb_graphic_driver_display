
#include"enc_base.h"
#include"tiny_jpeg.h"




class enc_jpg :public enc_base {

public:
	enc_jpg(){
			//enc_type=UDISP_TYPE_JPG;
			enc_type=0;
			jpg_quality=5;
		}
	enc_jpg(int q){
			//enc_type=UDISP_TYPE_JPG;
			enc_type=0;
			jpg_quality=q;
		}
	int enc(uint8_t* enc, uint8_t * src, int x, int y, int right, int bottom, int line_width);
protected:
	int jpg_quality;
};

