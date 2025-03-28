/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */


#ifndef _DWC2_ESP32_H_
#define _DWC2_ESP32_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "esp_intr_alloc.h"
#include "soc/interrupts.h"

#if CFG_TUSB_MCU == OPT_MCU_ESP32P4
  #define DWC2_PERIPH_COUNT    2
  #define DWC2_HS_PERIPH_BASE  0x50000000UL
  #define DWC2_FS_PERIPH_BASE  0x50040000UL
  #define DWC2_HS_INTR_SOURCE  ETS_USB_OTG_INTR_SOURCE
  #define DWC2_FS_INTR_SOURCE  ETS_USB_OTG11_CH0_INTR_SOURCE
#elif TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  #define DWC2_PERIPH_COUNT    1
  #define DWC2_FS_PERIPH_BASE  0x60080000UL
  #define DWC2_FS_INTR_SOURCE  ETS_USB_INTR_SOURCE
#else
  #error "DWC2_REG_BASE not defined for current MCU"
#endif

#if DWC2_HS_PERIPH_BASE
  #define DWC2_HS_EP_MAX       15             // USB_OUT_EP_NUM
  #define DWC2_HS_EP_FIFO_SIZE 4096
#endif

#if DWC2_FS_PERIPH_BASE
  #define DWC2_FS_EP_MAX       6             // USB_OUT_EP_NUM. TODO ESP32Sx only has 5 tx fifo (5 endpoint IN)
  #define DWC2_FS_EP_FIFO_SIZE 1024
#endif

// OTG HS always has higher number of endpoints than FS
// Declare the DWC2_EP_MAX as a higher number, because dcd_dwc2.c has a xfer_status
// buffer, tied to the EP MAX value.
// IMPORTANT: Be careful during usage of two USB peripherals (HS and FS) at the same time with full EPs load.
#ifdef DWC2_HS_PERIPH_BASE
  #define DWC2_EP_MAX   DWC2_HS_EP_MAX
#else
  #define DWC2_EP_MAX   DWC2_FS_EP_MAX
#endif

// On ESP32 for consistency we associate
// - Port0 to OTG_FS, and Port1 to OTG_HS
static const dwc2_controller_t _dwc2_controller[] =
{
#ifdef DWC2_FS_PERIPH_BASE
  { .reg_base = DWC2_FS_PERIPH_BASE, .irqnum = DWC2_FS_INTR_SOURCE, .ep_count = DWC2_FS_EP_MAX, .ep_fifo_size = DWC2_FS_EP_FIFO_SIZE },
#endif

#ifdef DWC2_HS_PERIPH_BASE
  { .reg_base = DWC2_HS_PERIPH_BASE, .irqnum = DWC2_HS_INTR_SOURCE, .ep_count = DWC2_HS_EP_MAX, .ep_fifo_size = DWC2_HS_EP_FIFO_SIZE },
#endif
};

static intr_handle_t usb_ih[DWC2_PERIPH_COUNT];

static void dcd_int_handler_wrap(void* arg)
{
  unsigned int rhport = (unsigned int) arg;
  dcd_int_handler(rhport);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_enable (uint8_t rhport)
{
  unsigned int port = (unsigned int) rhport;
  esp_intr_alloc(_dwc2_controller[rhport].irqnum, ESP_INTR_FLAG_LOWMED, dcd_int_handler_wrap, (void *) port, &usb_ih[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline void dwc2_dcd_int_disable (uint8_t rhport)
{
  esp_intr_free(usb_ih[rhport]);
}

static inline void dwc2_remote_wakeup_delay(void)
{
  vTaskDelay(pdMS_TO_TICKS(1));
}

// MCU specific PHY init, called BEFORE core reset
static inline void dwc2_phy_init(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // nothing to do
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t * dwc2, uint8_t hs_phy_type)
{
  (void) dwc2;
  (void) hs_phy_type;

  // nothing to do
}

#ifdef __cplusplus
}
#endif

#endif /* _DWC2_ESP32_H_ */
