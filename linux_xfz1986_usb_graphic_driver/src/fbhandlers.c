/*
 *    RoboPeak USB LCD Display Linux Driver
 *
 *    Copyright (C) 2009 - 2013 RoboPeak Team
 *    This file is licensed under the GPL. See LICENSE in the package.
 *
 *    http://www.robopeak.net
 *
 *    Author Shikai Chen
 *     @2025 add var fb size for support multi-dev such esp32s3/p4/allwinner t113
 *    Author xfz1986 rewrite some part code for sync windows udisp work logic
 *   ---------------------------------------------------
 *    Frame Buffer Handlers
 */

#include "inc/base_type.h"
#include "inc/usb_transfer.h"
#include "inc/fbhandlers.h"
#include "inc/log.h"
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include "inc/udisp_config.h"
#include "inc/usb_transfer.h"
#include <linux/time.h>


u64 get_os_us(void)
{
#if 0
    struct timespec ts;
    ts = current_kernel_time();
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
	#else
	return ktime_get_ns()/1000;

	#endif
}


enum {
    DISPLAY_UPDATE_HINT_NONE       = 0,
    DISPLAY_UPDATE_HINT_BITBLT     = 1,
    DISPLAY_UPDATE_HINT_FILLRECT   = 2,
    DISPLAY_UPDATE_HINT_COPYAREA   = 3,

};


struct rpusbdisp_fb_private {
    _u32 pseudo_palette [16];
    struct mutex      operation_lock;
    struct xfz1986_udisp_dev * binded_usbdev;
//lock free area
    atomic_t               unsync_flag;
};



static struct fb_fix_screeninfo _vfb_fix /*__devinitdata*/ = {
    .id =	    "xfz1986-udisp-fb",
    .type =		FB_TYPE_PACKED_PIXELS,
    .visual =	FB_VISUAL_TRUECOLOR,
    .accel =	FB_ACCEL_NONE,
    .line_length = UDISP_DEFAULT_WIDTH * 32 / 8,

};


static  struct fb_var_screeninfo _var_info /*__devinitdata*/ = {
    .xres = UDISP_DEFAULT_WIDTH,
    .yres = UDISP_DEFAULT_HEIGHT,
    .xres_virtual = UDISP_DEFAULT_WIDTH,
    .yres_virtual = UDISP_DEFAULT_HEIGHT,
    .width = UDISP_DEFAULT_WIDTH,
    .height = UDISP_DEFAULT_HEIGHT,

    .bits_per_pixel = 32 ,
    .red = { 0 , 8 , 0 } ,
    .green = { 8 , 8 , 0 } ,
    .blue = { 16 , 8 , 0 } ,

    .activate = FB_ACTIVATE_NOW,
    .vmode = FB_VMODE_NONINTERLACED,
};

static DEFINE_MUTEX(_mutex_devreg);

static inline struct rpusbdisp_fb_private * _get_fb_private(struct fb_info * info) {
    return (struct rpusbdisp_fb_private *)info->par;
}


static void _reset_fb_private(struct rpusbdisp_fb_private * pa)
{
    mutex_init(&pa->operation_lock);
    pa->binded_usbdev = NULL;
    atomic_set(&pa->unsync_flag, 1);
}


void fill_color_bar(pixel_type_t *buf, int len);

#if 1

void scale_for_sample_2to1(uint32_t * dst, uint32_t * src, int w, int h)
{
	int i = 0, j = 0;
	int line = w;
	int len=w*h;

	for (i = 0; i < len / 4; i++) {
		*dst++ = *src++;
		src++;
		j += 2;
		if (j >= line) {
			j = 0;
			src += line;
		}
	}

}

#endif


