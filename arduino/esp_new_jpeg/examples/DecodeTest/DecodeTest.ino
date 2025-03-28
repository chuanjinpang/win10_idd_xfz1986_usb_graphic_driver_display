#include <stdlib.h>
#include <Arduino.h>
#include <ESP32_JPEG_Library.h>
#include "FS.h"
#include "SPIFFS.h"

#define TEST_NUM                    100
#define TEST_IMAGE_FILE_PATH        "/img_test_320_240.jpg"
#define TEST_IMAGE_RGB565_SIZE      (320 * 240 * 2)

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

size_t getFileSize(fs::FS &fs, const char * path) {
    File file = fs.open(path);
    if(!file){
        Serial.println("- failed to open file for getting size");
        return 0;
    }

    size_t len = file.size();
    file.close();
    return len;
}

void readFile(fs::FS &fs, const char * path, uint8_t *buf, size_t buf_len) {
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    size_t len = file.size();
    len = len > buf_len ? buf_len : len;
    while (len) {
        size_t read_len = len > 512 ? 512 : len;
        read_len = file.read(buf, read_len);
        buf += read_len;
        len -= read_len;
    }

    file.close();

    Serial.println("Read file done");
}

static jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    int inbuf_consumed = 0;

    // Generate default configuration
    jpeg_dec_config_t config = {
        .output_type = JPEG_RAW_TYPE_RGB565_BE,
        .rotate = JPEG_ROTATE_0D,
    };

    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t *jpeg_dec = NULL;

    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);

    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        return JPEG_ERR_MEM;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        return JPEG_ERR_MEM;
    }
    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        Serial.println("JPEG decode parse failed");
        goto _exit;
    }

    jpeg_io->outbuf = output_buf;
    inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = input_buf + inbuf_consumed;
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        Serial.println("JPEG decode process failed");
        goto _exit;
    }

_exit:
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ret;
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */
    Serial.println("Hello Arduino!");

    sleep(3);

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    Serial.println("SPIFFS Mount OK");
    listDir(SPIFFS, "/", 0);

    uint8_t *image_jpeg = NULL;
    uint8_t *image_rgb = NULL;
    size_t image_jpeg_size = 0;
    image_jpeg_size = getFileSize(SPIFFS, TEST_IMAGE_FILE_PATH);
    /* The buffer used by JPEG decoder must be 16-byte aligned */
    image_jpeg = (uint8_t *)aligned_alloc(16, image_jpeg_size);
    image_rgb = (uint8_t *)aligned_alloc(16, TEST_IMAGE_RGB565_SIZE);
    if (image_jpeg == NULL || image_rgb == NULL) {
        Serial.println("Image memory allocation failed");
        return;
    }
    readFile(SPIFFS, TEST_IMAGE_FILE_PATH, image_jpeg, image_jpeg_size);

    jpeg_error_t ret = JPEG_ERR_OK;
    uint32_t t = millis();
    for (int i = 0; i < TEST_NUM; i++) {
        ret = esp_jpeg_decoder_one_image(image_jpeg, image_jpeg_size, image_rgb);
        if (ret != JPEG_ERR_OK) {
            Serial.printf("JPEG decode failed - %d\n", (int)ret);
            break;
        }
    }
    Serial.printf("JPEG decode %d images, average time is %d ms\n", TEST_NUM, (millis() - t) / TEST_NUM);
    free(image_jpeg);
    free(image_rgb);

    if (ret != JPEG_ERR_OK) {
        return;
    } else {
        Serial.println("JPEG decode OK");
    }
}

void loop()
{
    Serial.println("Loop");
    sleep(1);
}
