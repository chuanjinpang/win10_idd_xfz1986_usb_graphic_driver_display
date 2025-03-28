#include <stdio.h>
#include "decode_jpeg.h"
#include "tjpgd.h"
#include "esp_log.h"
#include <string.h>
#include "log.h"

#define CONFIG_WIDTH 480
#define CONFIG_HEIGHT 480

//Data that is passed from the decoder function to the infunc/outfunc functions.
typedef struct {
    pixel_jpeg dma_pix_data1[2*CONFIG_WIDTH*16];
    pixel_jpeg dma_pix_data2[2*CONFIG_WIDTH*16];//we need 2 buffer for one DMA transf ,2 for jpeg decode parallel
    pixel_jpeg * cur_data;		// pointers to arrays of 16-bit pixel values
    int screenWidth;		// Width of the screen
    int screenHeight;		// Height of the screen
    JRECT rect;
    int img_width;
    int img_height;
} JpegDev;





int jpeg_stream_read(uint8_t * msg, int len, int flg);



//Input function for jpeg decoder. Just returns bytes from the inData field of the JpegDev structure.
static UINT _infunc(JDEC *decoder, BYTE *buf, UINT len, int flg)
{

    return jpeg_stream_read(buf, len, flg);

}

static UINT infunc_can_less(JDEC *decoder, BYTE *buf, UINT len)
{

    return _infunc(decoder, buf, len, 0);
}

static UINT infunc_no_less(JDEC *decoder, BYTE *buf, UINT len)
{

    return _infunc(decoder, buf, len, 32); //max retry 32 times
}


#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
void jpeg_bitblt(pixel_jpeg * pix_buf, JRECT *rect);

#define TAG "jd"
//Output function. Re-encodes the RGB888 data from the decoder as big-endian RGB565 and
//stores it in the outData array of the JpegDev structure.
static int outfunc(JDEC *decoder, void *bitmap, JRECT *rect)
{
    JpegDev *jd = (JpegDev *) decoder->device;
    uint8_t *in = (uint8_t *) bitmap;
    int i = 0, j = 0;
    int line_size;


#define MAX(x,y) (x)>(y)?(x):(y)
#define MIN(x,y) (x)<(y)?(x):(y)

    jd->rect.left = MIN(jd->rect.left, rect->left);
    jd->rect.right = MAX(jd->rect.right, rect->right);
    jd->rect.top = MIN(jd->rect.top, rect->top);
    jd->rect.bottom = MAX(jd->rect.bottom, rect->bottom);

	//LOGI("out l%d t%d %d %d\n",rect->left,  rect->top, rect->right + 1, rect->bottom + 1);


    line_size = (jd->img_width - jd->rect.left);
    i = rect->left - jd->rect.left;


    for(int y = rect->top; y <= rect->bottom; y++) {
        for(int x = rect->left; x <= rect->right; x++) {

            if(y < jd->screenHeight && x < jd->screenWidth) {


                uint16_t pix = rgb565(in[0], in[1], in[2]);
                //jd->cur_data[i+j] = (pix >> 8) | (pix << 8);
                jd->cur_data[i+j] = pix;
                j++;
            }

            in += 3;
        }

        j = 0;
        i += line_size;

    }
#if 0
    ESP_LOGI(__FUNCTION__, "rect->top=%d rect->bottom=%d", rect->top, rect->bottom);
    ESP_LOGI(__FUNCTION__, "rect->left=%d rect->right=%d", rect->left, rect->right);
#endif
    //bitblt
    if((jd->rect.right + 1) >= jd->img_width) { //got a line witdh
        jpeg_bitblt(jd->cur_data, &jd->rect);

        //swap cur_data
        if(jd->cur_data == jd->dma_pix_data1) {
            jd->cur_data = jd->dma_pix_data2;
            jd->rect.left = CONFIG_WIDTH;
            jd->rect.right = 0;
            jd->rect.top = CONFIG_HEIGHT;
            jd->rect.bottom = 0;

        } else {
            jd->cur_data = jd->dma_pix_data1;
            jd->rect.left = CONFIG_WIDTH;
            jd->rect.right = 0;
            jd->rect.top = CONFIG_HEIGHT;
            jd->rect.bottom = 0;
        }
    }



    return 1;
}

