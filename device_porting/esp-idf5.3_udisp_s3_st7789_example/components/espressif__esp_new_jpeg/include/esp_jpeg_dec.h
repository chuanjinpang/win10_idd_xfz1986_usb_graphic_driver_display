// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#pragma once

#include <stdbool.h>
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DEFAULT_JPEG_DEC_CONFIG() {             \
    .output_type  = JPEG_PIXEL_FORMAT_RGB888,   \
    .scale        = {.width = 0, .height = 0},  \
    .clipper      = {.width = 0, .height = 0},  \
    .rotate       = JPEG_ROTATE_0D,             \
    .block_enable = false,                      \
}

/**
 * @brief  JPEG decoder handle
 */
typedef void *jpeg_dec_handle_t;

/**
 * @brief  Configuration for JPEG decoder
 *
 * @note  Feature supported and limitations
 *        - support scale. Maximum scale ratio is 1/8. Scale.height and scale.width require integer multiples of 8.
 *        - support clipper. Clipper.height and clipper.width require integer multiples of 8.
 *        - support 0, 90, 180 and 270 clockwise rotation. Width and height require integer multiples of 8.
 *        - support block decoder mode.
 *
 *        The general flow of decoder
 *        image data ----> decoding (required) ----> scale (optional) ----> clipper (optional) ----> rotation (optional)
 *
 *            Each case of general flow:
 *            image -> decoding: The variety of width and height supported.
 *            image -> decoding -> scale: The scale.height and scale.width require integer multiples of 8.
 *            image -> decoding -> clipper: The clipper.height and clipper.width require integer multiples of 8.
 *            image -> decoding -> rotation: The height and width of image require integer multiples of 8.
 *            image -> decoding -> scale -> clipper: The scale.height, scale.width, clipper.height and clipper.width require integer multiples of 8. The resolution of clipper should be less or equal than scale.
 *            image -> decoding -> scale -> rotation: The scale.height and scale.width require integer multiples of 8.
 *            image -> decoding -> clipper -> rotation: The clipper.height and clipper.width require integer multiples of 8.
 *            image -> decoding -> scale -> clipper -> rotation: The scale.height, scale.width, clipper.height and clipper.width require integer multiples of 8. The resolution of clipper should be less or equal than scale.
 *
 *        Flow of block decoder mode
 *        image data ----> decoding
 */
typedef struct {
    jpeg_pixel_format_t output_type;   /*!< Output pixel format. Support RGB888 RGB565_BE RGB565_LE CbYCrY, see jpeg_pixel_format_t enumeration. */
    jpeg_resolution_t   scale;         /*!< Resolution of scale. scale.width = 0 means disable the width scale. scale.height = 0 means disable the height scale.
                                            If enable this mode, scale.width and scale.height require integer multiples of 8. */
    jpeg_resolution_t   clipper;       /*!< Resolution of clipper. clipper.width = 0 means disable the width clipper. clipper.height = 0 means disable the height clipper.
                                            If enable this mode, clipper.width and clipper.height require integer multiples of 8. */
    jpeg_rotate_t       rotate;        /*!< Support 0 90 180 270 degree clockwise rotation.
                                            Only support when both width and height are multiples of 8. Otherwise unsupported. */
    bool                block_enable;  /*!< Configured to true to enable block mode, each time output 8 or 16 line data depending on the height of MCU(Minimum Coded Unit).
                                            If enabled this mode, scale and clipper are not supported, the width and height require integer multiples of 8 and rotate require JPEG_ROTATE_0D.
                                            Configured to false to disable block mode. */
} jpeg_dec_config_t;

/**
 * @brief  JPEG decoder output infomation
 */
typedef struct {
    uint16_t width;   /*!< Image width */
    uint16_t height;  /*!< Image height */
} jpeg_dec_header_info_t;

/**
 * @brief  JPEG decoder io control
 */
typedef struct {
    uint8_t *inbuf;         /*!< The input buffer pointer */
    int      inbuf_len;     /*!< The length of the input buffer in bytes */
    int      inbuf_remain;  /*!< Not used length of the input buffer */
    uint8_t *outbuf;        /*!< The buffer to store decoded data. The buffer must be aligned 16 byte. */
    int      out_size;      /*!< Output size of current block when block mode is enabled.
                                 Size of entire image when block mode is disabled. */
} jpeg_dec_io_t;

/**
 * @brief  Create a JPEG decoder handle, set user configuration infomation to decoder handle
 *
 * @param[in]   config    Pointer to configuration information
 * @param[out]  jpeg_dec  Pointer to JPEG decoder handle
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to create handle
 */
jpeg_error_t jpeg_dec_open(jpeg_dec_config_t *config, jpeg_dec_handle_t *jpeg_dec);

/**
 * @brief  Parse JPEG header, and output infomation to user
 *
 * @param[in]   jpeg_dec  JPEG decoder handle
 * @param[in]   io        Pointer to io control struct
 * @param[out]  out_info  Pointer to output infomation struct
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to parse JPEG header
 */
jpeg_error_t jpeg_dec_parse_header(jpeg_dec_handle_t jpeg_dec, jpeg_dec_io_t *io, jpeg_dec_header_info_t *out_info);

/**
 * @brief  Get image buffer size
 *
 * @note  User buffer must align to 16 bytes, user can malloc it through helper function `jpeg_calloc_align`.
 *
 * @param[in]   jpeg_dec    JPEG decoder handle
 * @param[out]  outbuf_len  Image buffer size
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to get output buffer size
 */
jpeg_error_t jpeg_dec_get_outbuf_len(jpeg_dec_handle_t jpeg_dec, int *outbuf_len);

/**
 * @brief  Get the number of times to call `jpeg_dec_process`
 *
 * @note  If disable block decoder mode, the process_count always be 1.
 *
 * @param[in]   jpeg_dec       JPEG decoder handle
 * @param[out]  process_count  Number of times
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to get process times
 */
jpeg_error_t jpeg_dec_get_process_count(jpeg_dec_handle_t jpeg_dec, int *process_count);

/**
 * @brief  Decode JPEG picture
 *
 * @param[in]  jpeg_dec  JPEG decoder handle
 * @param[in]  io        Pointer to io control struct
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to decode JPEG picture
 */
jpeg_error_t jpeg_dec_process(jpeg_dec_handle_t jpeg_dec, jpeg_dec_io_t *io);

/**
 * @brief  Close JPEG decoder
 *
 * @param[in]  jpeg_dec  JPEG decoder handle
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Fail to close JPEG decoder
 */
jpeg_error_t jpeg_dec_close(jpeg_dec_handle_t jpeg_dec);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
