// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#pragma once

#include <stdint.h>
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct esp_jpeg_stream {
    jpeg_dec_handle_t       jpeg_dec;
    jpeg_dec_io_t          *jpeg_io;
    jpeg_dec_header_info_t *out_info;
    jpeg_pixel_format_t     output_type;
};
typedef struct esp_jpeg_stream *esp_jpeg_stream_handle_t;

/**
 * @brief  Decode a single JPEG picture
 *
 * @param  input_buf   Pointer to the input buffer containing JPEG data
 * @param  len         Length of the input buffer in bytes
 * @param  output_buf  Pointer to output buffer, allocated in `esp_jpeg_decoder_one_picture` but it won't to free. Please release this buffer after decoding is complete.
 * @param  out_len     Acturally output length in bytes
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len, uint8_t **output_buf, int *out_len);

/**
 * @brief  Decode a single JPEG picture with block deocder API
 *
 * @param  input_buf  Pointer to the input buffer containing JPEG data
 * @param  len        Length of the input buffer in bytes
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_decode_one_picture_block(unsigned char *input_buf, int len);

/**
 * @brief  Open a JPEG stream handle for decoding
 *
 * @param  jpeg_handle  Handle to the JPEG stream
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_stream_open(esp_jpeg_stream_handle_t jpeg_handle);

/**
 * @brief  Decode a portion of the JPEG stream
 *
 * @param  jpeg_handle  Handle to the JPEG stream
 * @param  input_buf    Pointer to the input buffer containing JPEG data
 * @param  len          Length of the input buffer in bytes
 * @param  output_buf   Pointer to output buffer, allocated in `esp_jpeg_stream_decode` but it won't to free. Please release this buffer after decoding is complete.
 * @param  out_len      Acturally output length in bytes
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_stream_decode(esp_jpeg_stream_handle_t jpeg_handle, uint8_t *input_buf, int len, uint8_t **output_buf, int *out_len);

/**
 * @brief  Close the JPEG stream
 *
 * @param  jpeg_handle  Handle to the JPEG stream
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_stream_close(esp_jpeg_stream_handle_t jpeg_handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