// Specifies scaling factor N for output. The output image is descaled to 1 / 2 ^ N (N = 0 to 3).
// When scaling feature is disabled (JD_USE_SCALE == 0), it must be 0.
uint8_t getScale(uint16_t screenWidth, uint16_t screenHeight, uint16_t imageWidth, uint16_t imageHeight)
{
    if(screenWidth >= imageWidth && screenHeight >= imageHeight)  return 0;

    double scaleWidth = (double)imageWidth / (double)screenWidth;
    double scaleHeight = (double)imageHeight / (double)screenHeight;
    ESP_LOGD(__FUNCTION__, "scaleWidth=%f scaleHeight=%f", scaleWidth, scaleHeight);
    double scale = scaleWidth;
    if(scaleWidth < scaleHeight) scale = scaleHeight;
    ESP_LOGD(__FUNCTION__, "scale=%f", scale);
    if(scale <= 2.0) return 1;
    if(scale <= 4.0) return 2;
    return 3;

}

//Size of the work space for the jpeg decoder.
#define WORKSZ 1024*12

char jpeg_work_buf[WORKSZ];//static buffer avoid failed.
//Decode the embedded image into pixel lines that can be used with the rest of the logic.
esp_err_t decode_jpeg(uint16_t * imageWidth, uint16_t * imageHeight)
{
    char *work = NULL;
    int r;
    JDEC decoder;
    static JpegDev jd;

    uint16_t width;
    uint16_t height;
    esp_err_t ret = ESP_OK;

    //Allocate the work space for the jpeg decoder.
    work = jpeg_work_buf;
    if(work == NULL) {
        ESP_LOGE(__FUNCTION__, "Cannot allocate workspace");
        ret = ESP_ERR_NO_MEM;
        goto out;
    }

    //ofs=0;
    //Prepare and decode the jpeg.

    r = jd_prepare(&decoder, infunc_no_less, work, WORKSZ, (void *) &jd);
    if(r != JDR_OK) {

        ESP_LOGE(__FUNCTION__, "Image decoder: jd_prepare failed (%d)", r);
        //fflush(stdout);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto out;
    }
    //ESP_LOGI(__FUNCTION__, "decoder.width=%d decoder.height=%d", decoder.width, decoder.height);
    //fflush(stdout);
    //Calculate Scaling factor
    width = decoder.width;
    height = decoder.height;
    uint8_t scale = getScale(width, height, decoder.width, decoder.height);
    ESP_LOGD(__FUNCTION__, "scale=%d", scale);

    //Calculate image size
    double factor = 1.0;
    if(scale == 1) factor = 0.5;
    if(scale == 2) factor = 0.25;
    if(scale == 3) factor = 0.125;
    ESP_LOGD(__FUNCTION__, "factor=%f", factor);
    //fflush(stdout);
    if(imageWidth)
    *imageWidth = (double)decoder.width * factor;
	if(imageHeight)
    *imageHeight = (double)decoder.height * factor;
    ESP_LOGD(__FUNCTION__, "imageWidth=%d imageHeight=%d", *imageWidth, *imageHeight);
    // fflush(stdout);
    jd.screenWidth = width;
    jd.screenHeight = height;
    jd.cur_data = jd.dma_pix_data1;
    jd.rect.left = CONFIG_WIDTH;
    jd.rect.right = 0;
    jd.rect.top = CONFIG_HEIGHT;
    jd.rect.bottom = 0;
    jd.img_height = height;
    jd.img_width = width;

    decoder.infunc = infunc_can_less;
    r = jd_decomp(&decoder, outfunc, scale);
    if(r != JDR_OK) {
        ESP_LOGE(__FUNCTION__, "Image decoder: jd_decode failed (%d)", r);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto err;
    }

    //All done! Free the work area (as we don't need it anymore) and return victoriously.

    return ret;

    //Something went wrong! Exit cleanly, de-allocating everything we allocated.
err:
out:

    return ret;
}



