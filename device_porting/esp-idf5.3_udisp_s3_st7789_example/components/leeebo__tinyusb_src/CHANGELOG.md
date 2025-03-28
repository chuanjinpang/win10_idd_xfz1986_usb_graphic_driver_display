# ChangeLog

## v0.16.0~6 - 2024-07-31

* Fixed define for DWC_EP_MAX value(dwc2_esp32)

## v0.16.0~5 - 2024-06-19

* Support source code only mode. enable `TINYUSB_SOURCE_CODE_ONLY` (disable by default) to not build as static library. This is useful for projects that want to include tinyusb source code directly in their project.

## v0.16.0~4 - 2024-05-09

* Temporary merge https://github.com/hathach/tinyusb/pull/2656 for uvc frame based

## v0.16.0~3 - 2024-05-09

* Temporary merge https://github.com/hathach/tinyusb/pull/2328 for uac

## v0.16.0~2 - 2024-03-07

* Add build Workflow
* Support ESP32-P4
* Rebase upstream latest

## v0.15.0~6 - 2023-07-21

* Fix UVC Header EOH for compatibility

## v0.15.0~5 - 2023-07-19

* Rebase upstream latest master [acfaa4494f](https://github.com/hathach/tinyusb/commit/acfaa4494faccd615475e4ae9d3df940ed13d7af)
* Using new versioning scheme

## v0.0.4 - 2023-06-21

* Remove dependency `main`, please check README.md `Develop Guide` for details.

## v0.0.3 - 2023-06-21

* Rebase upstream latest master [6cf735031f3](https://github.com/hathach/tinyusb/commit/6cf735031f35cd223231b7f94b8c3caa8286cb9e)