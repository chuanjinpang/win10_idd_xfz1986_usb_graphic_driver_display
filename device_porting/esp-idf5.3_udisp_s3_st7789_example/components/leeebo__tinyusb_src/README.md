## TinyUSB Core Code for ESP32Sx/Px

[![Push component to https://components.espressif.com](https://github.com/leeebo/tinyusb_src/actions/workflows/sync_esp_pkgmng.yml/badge.svg?branch=master)](https://github.com/leeebo/tinyusb_src/actions/workflows/sync_esp_pkgmng.yml)

The Core code of TinyUSB as ESP-IDF component, users can use the TinyUSB native API for project development based on ESP32Sx/Px.

For more information about TinyUSB, please refer https://docs.tinyusb.org

## Version

|Component Version|TinyUSB Base Commit|
|--|--|
|0.15.0~5| Jul 19, 2023 [acfaa44](https://github.com/hathach/tinyusb/commit/acfaa4494faccd615475e4ae9d3df940ed13d7af)|
|0.15.0~7| Nov 1, 2023 [68faa45](https://github.com/hathach/tinyusb/commit/68faa45c6a259f6d64b0c17526df48ec00e6717f)|
|0.16.0~1| Dec 22, 2023 [Tag 0.16.0](https://github.com/hathach/tinyusb/commit/1eb6ce784ca9b8acbbe43dba9f1d9c26c2e80eb0)|
|0.16.0~2| Mar 7, 2024 [a0e5626b](https://github.com/hathach/tinyusb/commit/a0e5626bc50d484a23f33000c48082179f0cc2dd)|
|0.16.0~3| May 9, 2024 [a0e5626b](https://github.com/hathach/tinyusb/commit/a0e5626bc50d484a23f33000c48082179f0cc2dd)|
|0.16.0~4| May 22, 2024 [a0e5626b](https://github.com/hathach/tinyusb/commit/a0e5626bc50d484a23f33000c48082179f0cc2dd)|
|0.16.0~5| Jun 19, 2024 [a0e5626b](https://github.com/hathach/tinyusb/commit/a0e5626bc50d484a23f33000c48082179f0cc2dd)|

## Feature

1. Choose between `dcd_esp32sx` or `dcd_dwc2` through Kconfig
2. Include below device drivers by default
   1. audio
   2. bth
   3. cdc
   4. dfu
   5. hid
   6. midi
   7. msc
   8. net
   9. usbtmc
   10. vendor
   11. video

## Develop Guide

Like other native examples from TinyUSB repository, users need to add a configuration file `tusb_config.h` to the project and make it visible to the TinyUSB component. please refer:

```cmake
idf_component_get_property(tusb_lib leeebo__tinyusb_src COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE include)
```