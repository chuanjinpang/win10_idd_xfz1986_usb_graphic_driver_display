#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

int usb_transf_init(SLIST_HEADER * urb_list);
int usb_transf_exit(SLIST_HEADER * urb_list);

	NTSTATUS
	idd_usbdisp_evt_device_prepareHardware(
		WDFDEVICE Device,
		WDFCMRESLIST ResourceList,
		WDFCMRESLIST ResourceListTranslated
		);

	int usb_transf_msg(SLIST_HEADER * urb_list ,WDFUSBPIPE pipeHandle,uint8_t *msg,int total_bytes);

#ifdef __cplusplus
}  // extern C
#endif
