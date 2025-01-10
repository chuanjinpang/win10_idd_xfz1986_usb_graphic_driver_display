/*
 *    xfz1986 usb graphic Display Linux Driver sync from win10 version
 *
 *    Copyright (C) 2019 - 2024 
 *    This file is licensed under the GPL. See LICENSE in the package.
 *
 *    Author pcj
 *
 */
#ifndef _USB_TRANSFER_H
#define _USB_TRANSFER_H

#include <linux/kernel.h>

#include "inc/enc_base.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DISP_MAX_WIDTH  1080
#define DISP_MAX_HEIGHT 720

#define DISP_MAX_TRANSFER_SIZE (DISP_MAX_HEIGHT*DISP_MAX_WIDTH*4)
typedef struct {
    struct list_head node;
    int id;
    uint8_t		urb_msg[DISP_MAX_TRANSFER_SIZE];
    struct urb                      *  urb;
    struct xfz1986_udisp_dev            *  dev;
} urb_itm_t, *purb_itm_t;


typedef struct _urb_mgr_t{
    struct list_head                   urb_list;
    spinlock_t                         oplock;
    size_t                             urb_count;
} urb_mgr_t;


#define FPS_STAT_MAX 32

typedef struct {
    long tb[FPS_STAT_MAX];
    int cur;
    long last_fps;
} fps_mgr_t;



struct xfz1986_udisp_dev {
    // timing and sync
    int                                dev_id;
    struct mutex                       op_locker;
    __u8                               is_alive;

    // usb device info
    struct usb_device               *  udev;
    struct usb_interface            *  interface;

    // display data related
    __u8                               disp_out_ep_addr;
    size_t                             disp_out_ep_max_size;

     void                            *  fb_info;

    //urb_item mgr
    urb_mgr_t  urb_mgr;

	// dev info
	char udisp_dev_info[128];
    // encoder
    enc_base_t * encoder;
	enc_base_t  _encoder;
	int reg;
    int w;
	int h;
	int enc;
	int quality; //for jpg case so far
	int fps;

    fps_mgr_t fps_mgr;
	
};

int usb_transfer_init(void);
void usb_transfer_exit(void);

void xfz1986_udisp_usb_set_fbhandle(struct xfz1986_udisp_dev * dev, void * fbhandle);
void * xfz1986_udisp_usb_get_fbhandle(struct xfz1986_udisp_dev * dev);

int usb_send_msg_async(struct xfz1986_udisp_dev * dev,urb_itm_t * urb, const uint8_t * msg_data, int msg_size);

int xfz1986_udisp_usb_try_send_jpeg_image(struct xfz1986_udisp_dev * dev, const pixel_type_t * framebuffer, int x, int y, int right, int bottom);
#ifdef __cplusplus
}  // extern C
#endif

#endif
