//#include "Driver.h"
#include "base_type.h"
#include <stdarg.h>

uint16_t crc16_calc_multi(uint16_t crc_reg, unsigned char *puchMsg, unsigned int usDataLen)
{
	uint32_t i, j, check;
	for (i = 0; i < usDataLen; i++) {
		crc_reg = (crc_reg >> 8) ^ puchMsg[i];
		for (j = 0; j < 8; j++) {
			check = crc_reg & 0x0001;
			crc_reg >>= 1;
			if (check == 0x0001) {
				crc_reg ^= 0xA001;
			}
		}
	}
	return crc_reg;
}


uint16_t crc16_calc(unsigned char *puchMsg, unsigned int usDataLen)
{
	return crc16_calc_multi(0xFFFF, puchMsg, usDataLen);
}

#if 1


long get_os_us(void)
{

	SYSTEMTIME time;

	GetSystemTime(&time);
	long time_ms = (time.wSecond * 1000) + time.wMilliseconds;

	return time_ms * 1000;



}


long get_system_us(void)
{
	return get_os_us();
}



long get_fps(fps_mgr_t * mgr)
{	  
	 if(mgr->cur < FPS_STAT_MAX)//we ignore first loop and also ignore rollback case due to a long period
		 return mgr->last_fps;//if <0 ,please ignore it
	else {
	 int i=0;
	 long b=0;
		 long a = mgr->tb[(mgr->cur-1)%FPS_STAT_MAX];//cur
	 for(i=2;i<FPS_STAT_MAX;i++){
	 
		 b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
	 if((a-b) > 1000000)
		 break;
	 }
		 b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
		 long fps = (a - b) / (i-1);
		 fps = (1000000*10 ) / fps;
		 mgr->last_fps = fps;
		 return fps;
	 }

}

void put_fps_data(fps_mgr_t* mgr,long t)
{
	static int init = 0;


	if (0 == init) {
		mgr->cur = 0;
		mgr->last_fps = -1;
		init = 1;
	}

	mgr->tb[mgr->cur%FPS_STAT_MAX] = t;
	mgr->cur++;//cur ptr to next

}


/**********************/

void dump_memory_bytes(char *prefix, uint8_t * hash, int bytes)
{
	char out[256] = { 0 };
	int len = 0, i = 0;

	for (i = 0; i < bytes; i++) {
		if (i && (i % 16 == 0)) {
			LOG("[%04d]%s %s\n", i, prefix, out);
			len = 0;
		}
		len += sprintf(out + len, "0x%02x,", hash[i] & 0xff);

	}
	LOG("[%04d]%s %s\n", i, prefix, out);

}

#endif


void _log(const char* fmt, ...) {
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	OutputDebugStringA(buf);
}



