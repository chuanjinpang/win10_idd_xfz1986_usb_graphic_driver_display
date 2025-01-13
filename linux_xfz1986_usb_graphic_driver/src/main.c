/*
 *    xfz1986 usb graphic Display Linux Driver sync from win10 version
 *    base on robopeak, however we do a major change, almost rewrite usb part,
 *    ,use standard HID touch via composite usb dev for input, add multi-encoder support,and so on.
 *    Copyright (C) 2019 - 2024 
 *    This file is licensed under the GPL. See LICENSE in the package.
 *    support OS: win10/win11 , linux ubuntu and raspi
 *    support device: esp32s2/s3/p4, allwinner t113  open for compatible devices.
 *
 *    Author xfz1986
 *
 */
#include "inc/base_type.h"
#include "inc/usb_transfer.h"
#include "inc/fbhandlers.h"
#include "inc/log.h"

int debug_level=LOG_LEVEL_INFO;
module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "debug level (override kernel config)");

int scale_thd=200;
module_param(scale_thd, int, 0644);
MODULE_PARM_DESC(scale_thd, "scale threshold for scale double for samll screen dev");

static int __init xfz1986_usb_disp_init(void)
{
    int ret;

    ret = usb_transfer_init();
    if(ret) {
        LOGE("usb_register failed. Error number %d", ret);       
    }
    return ret;
}


static void __exit xfz1986_usb_disp_exit(void)
{
    usb_transfer_exit();
}

module_init(xfz1986_usb_disp_init);
module_exit(xfz1986_usb_disp_exit);

#define DRIVER_LICENSE_INFO    "GPL"
#define DRIVER_VERSION         "xfz1986 usb graphic drv v1.00"

MODULE_AUTHOR("pangcj<2426510@qq.com>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE(DRIVER_LICENSE_INFO);

