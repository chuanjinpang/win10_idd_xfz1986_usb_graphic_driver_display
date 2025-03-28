# ESP_NEW_JPEG

ESP_NEW_JPEG is Espressif's lightweight JPEG encoder and decoder library. The memory and CPU loading are optimized to make better use of Espressif chips.

## Features

### Encoder

- Support variety of width and height to encoder
- Support RGB888 RGBA YCbYCr YCbY2YCrY2 GRAY pixel format
- Support YUV444 YUV422 YUV420 subsampling
- Support quality(1-100)
- Support 0 90 180 270 degree clockwise rotation, under the follow cases
  1. src_type = JPEG_PIXEL_FORMAT_YCbYCr, subsampling = JPEG_SUBSAMPLE_420 and width and height are multiply of 16
  2. src_type = JPEG_PIXEL_FORMAT_YCbYCr, subsampling = JPEG_SUBSAMPLE_GRAY and width and height are multiply of 8
- Support mono-task and dual-task
- Support two mode encoder, respectively one image encoder and block encoder

### Decoder

- Support variety of width and height to decoder
- Support one and three channels decoder
- Support RGB888 RGB565(big endian) RGB565(little endian) CbYCrY pixel format output
- Support 0 90 180 270 degree clockwise rotation, under width and height are multiply of 8
- Support clipper and scale, under width and height are multiply of 8
- Support two mode decoder, respectively one image decoder and block decoder

## Performance

### Test on chip ESP32-S3

#### Encoder

For **mono task** encoder, the consume memory(10 kByte DRAM) is constant.  

| Resolution  | Source Pixel Format      | Output Quality | Output Subsampling | Frames Per Second(fps) |
|-------------|--------------------------|----------------|--------------------|------------------------|
| 1920 * 1080 | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 1.59                   |
| 1920 * 1080 | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 1.33                   |
| 1280 * 720  | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 4.84                   |
| 1280 * 720  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 2.92                   |
| 800 * 480   | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 10.82                  |
| 800 * 480   | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 6.74                   |
| 640 * 480   | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 13.24                  |
| 640 * 480   | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 8.32                   |
| 480 * 320   | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 24.35                  |
| 480 * 320   | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 15.84                  |
| 320 * 240   | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 45.30                  |
| 320 * 240   | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 30.37                  |

When **dual task** encoder in enabled, the wider the image, the more memory is consumed.  

| Resolution | Source Pixel Format      | Output Quality | Output Subsampling | Frames Per Second(fps) |
|------------|--------------------------|----------------|--------------------|------------------------|
| 1280 * 720 | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 4.62                   |
| 800 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 10.46                  |
| 640 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 12.89                  |
| 480 * 320  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 23.57                  |
| 320 * 240  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 43.97                  |

#### Decoder

The consume memory(10 kByte DRAM) is constant.  

Rotate JPEG_ROTATE_0D cases:

| Resolution  | Source Subsampling | Source Quality | Output Pixel Format         | Frames Per Second(fps) |
|-------------|--------------------|----------------|-----------------------------|------------------------|
| 1920 * 1080 | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 3.27                   |
| 1920 * 1080 | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 3.78                   |
| 1280 * 720  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 6.77                   |
| 1280 * 720  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 7.82                   |
| 800 * 480   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 14.73                  |
| 800 * 480   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 16.87                  |
| 640 * 480   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 17.90                  |
| 640 * 480   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 20.46                  |
| 480 * 320   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 32.27                  |
| 480 * 320   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 36.29                  |
| 320 * 240   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 58.95                  |
| 320 * 240   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 66.28                  |

Rotate JPEG_ROTATE_90D cases:

| Resolution  | Source Subsampling | Source Quality | Output Pixel Format         | Frames Per Second(fps) |
|-------------|--------------------|----------------|-----------------------------|------------------------|
| 1920 * 1080 | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 2.23                   |
| 1920 * 1080 | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 3.11                   |
| 1280 * 720  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 5.02                   |
| 1280 * 720  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 7.13                   |
| 800 * 480   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 14.09                  |
| 800 * 480   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 16.85                  |
| 640 * 480   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 17.16                  |
| 640 * 480   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 20.45                  |
| 480 * 320   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 30.87                  |
| 480 * 320   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 36.15                  |
| 320 * 240   | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 59.17                  |
| 320 * 240   | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 70.78                  |

