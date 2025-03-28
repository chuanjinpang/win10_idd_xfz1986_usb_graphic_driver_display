#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
#endif /* ARDUINO_USB_MODE */

#include "USB.h"
#include "udisp1986.h"

udisp1986 udisp_vendor;

#include <Arduino_GFX_Library.h>
#include <ESP_NEW_JPEG_Library.h>
Arduino_DataBus *bus = new Arduino_ESP32SPIDMA(38   /* DC */, 37 /* CS */, 33 /* SCK */, 34 /* MOSI */, GFX_NOT_DEFINED /* MISO */, HSPI  /* spi_num */);
Arduino_TFT *gfx = new Arduino_ST7789(bus, 1 /* RST */, 3 /* rotation */, false /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 0 /* row offset 1 */, 0 /* col offset 2 */, 0 /* row offset 2 */);


uint8_t *image_rgb = NULL;
size_t image_size = 0;

static int esp_jpeg_decoder(uint8_t *input_buf, int len, uint8_t *output_buf )
{
    esp_err_t ret = ESP_OK;
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    //config.clipper.width     = 480;
    //config.clipper.height    = 320;
    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;

    // Create jpeg_dec
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)malloc(sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        return ESP_FAIL;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info =(jpeg_dec_header_info_t *) malloc(sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        return ESP_FAIL;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        goto _exit;
    }

_exit:
  // Decoder deinitialize
  jpeg_dec_close(jpeg_dec);
  free(out_info);
  free(jpeg_io);
  return ret;
}

void draw_jpg(uint8_t *image_jpeg, size_t image_jpeg_size)
{
  unsigned long enc;
    int w = gfx->width();
  int h = gfx->height();

  unsigned long start = millis();

  int ret = 0;

  ret = esp_jpeg_decoder(image_jpeg,image_jpeg_size,image_rgb);
  enc=millis();
  if (ret != JPEG_ERR_OK)
  {
    Serial.printf("JPEG decode failed - %d\n", (int)ret);
  }
  else
  {
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)image_rgb, gfx->width(), gfx->height());
  }

  Serial.printf("%d %d Time used: %lu enc:%lu\n",start,enc, millis() - start,enc-start);

}
long get_system_us(void)
{
    return micros();
}


/*********fps***********/
#define FPS_STAT_MAX 4
typedef struct {
  long tb[FPS_STAT_MAX];
  int cur;
  long last_fps;
} fps_mgr_t;
fps_mgr_t fps_mgr = {
  .cur = 0,
  .last_fps = -1,
};
long get_fps(void) {
  fps_mgr_t *mgr = &fps_mgr;
  if (mgr->cur < FPS_STAT_MAX)  //we ignore first loop and also ignore rollback case due to a long period
    return mgr->last_fps;       //if <0 ,please ignore it
  else {
    int i = 0;
    long b = 0;
    long a = mgr->tb[(mgr->cur - 1) % FPS_STAT_MAX];  //cur
    for (i = 2; i < FPS_STAT_MAX; i++) {

      b = mgr->tb[(mgr->cur - i) % FPS_STAT_MAX];  //last
      if ((a - b) > 1000000)
        break;
    }
    b = mgr->tb[(mgr->cur - i) % FPS_STAT_MAX];  //last
    long fps = (a - b) / (i - 1);
    fps = (1000000 * 10) / fps;
    mgr->last_fps = fps;
    return fps / 10;
  }
}
void put_fps_data(long t)  //us
{
  fps_mgr_t *mgr = &fps_mgr;
  mgr->tb[mgr->cur % FPS_STAT_MAX] = t;
  mgr->cur++;  //cur ptr to next
}

/**********fps end***********/


void draw_task(void){
  udisp_frame_t * uf= udisp_vendor.udisp_get_data_frame (); 

  draw_jpg(uf->buf, uf->data_len);
  put_fps_data(get_system_us());
  Serial.printf("%d %d %d fps:%d\n", uf->id, uf->hd.frame_id, uf->data_len, get_fps());
  udisp_vendor.udisp_put_free_frame(uf);

}

void setup(void)
{
  Serial.begin(115200);
 
 Serial.printf("freeHeap: %lu\n", ESP.getFreeHeap());
 image_size = gfx->width() * gfx->height() * 2;
 retry:
  image_rgb = (uint8_t *)aligned_alloc(16, image_size);
  if(!image_rgb) {
    Serial.printf("alloc rgb img fail %d\n",image_size);
    image_size-=256;
    goto retry;
  }
  Serial.printf("freeHeap: %lu\n", ESP.getFreeHeap());
  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  delay(10); // 5 seconds

#if 1
//debug lcd color
  gfx->fillScreen(BLACK);
  gfx->fillRect(0,0,60,60,RGB565_RED);
  Serial.printf("RGB565_RED:%x\n",RGB565_RED);
  gfx->fillRect(60,0,60,60,RGB565_BLUE);
  Serial.printf("RGB565_BLUE:%x\n",RGB565_BLUE);
  gfx->fillRect(120,0,60,60,RGB565_GREEN);
  Serial.printf("RGB565_GREEN:%x\n",RGB565_GREEN);
  gfx->fillRect(0,60,60,60,RGB565_WHITE);
  Serial.printf("RGB565_BLACK:%x\n",RGB565_BLACK);
  gfx->fillRect(60,60,60,60,RGB565_BLACK);
  Serial.printf("RGB565_BLACK:%x\n",RGB565_BLACK);
#endif
  #if 1
  udisp_vendor.set_task_cb(draw_task);
  udisp_vendor.begin();
  USB.usbClass(0xff);
  USB.PID(0x2987);
  USB.productName("esp32s2_R240x240_Fps15_Bl18_Ejpg15");
  USB.begin();
#endif
}

void loop()
{
  delay(100);
}