#define MIN_DISP_INTERVAL_MS 10
static  void _display_update(struct fb_info *p, int x, int y, int width, int height, int hint, const void * hint_data)
{


    struct rpusbdisp_fb_private * pa = _get_fb_private(p);

    int clear_dirty = 0;
    static u64 last_tick=0;
    mutex_lock(&pa->operation_lock);

    if(!pa->binded_usbdev) goto final;
    // 1. update the dirty rect

    if(atomic_dec_and_test(&pa->unsync_flag)) {
           clear_dirty = 1;
    } 


#if 1
 if(( get_os_us()- last_tick) <  MIN_DISP_INTERVAL_MS*1000 ){ //we do a Op  after a interleave 

	LOGD("%s diff:%llu %llu %llu\n",__func__,get_os_us()-last_tick,get_os_us(),last_tick);
	goto final;
}
#endif
	LOGD("%s do diff:%llu %llu %llu\n",__func__,get_os_us()-last_tick,get_os_us(),last_tick);
	last_tick=get_os_us();

    // 2. try to send it
    if(pa->binded_usbdev) {

        switch(hint) {
        case DISPLAY_UPDATE_HINT_FILLRECT: 
        case DISPLAY_UPDATE_HINT_COPYAREA: //device can't support fillrect & copyarea, so just use system api do it, then issue fb data to dev.
        default:
        //	fill_color_bar((pixel_type_t *)p->fix.smem_start,480*100);
        	#if 1
                if(pa->binded_usbdev->w*2 == p->var.width)
                {
					LOGD("scale %d to %d\n",p->var.width,pa->binded_usbdev->w);
                    scale_for_sample_2to1((uint32_t *)p->fix.smem_start, (uint32_t *)p->fix.smem_start, p->var.width, p->var.height);
                   
                } 
			#endif	
            if(xfz1986_udisp_usb_try_send_jpeg_image(pa->binded_usbdev, (const pixel_type_t *)p->fix.smem_start,
                                            0, 0, pa->binded_usbdev->w-1, pa->binded_usbdev->h-1) ){
                // data sent, rect the dirty rect
                ;

            }
        }
    }
final:
    mutex_unlock(&pa->operation_lock);


}


static  void _display_fillrect(struct fb_info * p, const  struct fb_fillrect * rect)
{
#ifdef SYS_FB_SYMBOL
    sys_fillrect(p, rect);
#endif
	LOGM("%s \n", __FUNCTION__);
    _display_update(p, rect->dx, rect->dy, rect->width, rect->height, DISPLAY_UPDATE_HINT_FILLRECT, rect);
}

static  void _display_imageblit(struct fb_info * p, const  struct fb_image * image)
{
#ifdef SYS_FB_SYMBOL

    sys_imageblit(p, image);
#endif
	LOGM("%s \n", __FUNCTION__);
    _display_update(p, image->dx, image->dy, image->width, image->height, DISPLAY_UPDATE_HINT_BITBLT, image);
}

static  void _display_copyarea(struct fb_info * p, const  struct fb_copyarea * area)
{
#ifdef SYS_FB_SYMBOL

    sys_copyarea(p, area);
#endif
	LOGM("%s \n", __FUNCTION__);
    _display_update(p, area->dx, area->dy, area->width, area->height, DISPLAY_UPDATE_HINT_COPYAREA, area);
}


#include "inc/tiny_jpeg.h"


#if 1
ssize_t my_fb_sys_read(struct fb_info *info, char __user *buf, size_t count,
                       loff_t *ppos)
{
    unsigned long p = *ppos;
    void *src;
    int err = 0;
    unsigned long total_size;

    if(info->state != FBINFO_STATE_RUNNING)
        return -EPERM;

    total_size = info->screen_size;

    if(total_size == 0)
        total_size = info->fix.smem_len;
    if(p >= total_size)
        return 0;
    if(count >= total_size)
        count = total_size;
    if(count + p > total_size)
        count = total_size - p;
    src = (void __force *)(info->screen_base + p);
    if(info->fbops->fb_sync)
        info->fbops->fb_sync(info);
    if(copy_to_user(buf, src, count))
        err = -EFAULT;
    if(!err)
        *ppos += count;
    return (err) ? err : count;
}
ssize_t my_fb_sys_write(struct fb_info *info, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    unsigned long p = *ppos;
    void *dst;
    int err = 0;
    unsigned long total_size;
    if(info->state != FBINFO_STATE_RUNNING)
        return -EPERM;
    total_size = info->screen_size;
    if(total_size == 0)
        total_size = info->fix.smem_len;
    if(p > total_size)
        return -EFBIG;
    if(count > total_size) {
        err = -EFBIG;
        count = total_size;
    }
    if(count + p > total_size) {
        if(!err)
            err = -ENOSPC;
        count = total_size - p;
    }
    dst = (void __force *)(info->screen_base + p);
    if(info->fbops->fb_sync)
        info->fbops->fb_sync(info);
    if(copy_from_user(dst, buf, count))
        err = -EFAULT;
    if(!err)
        *ppos += count;
    return (err) ? err : count;
}
#endif
static ssize_t _display_write(struct fb_info * p, const  char * buf __user,
                              size_t count, loff_t * ppos)
{
    int retval = 0;
LOGD("%s\n", __FUNCTION__);
#ifdef SYS_FB_SYMBOL
    retval = fb_sys_write(p, buf, count, ppos);
#else
    retval = my_fb_sys_write(p, buf, count, ppos);
#endif
    _display_update(p, 0, 0, p->var.width, p->var.height, DISPLAY_UPDATE_HINT_NONE, NULL);

    return retval;
}


