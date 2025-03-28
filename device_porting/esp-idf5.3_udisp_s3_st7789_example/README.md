# USB 扩展屏示例

本示例源自乐鑫官方例程：[usb_extend_screen](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/device/usb_extend_screen)。**请注意：该示例需要在在 ``idf 5.3`` 环境下运行。**

USB 扩展屏示例可以将 ESP-SparkBot 开发板作为一块 windows 的副屏。支持以下功能。

* 支持 480*480@60FPS 的屏幕刷新速率

* 支持音频的输入和输出
  * 注意：因为 S3 使用 USB FS，所以带宽资源相对优先，若想提升体验可以更换为支持 USB HS 的 [ESP32-P4](https://www.espressif.com/zh-hans/products/socs/esp32-p4)

## 所需硬件

* 开发板

   1. [ESP-SparkBot](https://oshwhub.com/esp-college/esp-sparkbot) 开发板

* 连接

    1. 将开发板上 USB 口连接到 PC 上

## 编译和烧录

### 设备端

构建项目并将其烧录到板子上，然后运行监控工具以查看串行输出：

* 运行 `idf.py set-target esp32s3` 以设置目标芯片
* 如果上一步出现任何错误，请运行 `pip install "idf-component-manager~=1.1.4"` 来升级你的组件管理器
* 运行 `idf.py -p PORT flash` 来构建并烧录项目

请参阅《入门指南》了解配置和使用 ESP-IDF 构建项目的所有步骤。

>> 注意：此示例将在线获取 AVI 文件。请确保首次编译时连接互联网。

### PC 端

准备工作，请参考 [windows_driver](./windows_driver/README.md)

## 其他问题

## 再次烧录
* 烧录代码后会占用 ESP32-S3 的 USB 端口，**若想再次烧录需手动使芯片进入下载模式**。按住开发板背后的 Boot 键（GPIO0）再插入电源，若有电池可以先拆掉。

## 查看 LOG

* 烧录代码后会占用 ESP32-S3 的 USB 端口，可以通过开发板下方的 UART 口查看 LOG 进行 debug 调试，配置输出 GPIO：

  ```
  CONFIG_ESP_CONSOLE_UART_TX_GPIO=38
  CONFIG_ESP_CONSOLE_UART_RX_GPIO=48
  ```

### 调高/调低 JPEG 的图片质量

* 修改 `usb_descriptor.c` 文件中的 `string_desc_arr` Vendor 接口字符串，将 `Ejpg4` 修改为所需的图片质量，数字越大质量越高，同样的一帧图像占用内存更多。

### 修改屏幕分辨率

* 修改 `usb_descriptor.c` 文件中的 `string_desc_arr` Vendor 接口字符串，将 `R240x240` 修改为所需的屏幕大小
