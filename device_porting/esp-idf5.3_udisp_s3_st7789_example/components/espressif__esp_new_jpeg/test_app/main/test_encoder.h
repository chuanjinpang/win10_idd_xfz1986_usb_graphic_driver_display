// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#pragma once

#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Encode a single picture
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_encode_one_picture(void);

/**
 * @brief  Encode a single picture with block encoder API
 *
 * @return
 *       - JPEG_ERR_OK  Succeeded
 *       - Others       Failed
 */
jpeg_error_t esp_jpeg_encode_one_picture_block(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
