

#ifndef __LOG_H__
#define __LOG_H__

#define LOGD(fmt,args...) do {;}while(0)

//#define LOGD(format, ...) ESP_LOGI(TAG,  format, ##__VA_ARGS__)

#define LOGI(format, ...) ESP_LOGI(TAG,  format, ##__VA_ARGS__)


#endif
