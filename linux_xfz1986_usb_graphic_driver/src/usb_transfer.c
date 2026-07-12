/*
 *    xfz1986 usb graphic Display Linux Driver sync from win10 version
 *
 *    Copyright (C) 2019 - 2024 
 *    This file is licensed under the GPL. See LICENSE in the package.
 *    
 *    Author pcj
 *
 */
#include "inc/base_type.h"
#include "inc/usb_transfer.h"
#include "inc/drm_handlers.h"
#include "inc/udisp_config.h"
#include "inc/log.h"
#include "inc/enc_base.h"
#include "inc/enc_raw_rgb.h"
#include "inc/enc_jpg.h"

#include <drm/drm_modeset_helper.h>

#define DL_ALIGN_UP(x, a) ALIGN(x, a)
#define DL_ALIGN_DOWN(x, a) ALIGN(x-(a-1), a)

static const struct usb_device_id id_table[] = {
    {
        .idVendor    = UDISP_USB_VENDOR_ID,
        //.idProduct   = UDISP_USB_PRODUCT_ID,
        .idProduct   = 0x2986,
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
    },
        {
        .idVendor    = UDISP_USB_VENDOR_ID,
        .idProduct   = UDISP_USB_PRODUCT_ID,
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
    },
    { },
};
MODULE_DEVICE_TABLE(usb, id_table);

static atomic_t udisp_count = ATOMIC_INIT(0);


static void urb_write_completion_routine(struct urb *urb)
{
	LOGD("%s %d\n", __func__, __LINE__);
	purb_itm_t urb_itm = urb->context;
	struct xfz1986_drm_device *dev = urb_itm->dev;
	urb_mgr_t *mgr = &dev->urb_mgr;

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		LOGE("transmission failed for urb %p, error code %x\n",
		     urb, urb->status);
	}

LOG("%s  urb:%px %x\n",  __func__,urb,urb->status);

    // insert to the available queue
    {
        unsigned long  irq_flags;
        spin_lock_irqsave(&mgr->oplock, irq_flags);
        list_add_tail(&urb_itm->node, &mgr->urb_list);
        spin_unlock_irqrestore(&mgr->oplock, irq_flags);
    }

}


urb_itm_t *get_one_urb_item(struct xfz1986_drm_device *dev)
{
	LOGD("%s %d\n", __func__, __LINE__);
    unsigned long irq_flags;
     struct list_head *node;
     purb_itm_t purb=NULL;

	if (!dev->is_alive)
		return NULL;

    spin_lock_irqsave(&dev->urb_mgr.oplock, irq_flags);
	if (!list_empty(&dev->urb_mgr.urb_list)) {
		purb = list_first_entry(&dev->urb_mgr.urb_list,
					urb_itm_t, node);
       list_del(&purb->node);
    }
    spin_unlock_irqrestore(&dev->urb_mgr.oplock, irq_flags);
    return purb;
}

int usb_send_msg_async(struct xfz1986_drm_device *dev, urb_itm_t *urb,
		       const uint8_t *msg_data, int msg_size)
{
    LOG("%s %d\n",  __func__, msg_size);

	urb->urb->transfer_buffer_length = msg_size;
    if(usb_submit_urb(urb->urb, GFP_KERNEL)) 
	{
        // submit failure
        urb_write_completion_routine(urb->urb);
        LOGW("%s submit urb NG\n",__func__);
        return -1; 
    }
    LOG("%s  urb:%px\n",  __func__,urb->urb);

    return 0;
}


void decision_runtime_encoder(struct xfz1986_drm_device *dev)
{
	LOGD("%s %d\n", __func__, __LINE__);
	LOG("%s enc%d quality%d\n",__func__,dev->enc,dev->quality);
	dev->encoder=&dev->_encoder;
	switch(dev->enc){

	case UDISP_TYPE_RGB565:
		create_enc_rgb565(dev->encoder);
		break;
	case UDISP_TYPE_RGB888:
		create_enc_rgb888a(dev->encoder );
		break;
	case UDISP_TYPE_JPG:
		create_enc_jpg(dev->encoder,dev->quality);
		break;
    default:
		create_enc_jpg(dev->encoder , 4);
		break;
	}

}

u64 get_os_us(void)
{
	return ktime_get_ns() / 1000;
}


long get_fps(fps_mgr_t * mgr)
{	  
	LOGD("%s %d\n", __func__, __LINE__);
	 if(mgr->cur < FPS_STAT_MAX)//we ignore first loop and also ignore rollback case due to a long period
		 return mgr->last_fps;//if <0 ,please ignore it
	else {
	 int i=0;
	 long b=0;
		 long a = mgr->tb[(mgr->cur-1)%FPS_STAT_MAX];//cur
	 for(i=2;i<FPS_STAT_MAX;i++){
	 
		 b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
	 if((a-b) > 1000000)
		 break;
	 }
		 b = mgr->tb[(mgr->cur-i)%FPS_STAT_MAX]; //last
		 long fps = (a - b) / (i-1);
		 fps = (1000000*10 ) / fps;
		 mgr->last_fps = fps;
		 return fps;
	 }

}

