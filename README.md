
# update windows10/11 usb display to version 2.0
20240725

主要更新：

##　windows驱动程序：

1.支持设备报告其设备显示信息，如屏幕分辨率、解码数据模式，编码数据质量。

2.自动适应设备屏幕分辨率。通过设备报告usb产品字符串中的屏幕大小信息。

3.支持多种编码屏幕数据格式：如rgb16（表示rgb565）、rgb32（表示rgb888x）、jepg。

4.添加编码器基类，支持多编码算法。只需为encode基类添加一个实现。

例如：

esp32udisp0_R320x240_Ejpg4_Ergb16

t113udisp1_R48x480_Ejpg6_rgb16_rgb32

R表示：分辨率 R320x240
  宽度x高度信息

E表示：Ecode模式

jpg质量为1-10。对于esp32s2，它使用低质量来获得高fps。对于t113-s3可以支持更高质量Ejpg6

rgb16表示数据的rgb565模式。驱动程序将把屏幕数据编码为rgb565格式。

5.代码风格更清晰。将源代码拆分为多个文件。usb部分，编码部分，idd部分。

6.重构usb数据传输协议。删除每个usb包起始字节。只需使用帧头+编码数据内容。

因此，添加新的编码格式和传输所需的数据很容易。例如，jpg数据只填充到帧头后，不需要任何更改。而v1.0需要按每个usb包大小长度就增加一个start byte.
有一个奇怪的建包，解包的过程。其实没有必要。对于jpg包，帧头后面就是纯粹的jpg数据，直接送去解码就好了。


## esp32s2设备端：

1.通过产品字符串来报告设备信息。esp32udisp0_R320x240_Ejpg4_Ergb16

2.由于传输协议更改，更改usb数据帧解析。我们删除每个usb包中的起始字节。其实USB没必要，所以就干掉了。

3.将usb设备id从303a/1986更改为303a/2986。提醒我们有大变化，要对应更新。

major change:
## windows driver:
1. support device report its' device display info, such as screen resolution, decode data mode. encode data quality.
2. Automatically adapt to devie screen resolution via device report it's screen size in usb production string.
3. support multi encode screen data format for usb transfer: such as rgb16 (means rgb565), rgb32 (mean rgb888x), jepg.
4. add encoder base class for support multi-encode algo. just add an implementation for encode base class.
for example:
esp32udisp0_R320x240_Ejpg4_Ergb16
t113udisp1_R48x480_Ejpg6_Ergb16_Ergb32   
R means: Resolution  width x height
E means: Ecode mode 
  jpg with quality 1-10 end.  for esp32s2 it use low quality for high fps. for t113-s3 can support high quality
  rgb16 means rgb565 mode for data. driver will encode screen data to rgb565 format.
5. more clearly code style. split source code to mulit-files. usb part, encode part. idd part.
6. refactor usb data transfer protocol. delete usb package start bytes. just use frame header +  encode data playload.
   so it easy to add new encode format & transfer data you wanted. for example, the encode jpg data just fill to playload without any change.


## esp32s2 device side:
1. add device info report via product string. esp32udisp0_R320x240_Ejpg4_Ergb16
2. change usb data frame parse due to transfer protocol change. we delete the start byte in each usb package. it's no need any more.
3. change usb device id from 303a/1986 to 303a/2986. yes, we change major, we need update to version 2.0
   
for old driver:
please checkout tag v1.0

-------------------
# win10_idd_xfz1986_usb_graphic_driver_display

本项目是开源的，教程参考文件：win10_idd_xfz1986_usb_graphic_driver_display_readme。内有编译，安装，硬件连接等等信息。

单片机esp32s2+SPI屏实现一个win10 USB接口显示器。

本项目借鉴了众多开源项目，主要借鉴：

*1.github.com/microsoft/Windows-driver-samples/tree/master/video/IndirectDisplay

*2.git://github.com/roshkins/IddSampleDriver.git

*3.Bodmer/TFT_eSPI.git 

*4.nopnop2002/esp-idf-ili9340

*5.serge-rgb/TinyJPEG.git

*6.TJpgDec。

目前FPS在~13FPS,纯黑屏幕时能摸到20FPS。
主机使用IDD显示驱动方案，将屏幕进行JPEG压缩，然后通过URB（USB请求包）发送到下位机。下位机解压并发DMA传输写屏达到高性能。下位机esp32s2只支持全速度12Mhz,所以必须高压缩的JPEG才能有高FPS.
为了获得较稳定的FPS，采用了动态码率策略，会依据图像情况，进行压缩率调整。

# overview

it's a USB mini display for win10 with esp32-s2 kit board + SPI LCD display (ili9341 or st7789).

it refer many opensource projects:  thanks

*1.github.com/microsoft/Windows-driver-samples/tree/master/video/IndirectDisplay

*2.git://github.com/roshkins/IddSampleDriver.git

*3.Bodmer/TFT_eSPI.git 

*4.nopnop2002/esp-idf-ili9340

*5.serge-rgb/TinyJPEG.git

*6.TJpgDec。

esp32s2 support USB OTG, the Linux host compress framebuffer Zone with JPEG, and then issue URB to esp32s2, the S2 wil decode JPEG stream bytes to RGB data,and use DMA SPI to ili9341 screen.

now it can run ~13pfs in most time.

# folder intro:

/bin  include win10 driver and esp32-s2 hex bin files　　windows10驱动和esp32s2的固件测试文件

/src  源代码目录 source code for host and device

esp-idf-esp32s2_usbdisp_firmware    firmware source code for esp32s2 base esp-idf  v4.3-beta3

win10_idd_xfz1986_usb_graphic_driver  windows driver 

win10_idd_xfz1986_usb_graphic_driver_display_readme.doc is a file for how to Hardware connect and other info in CN 主要的文档，有编译，安装与硬件连接图，元件清单等等

#install:

install win10 driver, flash your esp32-s2 board with bin files. then plus USB cable, it should work like below img.

# Demo

![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/all.jpg)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/esp32s2.jpg)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/drv.png)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/setting.png)


please seem below links for view demo:

https://www.bilibili.com/video/BV1tU4y1F7B6?spm_id_from=333.999.0.0
