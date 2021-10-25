# win10_idd_xfz1986_usb_graphic_driver_display

it's opensource project, plan release source code on 2022/01/01.

if you want get source code before that schedule. 

please comment your email address in  below demo video in https://www.bilibili.com/video/BV1tU4y1F7B6?spm_id_from=333.999.0.0



单片机esp32s2+SPI屏实现一个win10 USB接口显示器。

本项目借鉴了众多开源项目，主要借鉴：


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

/bin  include win10 driver and esp32-s2 hex bin files

win10_idd_xfz1986_usb_graphic_driver_display_readme.doc is a file for how to Hardware connect and other info in CN

#install:

install win10 driver, flash your esp32-s2 board with bin files. then plus USB cable, it should work like below img.

# Demo

![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/all.jpg)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/esp32s2.jpg)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/drv.png)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/setting.png)


please seem below links for view demo:

https://www.bilibili.com/video/BV1tU4y1F7B6?spm_id_from=333.999.0.0