void put_fps_data(fps_mgr_t* mgr,long t)
{
	static int init = 0;


	if (0 == init) {
		mgr->cur = 0;
		mgr->last_fps = -1;
		init = 1;
	}

	mgr->tb[mgr->cur%FPS_STAT_MAX] = t;
	mgr->cur++;//cur ptr to next

}




int xfz1986_udisp_usb_try_send_jpeg_image(struct xfz1986_drm_device *dev,
					  const pixel_type_t *framebuffer,
					  int x, int y, int right, int bottom)
{
	LOGD("%s %d\n", __func__, __LINE__);
    int total_bytes;
    int ret=0;
	long a, b, c, d;
	urb_itm_t *purb;
	static u64 last_tick;
	u64 now;

	a = get_os_us();

#if 1
	now = get_os_us();
	if ((now - last_tick) < 10 * 1000) { /* ~10ms min interval */
		LOGD("%s skip, diff:%llu\n", __func__, now - last_tick);
		return -1;
	}
	last_tick = now;
#endif
	purb = get_one_urb_item(dev);
    if(NULL != purb) {
		LOGD("issue urb id:%d (%d %d %d %d)\n",
		     purb->id, x, y, right, bottom);
		b = get_os_us();
		total_bytes = dev->encoder->enc(
			dev->encoder,
			&purb->urb->transfer_buffer[sizeof(udisp_frame_header_t)],
			(uint8_t *)framebuffer,
			0, 0, right, bottom, right + 1);
		c = get_os_us();
		dev->encoder->enc_header(
			dev->encoder,
			purb->urb->transfer_buffer,
			0, 0, right, bottom, total_bytes);
        total_bytes+=sizeof(udisp_frame_header_t);
		ret = usb_send_msg_async(dev, purb,
					 purb->urb->transfer_buffer,
					 total_bytes);
    
		d = get_os_us();
		put_fps_data(&dev->fps_mgr, d);
		LOGM("%d fps:%d e%ld s%ld %ld\n",
		     total_bytes, ((int)get_fps(&dev->fps_mgr)) / 10,
		     c - b, d - c, d - a);
    
    } else {
        LOGM("%s no urb item so drop\n",__func__);
		ret=-1;
    }

    return ret;
}

int urb_mgr_init(urb_mgr_t *mgr, struct xfz1986_drm_device *dev)
{
    LOGD("%s %d\n", __func__, __LINE__);
    int i = 0;
    purb_itm_t purb;
     _u8   * transfer_buffer;

	LOG("%s init urb list\n",__func__);
    spin_lock_init(&mgr->oplock);
    INIT_LIST_HEAD(&mgr->urb_list);
    // Insert into the list.
#define MAX_URB_SIZE 3  //1 for encoder, 2 for usb transfer with pingpong mode.
    for(i = 1; i <= MAX_URB_SIZE; i++) {
        purb = (urb_itm_t *)kmalloc(sizeof(urb_itm_t), GFP_KERNEL);
        if(NULL == purb) {
            LOGE("%s Memory allocation failed.\n",__func__);
            break;
        }

        purb->urb = usb_alloc_urb(0, GFP_KERNEL);
        if(!purb->urb) {
            kfree(purb);
            break;
        }

		transfer_buffer = usb_alloc_coherent(
			dev->udev, DISP_MAX_TRANSFER_SIZE,
			GFP_KERNEL, &purb->urb->transfer_dma);
        if(!transfer_buffer) {
            usb_free_urb(purb->urb);
            kfree(purb);
            break;
        }

        // setup urb
		usb_fill_bulk_urb(purb->urb, dev->udev,
				  usb_sndbulkpipe(dev->udev,
						  dev->disp_out_ep_addr),
				  transfer_buffer, DISP_MAX_TRANSFER_SIZE,
				  urb_write_completion_routine, purb);
        purb->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

        purb->id = i;
		purb->dev=dev;
      
        list_add_tail(&purb->node, &mgr->urb_list);

		LOGD("create urb item %d\n",purb->id);
       
    }
	return 0;
}

#define  USB_INFO_STR_SIZE  16

typedef struct {
char str[USB_INFO_STR_SIZE];
} item_t;

