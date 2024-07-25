#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4


//#define LOG(fmt,...) do {;} while(0)

extern long debug_level;
#define LOGE(fmt,...)  do  {if(debug_level >= LOG_LEVEL_ERROR) _log(fmt,__VA_ARGS__); } while(0)
#define LOGW(fmt,...)  do  {if(debug_level >= LOG_LEVEL_WARN) _log(fmt,__VA_ARGS__); } while(0)
#define LOGI(fmt,...)  do  {if(debug_level >= LOG_LEVEL_INFO) _log(fmt,__VA_ARGS__); } while(0)
#define LOGD(fmt,...)  do  {if(debug_level >= LOG_LEVEL_DEBUG) _log(fmt,__VA_ARGS__); } while(0)
#define LOG(fmt,...)   do  {if(debug_level >= LOG_LEVEL_DEBUG) _log(fmt,__VA_ARGS__); } while(0)


void _log(const char* fmt, ...);

#ifdef __cplusplus
}  // extern C
#endif
