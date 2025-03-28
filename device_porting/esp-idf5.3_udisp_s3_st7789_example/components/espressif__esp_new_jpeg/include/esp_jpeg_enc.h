// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DEFAULT_JPEG_ENC_CONFIG() {                 \
    .width             = 320,                       \
    .height            = 240,                       \
    .src_type          = JPEG_PIXEL_FORMAT_YCbYCr,  \
    .subsampling       = JPEG_SUBSAMPLE_420,        \
    .quality           = 40,                        \
    .rotate            = JPEG_ROTATE_0D,            \
    .task_enable       = false,                     \
    .hfm_task_priority = 13,                        \
    .hfm_task_core     = 1,                         \
}

/**
 * @brief  JPEG encoder handle
 */
typedef void *jpeg_enc_handle_t;

/* JPEG configure Configuration */
typedef struct jpeg_info {
    int                 width;              /*!< Image width */
    int                 height;             /*!< Image height */
    jpeg_pixel_format_t src_type;           /*!< Input image type */
    jpeg_subsampling_t  subsampling;        /*!< JPEG chroma subsampling factors. If `src_type` is JPEG_PIXEL_FORMAT_YCbY2YCrY2, this must be `JPEG_SUBSAMPLE_GRAY` or `JPEG_SUBSAMPLE_420`. Others is un-support */
    uint8_t             quality;            /*!< Quality: 1-100, higher is better. Typical values are around 40 - 100. */
    jpeg_rotate_t       rotate;             /*!< Supports 0, 90 180 270 degree clockwise rotation.
                                                 Under src_type = JPEG_PIXEL_FORMAT_YCbYCr, subsampling = JPEG_SUBSAMPLE_420, width % 16 == 0. height % 16 = 0 conditions, rotation enabled.
                                                 Otherwise unsupported */
    bool                task_enable;        /*!< True: `jpeg_enc_open` would create task to finish part of encoding work. false: no task help the encoder encode */
    uint8_t             hfm_task_priority;  /*!< Task priority. If task_enable is true, this must be set */
    uint8_t             hfm_task_core;      /*!< Task core. If task_enable is true, this must be set */
} jpeg_enc_config_t;

/**
 * @brief  Open JPEG encoder
 *
 * @param[in]   info      The configuration information
 * @param[out]  jpeg_enc  The JPEG encoder handle
 *
 * @return
 *       - positive  value  JPEG encoder handle
 *       - NULL      refer to `jpeg_error_t`
 */
jpeg_error_t jpeg_enc_open(jpeg_enc_config_t *info, jpeg_enc_handle_t *jpeg_enc);

/**
 * @brief  Encode one image
 *
 * @param[in]   jpeg_enc     The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param[in]   in_buf       The input buffer, It needs a completed image. The buffer must be aligned 16 byte.
 * @param[in]   inbuf_size   The size of `in_buf`. The value must be size of a completed image.
 * @param[out]  out_buf      The output buffer
 * @param[out]  outbuf_size  The size of output buffer
 * @param[out]  out_size     The size of JPEG image
 *
 * @return
 *       - JPEG_ERR_OK  It has finished to encode one image.
 *       - other        values    refer to `jpeg_error_t`
 */
jpeg_error_t jpeg_enc_process(const jpeg_enc_handle_t jpeg_enc, const uint8_t *in_buf, int inbuf_size, uint8_t *out_buf, int outbuf_size, int *out_size);

/**
 * @brief  Get block size. Block size is minimum process unit.
 *
 * @param[in]  jpeg_enc  The JPEG encoder handle. It gained from `jpeg_enc_open`
 *
 * @return
 *       - positive  value  block size
 *       - other     values    not valid data
 */
int jpeg_enc_get_block_size(const jpeg_enc_handle_t jpeg_enc);

/**
 * @brief  Encode block size image. Get block size from `jpeg_enc_get_block_size`
 *
 * @param[in]   jpeg_enc     The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param[in]   in_buf       The input buffer, It needs a completed image.
 * @param[in]   inbuf_size   The size of `in_buf`. Get block size from  `jpeg_enc_get_block_size`
 * @param[out]  out_buf      The output buffer, it saves a completed JPEG image.
 * @param[out]  outbuf_size  The size of output buffer
 * @param[out]  out_size     The size of JPEG image
 *
 * @return
 *       - block          size      It has finished to encode current block size image.
 *       - JPEG_ERR_OK    It has finished to encode one image.
 *       - JPEG_ERR_FAIL  Encoder failed.
 */
jpeg_error_t jpeg_enc_process_with_block(const jpeg_enc_handle_t jpeg_enc, const uint8_t *in_buf, int inbuf_size, uint8_t *out_buf, int outbuf_size, int *out_size);

/**
 * @brief  Reset quality.
 *
 * @note  After encoding an entire image, user can call this function to change the quality value.
 *
 * @param[in]  jpeg_enc  The JPEG encoder handle. It gained from `jpeg_enc_open`
 * @param[in]  q         Quality: 1-100, higher is better. Typical values are around 40 - 100.
 *
 * @return
 *       - JPEG_ERR_OK  succeed
 *       - others       values   refer to `jpeg_error_t`
 */
jpeg_error_t jpeg_enc_set_quality(const jpeg_enc_handle_t jpeg_enc, uint8_t q);

/**
 * @brief  Close JPEG encoder
 *
 * @param[in]  jpeg_enc  The JPEG encoder handle. It gained from `jpeg_enc_open`
 *
 * @return
 *       - JPEG_ERR_OK  It has finished to deinit.
 *       - other        values    refer to `jpeg_error_t`
 */
jpeg_error_t jpeg_enc_close(jpeg_enc_handle_t jpeg_enc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
