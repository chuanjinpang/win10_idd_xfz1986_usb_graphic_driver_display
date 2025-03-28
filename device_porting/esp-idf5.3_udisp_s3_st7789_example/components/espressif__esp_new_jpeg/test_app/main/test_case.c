// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "test_decoder.h"
#include "test_encoder.h"
#include "image_io.h"

static const char *TAG = "JPEG";

#define JPEG_DECODER_LIMIT 16

TEST_CASE("test_demo", "[sys]")
{
    ESP_LOGI(TAG, "TEST");
}

TEST_CASE("test_system_heap", "[sys]")
{
    ESP_LOGI(TAG, "Internal free heap size: %ld bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "PSRAM    free heap size: %ld bytes", esp_get_free_heap_size() - esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "Total    free heap size: %ld bytes", esp_get_free_heap_size());
}

TEST_CASE("test_decoder_once", "[dec]")
{
    unsigned char *input_buffer = test_jpeg_data;
    int intput_len = sizeof(test_jpeg_data);
    unsigned char *ouput_buffer = NULL;
    int output_len = 0;
    uint8_t *curpix = NULL;

    // Test for decode process
    TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_decode_one_picture(input_buffer, intput_len, &ouput_buffer, &output_len));

    // Print output buffer
    // ESP_LOG_BUFFER_HEX(TAG, ouput_buffer, output_len);

    // Test for while pixel
    curpix = &ouput_buffer[(TEST_JPEG_WHITE_Y * TEST_JPEG_X + TEST_JPEG_WHITE_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[0][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[0][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[0][2], curpix[2]);

    // Test for black pixel
    curpix = &ouput_buffer[(TEST_JPEG_BLACK_Y * TEST_JPEG_X + TEST_JPEG_BLACK_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[1][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[1][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[1][2], curpix[2]);

    // Test for cyan pixel
    curpix = &ouput_buffer[(TEST_JPEG_CYAN_Y * TEST_JPEG_X + TEST_JPEG_CYAN_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[2][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[2][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[2][2], curpix[2]);

    // Test for red pixel
    curpix = &ouput_buffer[(TEST_JPEG_RED_Y * TEST_JPEG_X + TEST_JPEG_RED_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[3][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[3][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[3][2], curpix[2]);

    // Test for green pixel
    curpix = &ouput_buffer[(TEST_JPEG_GREEN_Y * TEST_JPEG_X + TEST_JPEG_GREEN_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[4][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[4][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[4][2], curpix[2]);

    // Test for blue pixel
    curpix = &ouput_buffer[(TEST_JPEG_BLUE_Y * TEST_JPEG_X + TEST_JPEG_BLUE_X) * 3];
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[5][0], curpix[0]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[5][1], curpix[1]);
    TEST_ASSERT_UINT8_WITHIN(JPEG_DECODER_LIMIT, test_color_value[5][2], curpix[2]);

    // Free output buffer
    if (ouput_buffer) {
        jpeg_free_align(ouput_buffer);
    }

    ESP_LOGI(TAG, "in_buffer: %p, in_len: %d, out_buffer: %p, out_len: %d", input_buffer, intput_len, ouput_buffer, output_len);
}

TEST_CASE("test_decoder_block", "[dec]")
{
    unsigned char *input_buffer = test_jpeg_data;
    int intput_len = sizeof(test_jpeg_data);

#if TEST_USE_SDCARD
    mount_sd();
#endif  /* TEST_USE_SDCARD */

    // Test for decode process
    TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_decode_one_picture_block(input_buffer, intput_len));

#if TEST_USE_SDCARD
    unmount_sd();
#endif  /* TEST_USE_SDCARD */
}

TEST_CASE("test_decoder_stream", "[dec]")
{
    unsigned char *input_buffer = test_jpeg_data;
    int intput_len = sizeof(test_jpeg_data);
    unsigned char *ouput_buffer = NULL;
    int output_len = 0;

    struct esp_jpeg_stream jpeg_handle = {0};

    TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_stream_open(&jpeg_handle));

    for (int frame_cnt = 0; frame_cnt < 10; frame_cnt++) {
        TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_stream_decode(&jpeg_handle, input_buffer, intput_len, &ouput_buffer, &output_len));
        ESP_LOGI(TAG, "frame_index: %d, in_buffer: %p, in_len: %d, out_buffer: %p, out_len: %d", frame_cnt, input_buffer, intput_len, ouput_buffer, output_len);

        // Free output buffer
        if (ouput_buffer) {
            jpeg_free_align(ouput_buffer);
        }
    }

    TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_stream_close(&jpeg_handle));
}

TEST_CASE("test_encoder_once", "[enc]")
{
#if TEST_USE_SDCARD
    mount_sd();
#endif  /* TEST_USE_SDCARD */

    // Test for encode process
    TEST_ASSERT_EQUAL(JPEG_ERR_OK, esp_jpeg_encode_one_picture());

#if TEST_USE_SDCARD
    unmount_sd();
#endif  /* TEST_USE_SDCARD */
}

TEST_CASE("test_encoder_block", "[enc]")
{
#if TEST_USE_SDCARD
    mount_sd();
#endif  /* TEST_USE_SDCARD */

    // Test for encode process
    TEST_ASSERT_GREATER_OR_EQUAL(JPEG_ERR_OK, esp_jpeg_encode_one_picture_block());

#if TEST_USE_SDCARD
    unmount_sd();
#endif  /* TEST_USE_SDCARD */
}
