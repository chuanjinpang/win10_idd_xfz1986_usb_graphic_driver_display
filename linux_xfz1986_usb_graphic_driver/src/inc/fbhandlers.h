/*
 *    RoboPeak USB LCD Display Linux Driver
 *
 *    Copyright (C) 2009 - 2013 RoboPeak Team
 *    This file is licensed under the GPL. See LICENSE in the package.
 *
 *    http://www.robopeak.net
 *
 *    Author Shikai Chen
 *
 *   ---------------------------------------------------
 *    Definition of Frame Buffer Handlers
 */

#ifndef _RPUSBDISP_FBHANDLERS_H
#define _RPUSBDISP_FBHANDLERS_H


int register_fb_handlers(struct xfz1986_udisp_dev * dev);
void unregister_fb_handlers(struct xfz1986_udisp_dev * dev);






#endif

