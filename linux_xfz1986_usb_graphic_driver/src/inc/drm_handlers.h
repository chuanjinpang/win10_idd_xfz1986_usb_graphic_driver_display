/*
 *    xfz1986 USB Graphic Display Linux Driver
 *
 *    Copyright (C) 2019 - 2025
 *    This file is licensed under the GPL. See LICENSE in the package.
 *
 *    Author xfz1986
 *
 *   ---------------------------------------------------
 *    DRM/KMS Display Handlers (replaces legacy fbdev)
 *    Referenced from linux/drivers/gpu/drm/tiny/gm12u320.c
 */

#ifndef _XFZ1986_DRM_HANDLERS_H
#define _XFZ1986_DRM_HANDLERS_H

#include <drm/drm_drv.h>

struct xfz1986_drm_device;

int xfz1986_drm_device_init(struct xfz1986_drm_device *xdev);
void xfz1986_drm_device_fini(struct xfz1986_drm_device *xdev);

/* Exported for usb_transfer.c probe */
extern const struct drm_driver xfz1986_drm_driver;

#endif