### Test on chip ESP32-S2

#### Encoder

Only support mono task. The consume memory(10 kByte DRAM) is constant.  

| Resolution | Source Pixel Format      | Output Quality | Output Subsampling | Frames Per Second(fps) |
|------------|--------------------------|----------------|--------------------|------------------------|
| 800 * 480  | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 3.60                   |
| 800 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 2.76                   |
| 640 * 480  | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 4.47                   |
| 640 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 3.43                   |
| 480 * 320  | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 8.64                   |
| 480 * 320  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 6.69                   |
| 320 * 240  | JPEG_PIXEL_FORMAT_YCbYCr | 60             | JPEG_SUBSAMPLE_420 | 16.30                  |
| 320 * 240  | JPEG_PIXEL_FORMAT_RGB888 | 60             | JPEG_SUBSAMPLE_420 | 13.05                  |

#### Decoder

The consume memory(10 kByte DRAM) is constant.  

Rotate JPEG_ROTATE_0D cases:

| Resolution | Source Subsampling | Source Quality | Output Pixel Format         | Frames Per Second(fps) |
|------------|--------------------|----------------|-----------------------------|------------------------|
| 800 * 480  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 5.44                   |
| 800 * 480  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 5.76                   |
| 640 * 480  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 6.70                   |
| 640 * 480  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 7.09                   |
| 480 * 320  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 12.71                  |
| 480 * 320  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 13.42                  |
| 320 * 240  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 24.18                  |
| 320 * 240  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 25.60                  |

Rotate JPEG_ROTATE_90D cases:

| Resolution | Source Subsampling | Source Quality | Output Pixel Format         | Frames Per Second(fps) |
|------------|--------------------|----------------|-----------------------------|------------------------|
| 800 * 480  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 4.53                   |
| 800 * 480  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 4.81                   |
| 640 * 480  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 5.59                   |
| 640 * 480  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 5.94                   |
| 480 * 320  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 10.69                  |
| 480 * 320  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 11.33                  |
| 320 * 240  | JPEG_SUBSAMPLE_422 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 22.40                  |
| 320 * 240  | JPEG_SUBSAMPLE_420 | 60             | JPEG_PIXEL_FORMAT_RGB565_LE | 21.77                  |

## Usage

Please refer to the `test_app` folder for more details on API usage.

- `test_app/main/test_encoder.c` for encoder
  - Encode a single picture
  - Encode a single picture with block encoder API
- `test_app/main/test_decoder.c` for decoder
  - Decode a single JPEG picture
  - Decode a single JPEG picture with block deocder API
  - Decode JPEG stream of the same size

## FAQ

1. Does ESP_NEW_JPEG support decoding progressive JPEG?

   No, ESP_NEW_JPEG only support decoding baseline JPEG.

   You can use the following code to check if the image is progressive JPEG. Output 1 means progressive JPEG, 0 means baseline JPEG.

    ```bash
    python
    >>> from PIL import Image
    >>> Image.open("file_name.jpg").info.get('progressive', 0)
    ```

2. Why does the decoded output image appear misaligned?

   The issue typically occurs when a few columns of the image on the far left or right side appear on the opposite side. If you are using the ESP32-S3, a possible cause is that the output buffer is not 16-byte aligned. Please use the `jpeg_calloc_align` function to allocate output buffer.

3. How can I use a simpler method to check if the encoder's output is correct?

   You can use the following code to directly print the output JPEG data. Copy the output data, paste it into a hex editor, and save it as a .jpg file.

    ```c
    for (int i = 0; i < out_len; i++) {
        printf("%02x", outbuf[i]);
    }
    printf("\n");
    ```

## Supported chip

The following table shows the support of ESP_NEW_JPEG for Espressif SoCs. The "&#10004;" means supported, and the "&#10006;" means not supported.

| Chip     | v0.5.1   |
|----------|----------|
| ESP32    | &#10004; |
| ESP32-S2 | &#10004; |
| ESP32-S3 | &#10004; |
| ESP32-P4 | &#10004; |
| ESP32-C2 | &#10004; |
| ESP32-C3 | &#10004; |
| ESP32-C5 | &#10004; |
| ESP32-C6 | &#10004; |
