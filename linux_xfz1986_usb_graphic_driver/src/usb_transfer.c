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
#include "inc/fbhandlers.h"
#include "inc/udisp_config.h"
#include "inc/log.h"
#include "inc/enc_base.h"
#include "inc/enc_raw_rgb.h"
#include "inc/enc_jpg.h"


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

static atomic_t udisp_count = ATOMIC_INIT(0);


static void urb_write_completion_routine(struct urb *urb)
{
    purb_itm_t urb_itm=urb->context;
    struct xfz1986_udisp_dev   * dev = urb_itm->dev;
    urb_mgr_t * mgr=&dev->urb_mgr;
    /* sync/async unlink faults aren't errors */
    if(urb->status) {
        LOGE("transmission failed for urb %p, error code %x\n", urb, urb->status);
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


static purb_itm_t get_one_urb_item(struct xfz1986_udisp_dev * dev)
{
    unsigned long irq_flags;
     struct list_head *node;
     purb_itm_t purb=NULL;

    if(!dev->is_alive) return 0;

    spin_lock_irqsave(&dev->urb_mgr.oplock, irq_flags);
    if(!list_empty(&dev->urb_mgr.urb_list))
    {
       purb =list_first_entry(&dev->urb_mgr.urb_list,urb_itm_t,node);	      
       list_del(&purb->node);
    }
    spin_unlock_irqrestore(&dev->urb_mgr.oplock, irq_flags);
    return purb;
}

int usb_send_msg_async(struct xfz1986_udisp_dev * dev,urb_itm_t * urb, const uint8_t * msg_data, int msg_size)
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


void decision_runtime_encoder(struct xfz1986_udisp_dev * dev){

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

u64 get_os_us(void);


long get_system_us(void)
{
    return get_os_us();
}


long get_fps(fps_mgr_t * mgr)
{	  
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




int xfz1986_udisp_usb_try_send_jpeg_image(struct xfz1986_udisp_dev * dev, const pixel_type_t * framebuffer, int x, int y, int right, int bottom)
{
    int total_bytes;
    int ret=0;
    long a,b,c,d,t;
    urb_itm_t* purb = get_one_urb_item(dev);
    if(NULL != purb) {
        LOGD("issue urb id:%d (%d %d %d %d)\n",purb->id,x,y,right,bottom);
        total_bytes = dev->encoder->enc(dev->encoder,&purb->urb->transfer_buffer[sizeof(udisp_frame_header_t)],(uint8_t *)framebuffer,0, 0, right, bottom, right+1);
        c=get_system_us();						
        dev->encoder->enc_header(dev->encoder,purb->urb->transfer_buffer,0,0, right, bottom, total_bytes);
        total_bytes+=sizeof(udisp_frame_header_t);
        ret = usb_send_msg_async(dev,purb, purb->urb->transfer_buffer, total_bytes);
    
        d=get_system_us();
        put_fps_data(&dev->fps_mgr,get_system_us());
        LOGM("%d fps:%d g%ld e%ld s%ld %ld (%ld)\n", total_bytes+sizeof(udisp_frame_header_t), ((int)get_fps(&dev->fps_mgr))/10,b-a,c-b,d-c,d-a,a-t);
    
    } else {
        LOGM("%s no urb item so drop\n",__func__);
		ret=-1;
    }

    return ret;
}

int  urb_mgr_init(urb_mgr_t * mgr,struct xfz1986_udisp_dev * dev){
    int i = 0;
    purb_itm_t purb;
	int status;
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

        transfer_buffer = usb_alloc_coherent(dev->udev, DISP_MAX_TRANSFER_SIZE, GFP_KERNEL, &purb->urb->transfer_dma);
        if(!transfer_buffer) {
            usb_free_urb(purb->urb);
            kfree(purb);
            break;
        }

        // setup urb
        usb_fill_bulk_urb(purb->urb, dev->udev, usb_sndbulkpipe(dev->udev, dev->disp_out_ep_addr),
                          transfer_buffer, DISP_MAX_TRANSFER_SIZE, urb_write_completion_routine, purb);
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

int  split_config_str(char * str, item_t * cfg ,int max){

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
void parse_usb_dev_info(char * str,int * reg,int * w, int * h, int * enc, int * quality, int * fps){
item_t cfg[CFG_MAX] = { 0 };
int cnt=0,t,i;
int enc_cfged=0;
	cnt=split_config_str(str,cfg,CFG_MAX);
		
	//cnt = sscanf(str,"%[^_]_%[^_]_%[^_]_%[^_]",type_str,whstr,enc_str,fps_str);
	if(cnt<2){
	LOGW("%s [%d] dev info string violation xfz1986 udisp SPEC, so use default\n",str,cnt);
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
		char * str=cfg[i].str;
		LOGD("%d %s\n",i,str);
		switch(str[0])
		{
		case 'R':
			t=sscanf(str,"R%dx%d",w,h);
			if(t == 2){
				LOGI("udisp w%d h%d\n",*w, *h);
			}
			break;
		case 'F':
			t = sscanf(str, "Fps%d", fps);
		    if (t == 1) {
		        LOGI("udisp fps%d\n", *fps);
		    }
			break;
		case 'E':
			if(enc_cfged) //first is high prio
				break;
			switch(str[1]){
			case 'j':
				t=sscanf(str,"Ejpg%d",quality);
				if(t==1){
					*enc=UDISP_TYPE_JPG;
					enc_cfged++;
					LOGI("enc:%d quality:%d\n",*enc,*quality);
				}
				break;
			case 'r':
				t=sscanf(str,"Ergb%d",quality);
				if(t==1){
					if(*quality ==16 ){
						*enc=UDISP_TYPE_RGB565;
						enc_cfged++;
						LOGI("enc:%d quality:%d\n",*enc,*quality);
					}else if (*quality == 32)
						{
						*enc=UDISP_TYPE_RGB888;
						enc_cfged++;
						LOGI("enc:%d quality:%d\n",*enc,*quality);
					}
				}else {
					*enc=UDISP_TYPE_RGB888;
					LOGE("wrong Enc str %s",str);
			
				}
				break;
			default:
				*enc=UDISP_TYPE_JPG;
				*quality=5;
				LOGW("default enc:%d quality:%d\n",*enc,*quality);
			}
			break;
		default:
			;

		}
	}

}


static int _on_new_usb_device(struct xfz1986_udisp_dev * dev)
{
    mutex_init(&dev->op_locker);


  
    if(urb_mgr_init(&dev->urb_mgr,dev) < 0) {
        dev_info(&dev->interface->dev, "Cannot create urb mgr\n");
        goto urb_mgr_ceate_fail;
    }

	
	parse_usb_dev_info(dev->udisp_dev_info,&dev->reg,&dev->w,&dev->h,&dev->enc,&dev->quality,&dev->fps);
	LOGI("w%d h%d enc%d quaility%d fps:%d",dev->w,dev->h,dev->enc,dev->quality,dev->fps);

	decision_runtime_encoder(dev);
	
	atomic_inc(&udisp_count);
     dev->dev_id =  atomic_read(&udisp_count);
    dev->is_alive = 1;

	register_fb_handlers(dev);
    

    return 0;

urb_mgr_ceate_fail:
    return -ENOMEM;
}


static void _on_del_usb_device(struct xfz1986_udisp_dev * dev)
{

    mutex_lock(&dev->op_locker);
    dev->is_alive = 0;
    mutex_unlock(&dev->op_locker);

	unregister_fb_handlers(dev);

    // kill all pending urbs


	atomic_dec(&udisp_count);

    dev_info(&dev->interface->dev, "udisp (#%d) now disconnected\n", dev->dev_id);
}

static int xfz1986_udisp_probe(struct usb_interface *interface, const struct usb_device_id *id)
{

    struct xfz1986_udisp_dev *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    size_t buffer_size;
    int i;
    int retval = -ENOMEM;
    printk("in %s %d\n", __FUNCTION__,sizeof(*dev));
    /* allocate memory for our device state and initialize it */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if(dev == NULL) {
        LOGE("%s Out of memory\n",__func__);
        goto error;
    }

    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;

    printk("%s prod:%s\n", __FUNCTION__,dev->udev->product);
    if(le16_to_cpu(dev->udev->descriptor.bcdDevice) > 0x0200) {
        dev_warn(&interface->dev, "The device you used may requires a newer driver version to work.\n");
    }

    // check for endpoints
    iface_desc = interface->cur_altsetting;

    printk("%s ifstr:%s\n", __FUNCTION__,iface_desc->string);
	//strcpy(dev->udisp_dev_info,iface_desc->string);
	strcpy(dev->udisp_dev_info,dev->udev->product);
	
    for(i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;


        if(!dev->disp_out_ep_addr &&
           usb_endpoint_is_bulk_out(endpoint) && endpoint->wMaxPacketSize) {
            LOGI("%s endpoint for video output has been found\n",__func__);
            dev->disp_out_ep_addr = endpoint->bEndpointAddress;
            dev->disp_out_ep_max_size = le16_to_cpu(endpoint->wMaxPacketSize);

        }
    }

    if(!(dev->disp_out_ep_addr)) {
        LOGE("%s Could not find the required endpoints\n",__func__);
        goto error;
    }


    /* save our data pointer in this interface device */
    usb_set_intfdata(interface, dev);

    // add the device to the list
    if(_on_new_usb_device(dev)) {
        goto error;
    }
    return 0;

error:
    if(dev) {
        kfree(dev);
    }
    return retval;
}




static int xfz1986_udisp_suspend(struct usb_interface *intf, pm_message_t message)
{
    struct usb_lcd *dev = usb_get_intfdata(intf);

    if(!dev)
        return 0;
    // not implemented yet
    return 0;
}


static int xfz1986_udisp_resume(struct usb_interface *intf)
{
    // not implemented yet
    return 0;
}

static void xfz1986_udisp_disconnect(struct usb_interface *interface)
{
    struct xfz1986_udisp_dev *dev;

    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    _on_del_usb_device(dev);

    kfree(dev);
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