static int _display_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                              u_int transp, struct fb_info *info)
{
#define CNVT_TOHW(val, width) ((((val) << (width)) +0x7FFF-(val)) >> 16)
    int ret = 1 ;
    if(info->var.grayscale)
        red = green = blue = (19595 * red + 38470 * green +
                              7471 * blue) >> 16 ;
    switch(info->fix.visual) {
    case FB_VISUAL_TRUECOLOR:
        if(regno < 16) {
            u32 * pal = info->pseudo_palette;
            u32 value;

            red = CNVT_TOHW(red, info->var.red.length);
            green = CNVT_TOHW(green, info->var.green.length);
            blue = CNVT_TOHW(blue, info->var.blue.length);
            transp = CNVT_TOHW(transp, info->var.transp.length);

            value = (red << info->var.red.offset) |
                    (green << info->var.green.offset) |
                    (blue << info->var.blue.offset) |
                    (transp << info->var.transp.offset);

            pal[regno] = value;
            ret = 0 ;
        }
        break ;
    case FB_VISUAL_STATIC_PSEUDOCOLOR:
    case FB_VISUAL_PSEUDOCOLOR:
        break ;
    }
    return ret;
}


static void _display_defio_handler(struct fb_info *info,
                                   struct list_head *pagelist)
{
    struct page *cur;
    struct fb_deferred_io *fbdefio = info->fbdefio;
    int top = UDISP_DEFAULT_HEIGHT, bottom = 0;
    int current_val;
    unsigned long offset;
    unsigned long page_start;

    struct rpusbdisp_fb_private * pa = _get_fb_private(info);
    if(!pa->binded_usbdev) return;  //simply ignore it

    list_for_each_entry(cur, &fbdefio->pagelist, lru) {

        // convert page range to dirty box
        page_start = (cur->index << PAGE_SHIFT);

        if(page_start < info->fix.mmio_start && page_start >= info->fix.mmio_start + info->fix.smem_len) {
            continue;
        }

        offset = (unsigned long)(page_start - info->fix.mmio_start);
        current_val = offset / info->fix.line_length;
        if(top > current_val) top = current_val;
        current_val = (offset + PAGE_SIZE + info->fix.line_length - 1) / info->fix.line_length;
        if(bottom < current_val) bottom = current_val;

    }
    if(bottom >= UDISP_DEFAULT_HEIGHT) bottom = UDISP_DEFAULT_HEIGHT - 1;

	LOGM("%s\n", __FUNCTION__);

    _display_update(info, 0, top, info->var.width, bottom - top + 1, DISPLAY_UPDATE_HINT_NONE, NULL);
}

static  struct fb_ops _display_fbops /*__devinitdata*/ = {
    .owner = THIS_MODULE,
#ifdef SYS_FB_SYMBOL
    .fb_read = fb_sys_read,
#else
    .fb_read = my_fb_sys_read,
#endif
    .fb_write =     _display_write,
    .fb_fillrect =  _display_fillrect,
    .fb_copyarea =  _display_copyarea,
    .fb_imageblit = _display_imageblit,
    .fb_setcolreg = _display_setcolreg,
};


