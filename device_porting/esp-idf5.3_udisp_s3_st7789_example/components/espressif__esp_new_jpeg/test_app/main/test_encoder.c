// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "image_io.h"
#include "test_encoder.h"

jpeg_error_t esp_jpeg_encode_one_picture(void)
{
    // configure encoder
    jpeg_enc_config_t jpeg_enc_cfg = DEFAULT_JPEG_ENC_CONFIG();
    jpeg_enc_cfg.width = TEST_JPEG_X;
    jpeg_enc_cfg.height = TEST_JPEG_Y;
    jpeg_enc_cfg.src_type = JPEG_PIXEL_FORMAT_RGB888;
    jpeg_enc_cfg.subsampling = JPEG_SUBSAMPLE_420;
    jpeg_enc_cfg.quality = 60;
    jpeg_enc_cfg.rotate = JPEG_ROTATE_0D;
    jpeg_enc_cfg.task_enable = false;
    jpeg_enc_cfg.hfm_task_priority = 13;
    jpeg_enc_cfg.hfm_task_core = 1;

    jpeg_error_t ret = JPEG_ERR_OK;
    uint8_t *inbuf = test_rgb888_data;
    int image_size = jpeg_enc_cfg.width * jpeg_enc_cfg.height * 3;
    uint8_t *outbuf = NULL;
    int outbuf_size = 100 * 1024;
    int out_len = 0;
    jpeg_enc_handle_t jpeg_enc = NULL;
    FILE *out = NULL;

    // open
    ret = jpeg_enc_open(&jpeg_enc_cfg, &jpeg_enc);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // allocate output buffer to fill encoded image stream
    outbuf = (uint8_t *)calloc(1, outbuf_size);
    if (outbuf == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_example_exit;
    }

    // process
    ret = jpeg_enc_process(jpeg_enc, inbuf, image_size, outbuf, outbuf_size, &out_len);
    if (ret != JPEG_ERR_OK) {
        goto jpeg_example_exit;
    }

#if TEST_USE_SDCARD
    out = fopen("/sdcard/esp_jpeg_encode_one_picture.jpg", "wb");
    if (out == NULL) {
        goto jpeg_example_exit;
    }
    fwrite(outbuf, 1, out_len, out);
    fclose(out);
#endif  /* TEST_USE_SDCARD */

jpeg_example_exit:
    // close
    jpeg_enc_close(jpeg_enc);
    if (outbuf) {
        free(outbuf);
    }
    return ret;
}

jpeg_error_t esp_jpeg_encode_one_picture_block(void)
{
    // configure encoder
    jpeg_enc_config_t jpeg_enc_cfg = DEFAULT_JPEG_ENC_CONFIG();
    jpeg_enc_cfg.width = TEST_JPEG_X;
    jpeg_enc_cfg.height = TEST_JPEG_Y;
    jpeg_enc_cfg.src_type = JPEG_PIXEL_FORMAT_RGB888;
    jpeg_enc_cfg.subsampling = JPEG_SUBSAMPLE_420;
    jpeg_enc_cfg.quality = 60;
    jpeg_enc_cfg.rotate = JPEG_ROTATE_0D;
    jpeg_enc_cfg.task_enable = false;
    jpeg_enc_cfg.hfm_task_priority = 13;
    jpeg_enc_cfg.hfm_task_core = 1;

    jpeg_error_t ret = JPEG_ERR_OK;
    uint8_t *inbuf = test_rgb888_data;
    int image_size = jpeg_enc_cfg.width * jpeg_enc_cfg.height * 3;
    int block_size = 0;
    int num_times = 0;
    uint8_t *outbuf = NULL;
    int outbuf_size = 100 * 1024;
    int out_len = 0;
    jpeg_enc_handle_t jpeg_enc = NULL;
    FILE *out = NULL;

    // open
    ret = jpeg_enc_open(&jpeg_enc_cfg, &jpeg_enc);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // outbuf
    if ((outbuf = (uint8_t *)calloc(1, outbuf_size)) == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_example_exit;
    }

    // inbuf
    block_size = jpeg_enc_get_block_size(jpeg_enc);
    if ((inbuf = (uint8_t *)jpeg_calloc_align(block_size, 16)) == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_example_exit;
    }
    num_times = image_size / block_size;

    // process
    int in_offset = 0;
    for (size_t j = 0; j < num_times; j++) {
        // copy memory or read from SDCard
        memcpy(inbuf, test_rgb888_data + in_offset, block_size);
        in_offset += block_size;

        ret = jpeg_enc_process_with_block(jpeg_enc, inbuf, block_size, outbuf, outbuf_size, &out_len);
        if (ret < JPEG_ERR_OK) {
            goto jpeg_example_exit;
        }
    }

jpeg_example_exit:
#if TEST_USE_SDCARD
    out = fopen("/sdcard/esp_jpeg_encode_one_picture_block.jpg", "wb");
    if (out != NULL) {
        fwrite(outbuf, 1, out_len, out);
        fclose(out);
    }
#endif  /* TEST_USE_SDCARD */

    // close
    jpeg_enc_close(jpeg_enc);
    if (inbuf) {
        jpeg_free_align(inbuf);
    }
    if (outbuf) {
        free(outbuf);
    }
    return ret;
}