int split_config_str(char *str, item_t *cfg, int max)
{
	LOGD("%s %d\n", __func__, __LINE__);
	int cnt=0;
	int i=0,j=0;
	redo:
		for(j=0;str[i]!=0;i++){
			if(str[i] !='_' && str[i] !=0){
				cfg[cnt].str[j++]=str[i];
				LOGD("[%d]%d %c %s\n",cnt,i,str[i],cfg[cnt].str);
			}else{
				LOGD("[%d]%s\n",cnt,cfg[cnt].str);
				cnt++;
				if( cnt > max)
					break;
				i++;//skip 				
				goto redo;
				}
		}
		if(j)
			cnt++;
		return cnt;

}

#define CFG_MAX 10
void parse_usb_dev_info(char *str, int *reg, int *w, int *h,
			int *enc, int *quality, int *fps)
{
	LOGD("%s %d\n", __func__, __LINE__);
	item_t cfg[CFG_MAX] = { { 0 } };
int cnt=0,t,i;
int enc_cfged=0;
	cnt=split_config_str(str,cfg,CFG_MAX);
		
	//cnt = sscanf(str,"%[^_]_%[^_]_%[^_]_%[^_]",type_str,whstr,enc_str,fps_str);
	if(cnt<2){
		LOGW("%s [%d] dev info string violation, using defaults\n",
		     str, cnt);
	}
	

	int len=strlen(cfg[0].str);
	t=sscanf(&cfg[0].str[len-1],"%d",reg);
	if(t ==1){
		LOGI("udisp reg idx:%d\n",*reg);
	}else {
		*reg=0;
		LOGW("default udisp reg idx:%d\n",*reg);
		
	}
	//set default
	*w=0;
	*h=0;
	*fps=60;
	*enc=UDISP_TYPE_JPG;
	*quality=5;

	for(i=1;i<cnt;i++){
		char *s = cfg[i].str;
		LOGD("%d %s\n", i, s);
		switch (s[0]) {
		case 'R':
			t = sscanf(s, "R%dx%d", w, h);
			if(t == 2){
				LOGI("udisp w%d h%d\n",*w, *h);
			}
			break;
		case 'F':
			t = sscanf(s, "Fps%d", fps);
		    if (t == 1) {
		        LOGI("udisp fps%d\n", *fps);
		    }
			break;
		case 'E':
			if(enc_cfged) //first is high prio
				break;
			switch (s[1]) {
			case 'j':
				t = sscanf(s, "Ejpg%d", quality);
				if(t==1){
					*enc=UDISP_TYPE_JPG;
					enc_cfged++;
					LOGI("enc:%d quality:%d\n",
					     *enc, *quality);
				}
				break;
			case 'r':
				t = sscanf(s, "Ergb%d", quality);
				if(t==1){
					if(*quality ==16 ){
						*enc=UDISP_TYPE_RGB565;
						enc_cfged++;
					} else if (*quality == 32) {
						*enc=UDISP_TYPE_RGB888;
						enc_cfged++;
					}
					LOGI("enc:%d quality:%d\n",
					     *enc, *quality);
				}else {
					*enc=UDISP_TYPE_RGB888;
					LOGE("wrong Enc str %s\n", s);
			
				}
				break;
			default:
				*enc=UDISP_TYPE_JPG;
				*quality=5;
				LOGW("default enc:%d quality:%d\n",
				     *enc, *quality);
			}
			break;
		default:
			;

		}
	}

}


/* ------------------------------------------------------------------ */
/* USB Probe / Disconnect                                              */

