# win10_idd_xfz1986_usb_graphic_driver_display

it's opensource project, plan release source code on 2022/01/01.
if you want get source code before that schedule. please comment your email address in  below demo video in https://www.bilibili.com/video/BV1tU4y1F7B6?spm_id_from=333.999.0.0

overview

it's a USB mini display for win10 with esp32-s2 kit board + SPI LCD display (ili9341 or st7789).

it refer many opensource projects:  thanks
1.github.com/microsoft/Windows-driver-samples/tree/master/video/IndirectDisplay
2.git://github.com/roshkins/IddSampleDriver.git
3.Bodmer/TFT_eSPI.git 
4.nopnop2002/esp-idf-ili9340，
5.serge-rgb/TinyJPEG.git
6.TJpgDec。

esp32s2 support USB OTG, the Linux host compress framebuffer Zone with JPEG, and then issue URB to esp32s2, the S2 wil decode JPEG stream bytes to RGB data,and use DMA SPI to ili9341 screen.

now it can run ~13pfs in most time.

folder intro:

/bin  include win10 driver and esp32-s2 hex bin files

win10_idd_xfz1986_usb_graphic_driver_display_readme.doc is a file for how to Hardware connect and other info in CN

install:

install win10 driver, flash your esp32-s2 board with bin files. then plus USB cable, it should work like below img.

Demo

![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/all.jpg)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/drv.png)
![image](https://github.com/chuanjinpang/win10_idd_xfz1986_usb_graphic_driver_display/blob/main/demo/setting.png)


please seem below links for view demo:

https://www.bilibili.com/video/BV1tU4y1F7B6?spm_id_from=333.999.0.0