static void *rvmalloc(unsigned long size)
{
    void *mem;
    unsigned long adr;

    size = PAGE_ALIGN(size);
    mem = vmalloc_32(size);
    if(!mem)
        return NULL;

    memset(mem, 0, size); /* Clear the ram out, no junk to the user */
    adr = (unsigned long) mem;
    while(size > 0) {
        SetPageReserved(vmalloc_to_page((void *)adr));
        adr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    return mem;
}

static void rvfree(void *mem, unsigned long size)
{
    unsigned long adr;

    if(!mem)
        return;

    adr = (unsigned long) mem;
    while((long) size > 0) {
        ClearPageReserved(vmalloc_to_page((void *)adr));
        adr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    vfree(mem);
}

extern int scale_thd;
static int _on_create_new_fb(struct fb_info ** out_fb, struct xfz1986_udisp_dev *dev)
{
    int ret = -ENOMEM;
    size_t fbmem_size = 0;
    void * fbmem = NULL;
    struct fb_info * fb;
    struct fb_deferred_io *fbdefio;
	int scale=1;
    *out_fb = NULL;

	if(dev->w <= scale_thd){
		scale=2;
		LOGI("%sdev->w:%d  scale_thd:%d %d\n",__func__,dev->w,scale_thd,scale);
	}

    fb = framebuffer_alloc(sizeof(struct rpusbdisp_fb_private), NULL/*rpusbdisp_usb_get_devicehandle(dev)*/);

    if(!fb) {
        LOGE("Failed to initialize framebuffer device\n");
        goto failed;
    }

    fb->fix = _vfb_fix;
	fb->fix.line_length=dev->w*4*scale;
    fb->var = _var_info;
	fb->var.xres=(__u32)dev->w*scale;
	fb->var.yres=dev->h*scale;
	fb->var.xres_virtual=dev->w*scale;
	fb->var.yres_virtual=dev->h*scale;	
	fb->var.width=dev->w*scale;
	fb->var.height=dev->h*scale;


    fb->fbops       = &_display_fbops;
    fb->flags       = FBINFO_DEFAULT | FBINFO_VIRTFB;

    fbmem_size = fb->var.yres * fb->fix.line_length; // Correct issue with size allocation (too big)
    fbmem =  rvmalloc(fbmem_size);
    if(!fbmem) {

        LOGE("Cannot allocate fb memory.\n");
        goto failed_nofb;
    }


    memset(fbmem, 0, fbmem_size);

    fb->screen_base = (char __iomem *)fbmem;
    fb->fix.smem_start = (unsigned long)fb->screen_base;
    fb->fix.smem_len = fbmem_size;

    fb->pseudo_palette = _get_fb_private(fb)->pseudo_palette;


    if(fb_alloc_cmap(&fb->cmap, 256, 0)) {
        LOGE("Cannot alloc the cmap.\n");

        goto failed_nocmap;

    }


    // Since kernel 3.5 the fb_deferred_io structure has a second callback (first_io) that must be set to a valid function or NULL.
    // To avoid unexpected crash due to a non initialized function pointer do a kzalloc rather than a kmalloc
    fbdefio = /*kmalloc*/kzalloc(sizeof(struct fb_deferred_io), GFP_KERNEL);

    if(fbdefio) {
        // frame rate is configurable through the fps option during the load operation
        fbdefio->delay = HZ / dev->fps;
        fbdefio->deferred_io = _display_defio_handler;
    } else {
        LOGE("Cannot alloc the fb_deferred_io.\n");

        goto failed_nodefio;

    }

    fb->fbdefio = fbdefio;
    fb_deferred_io_init(fb);


    _reset_fb_private(_get_fb_private(fb));

    // register the framebuffer device
    ret = register_framebuffer(fb);
    if(ret < 0) {
        LOGE("Cannot register the framebuffer device.\n");
        goto failed_on_reg;
    }
    *out_fb = fb;
    return ret;

failed_on_reg:
    kfree(fbdefio);
failed_nodefio:
    fb_dealloc_cmap(&fb->cmap);
failed_nocmap:
    rvfree(fbmem, fbmem_size);
failed_nofb:
    framebuffer_release(fb);
failed:
    return ret;
}

static void _on_release_fb(struct fb_info * fb)
{
    if(!fb) return;
	LOG("%s\n",__FUNCTION__);
    // del the defio
    fb_deferred_io_cleanup(fb);

    unregister_framebuffer(fb);
    kfree(fb->fbdefio);
    fb_dealloc_cmap(&fb->cmap);
    rvfree(fb->screen_base, fb->fix.smem_len);
    framebuffer_release(fb);


}


int  register_fb_handlers(struct xfz1986_udisp_dev * dev)
{
	struct fb_info * fb;
	struct rpusbdisp_fb_private * pa;
	int ret;
	LOG("%s\n",__FUNCTION__);
    ret= _on_create_new_fb(&fb, dev);
	pa = _get_fb_private(fb);
	pa->binded_usbdev=dev;
	dev->fb_info=fb;
	return ret;
}

void unregister_fb_handlers(struct xfz1986_udisp_dev * dev)
{
    _on_release_fb(dev->fb_info);
}