static int xfz1986_udisp_probe(struct usb_interface *interface,
			       const struct usb_device_id *id)
{
	LOGI("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev = NULL;
	struct drm_device *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	int retval = -ENOMEM;

	printk("in %s sizeof(*xdev)=%zu\n", __func__, sizeof(*xdev));

	/*
	 * Allocate + initialize DRM device in one call (5.15+ API).
	 * struct xfz1986_drm_device MUST have drm_device as first member.
	 */
	xdev = devm_drm_dev_alloc(&interface->dev, &xfz1986_drm_driver,
				  struct xfz1986_drm_device, dev);
	if (IS_ERR(xdev)) {
		retval = PTR_ERR(xdev);
		dev_err(&interface->dev,
			"drm_dev_alloc failed: %d\n", retval);
		return retval;
	}
	dev = &xdev->dev;

	xdev->udev = usb_get_dev(interface_to_usbdev(interface));
	xdev->interface = interface;

	printk("%s prod:%s\n", __func__, xdev->udev->product);
	if (le16_to_cpu(xdev->udev->descriptor.bcdDevice) > 0x0200) {
		dev_warn(&interface->dev,
			 "device may require a newer driver version\n");
	}

	/* Get DMA device for prime buffer sharing */
	xdev->dmadev = usb_intf_get_dma_device(interface);
	if (!xdev->dmadev)
		printk("buffer sharing not supported\n"); /* not an error */

	/* check for endpoints */
	iface_desc = interface->cur_altsetting;
	printk("%s ifstr:%s\n", __func__, iface_desc->string);

	/* Note: udisp_dev_info may not be null-terminated in the descriptor.
	 * Copy product string safely.
	 */
	//strncpy(xdev->udisp_dev_info, xdev->udev->product,
	strncpy(xdev->udisp_dev_info, iface_desc->string,
		sizeof(xdev->udisp_dev_info) - 1);

	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!xdev->disp_out_ep_addr &&
		    usb_endpoint_is_bulk_out(endpoint) &&
		    endpoint->wMaxPacketSize) {
			LOGI("%s endpoint for video output found\n", __func__);
			xdev->disp_out_ep_addr = endpoint->bEndpointAddress;
			xdev->disp_out_ep_max_size =
				le16_to_cpu(endpoint->wMaxPacketSize);
		}
	}

	if (!(xdev->disp_out_ep_addr)) {
		LOGE("%s Could not find the required endpoints\n", __func__);
		retval = -ENODEV;
		goto err_put_dev;
	}

	/* Initialize mutex */
	mutex_init(&xdev->op_locker);

	/* Initialize URB manager */
	if (urb_mgr_init(&xdev->urb_mgr, xdev) < 0) {
		dev_info(&interface->dev, "Cannot create urb mgr\n");
		retval = -ENOMEM;
		goto err_put_dev;
	}

	/* Parse USB device info string for resolution, encoder, etc. */
	parse_usb_dev_info(xdev->udisp_dev_info, &xdev->reg,
			   &xdev->w, &xdev->h,
			   &xdev->enc, &xdev->quality, &xdev->fps);
	LOGI("w%d h%d enc%d quality%d fps:%d\n",
	     xdev->w, xdev->h, xdev->enc, xdev->quality, xdev->fps);

	/* Fallback to defaults if parsing didn't provide w/h */
	if (xdev->w == 0)
		xdev->w = UDISP_DEFAULT_WIDTH;
	if (xdev->h == 0)
		xdev->h = UDISP_DEFAULT_HEIGHT;

	/* Select encoder based on device config */
	decision_runtime_encoder(xdev);

	atomic_inc(&udisp_count);
	xdev->dev_id = atomic_read(&udisp_count);
	xdev->is_alive = 1;

	/* Save data pointer in interface */
	usb_set_intfdata(interface, xdev);

	/* Initialize and register DRM device */
	retval = xfz1986_drm_device_init(xdev);
	if (retval) {
		dev_err(&interface->dev,
			"DRM device init failed: %d\n", retval);
		goto err_put_dev;
	}

	return 0;

err_put_dev:
	put_device(xdev->dmadev);
	xdev->dmadev = NULL;
	usb_put_dev(xdev->udev);
	return retval;
}


static int xfz1986_udisp_suspend(struct usb_interface *intf,
				 pm_message_t message)
{
	LOGI("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev = usb_get_intfdata(intf);

	if (!xdev)
		return 0;

	return drm_mode_config_helper_suspend(&xdev->dev);
}


static int xfz1986_udisp_resume(struct usb_interface *intf)
{
	LOGI("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev = usb_get_intfdata(intf);

	if (!xdev)
		return 0;

	return drm_mode_config_helper_resume(&xdev->dev);
}


static void xfz1986_udisp_disconnect(struct usb_interface *interface)
{
	LOGI("%s %d\n", __func__, __LINE__);
	struct xfz1986_drm_device *xdev;

	xdev = usb_get_intfdata(interface);
	if (!xdev)
		return;

	xdev->is_alive = 0;

	xfz1986_drm_device_fini(xdev);

	put_device(xdev->dmadev);
	xdev->dmadev = NULL;

	usb_set_intfdata(interface, NULL);

	usb_put_dev(xdev->udev);

	atomic_dec(&udisp_count);

	dev_info(&interface->dev, "udisp (#%d) now disconnected\n",
		 xdev->dev_id);
}


static struct usb_driver usbdisp_driver = {
    .name       =  "xfz1986_udisp",
    .probe      =  xfz1986_udisp_probe,
    .disconnect =  xfz1986_udisp_disconnect,
    .suspend    =  xfz1986_udisp_suspend,
    .resume     =  xfz1986_udisp_resume,
    .id_table   =  id_table,
    .supports_autosuspend = 0,
};

int __init usb_transfer_init(void)
{
    return usb_register(&usbdisp_driver);
}

void usb_transfer_exit(void)
{
    usb_deregister(&usbdisp_driver);
}

