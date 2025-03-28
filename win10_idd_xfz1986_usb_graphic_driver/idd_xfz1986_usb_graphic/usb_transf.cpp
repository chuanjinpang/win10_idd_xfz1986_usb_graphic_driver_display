

VOID EvtRequestWriteCompletionRoutine(
	_In_ WDFREQUEST                  Request,
	_In_ WDFIOTARGET                 Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
	_In_ WDFCONTEXT                  Context
	)
	/*++

	Routine Description:

	This is the completion routine for writes
	If the irp completes with success, we check if we
	need to recirculate this irp for another stage of
	transfer.

	Arguments:

	Context - Driver supplied context
	Device - Device handle
	Request - Request handle
	Params - request completion params

	Return Value:
	None

	--*/
{
	NTSTATUS    status;
	size_t      bytesWritten = 0;
	PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

	UNREFERENCED_PARAMETER(Target);
	urb_itm_t * urb = (urb_itm_t *)Context;
	// UNREFERENCED_PARAMETER(Context);

	status = CompletionParams->IoStatus.Status;

	//
	// For usb devices, we should look at the Usb.Completion param.
	//
	usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

	bytesWritten = usbCompletionParams->Parameters.PipeWrite.Length;

	if (NT_SUCCESS(status)) {
		;

	}
	else {
		LOGE("Write failed: request Status 0x%x UsbdStatus 0x%x\n",
			status, usbCompletionParams->UsbdStatus);
	}

	if (NULL != urb->wdfMemory) {
		//LOG("urb cpl del wdfMemory\n", urb->wdfMemory);
		WdfObjectDelete(urb->wdfMemory);
		urb->wdfMemory = NULL;
	}

	LOGD("pipe:%p cpl urb id:%d\n", urb->pipe, urb->id);
	InterlockedPushEntrySList(urb->urb_list,
		&(urb->node));

	return;
}


NTSTATUS usb_send_msg_async(urb_itm_t * urb, WDFUSBPIPE pipe, WDFREQUEST Request, PUCHAR msg, int tsize)
{
	WDF_MEMORY_DESCRIPTOR  writeBufDesc;
	WDFMEMORY  wdfMemory;
	ULONG  writeSize, bytesWritten;
	size_t  bufferSize;
	NTSTATUS status;
	PVOID  writeBuffer;

	writeSize = tsize;


	status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		0,
		writeSize,
		&wdfMemory,
		NULL
		);
	if (!NT_SUCCESS(status)) {
		LOGE("WdfMemoryCreate NG %x\n",status);
		return status;
	}

	WdfMemoryCopyFromBuffer(wdfMemory, 0, msg, writeSize);


	status = WdfUsbTargetPipeFormatRequestForWrite(
		pipe,
		Request,
		wdfMemory,
		NULL // Offsets
		);
	if (!NT_SUCCESS(status)) {
		LOGE("WdfUsbTargetPipeFormatRequestForWrite NG\n");
		goto Exit;
	}
	//
	// Set a CompletionRoutine callback function.
	//
	urb->pipe = pipe;
	urb->wdfMemory = wdfMemory;

	WdfRequestSetCompletionRoutine(
		Request,
		EvtRequestWriteCompletionRoutine,
		urb
		);
	//
	// Send the request. If an error occurs, complete the request.
	//
	if (WdfRequestSend(
		Request,
		WdfUsbTargetPipeGetIoTarget(pipe),
		WDF_NO_SEND_OPTIONS
		) == FALSE) {
		status = WdfRequestGetStatus(Request);
		LOGE("WdfRequestSend NG %x\n", status);
		goto Exit;
	}

Exit:


	return status;

}




NTSTATUS usb_send_msg(WDFUSBPIPE pipeHandle, WDFREQUEST Request, PUCHAR msg, int tsize)
{
	WDF_MEMORY_DESCRIPTOR  writeBufDesc;
	WDFMEMORY  wdfMemory;
	ULONG  writeSize, bytesWritten;
	size_t  bufferSize;
	NTSTATUS status;
	PVOID  writeBuffer;

	writeSize = tsize;
	status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		0,
		writeSize,
		&wdfMemory,
		NULL
		);
	if (!NT_SUCCESS(status)) {
		LOGE("WdfMemoryCreate NG\n");
		return status;
	}

	writeBuffer = WdfMemoryGetBuffer(
		wdfMemory,
		&bufferSize
		);



	//memcpy(writeBuffer,msg, writeSize);
	WdfMemoryCopyFromBuffer(wdfMemory, 0, msg, writeSize);


	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&writeBufDesc,
		writeBuffer,
		writeSize
		);

	status = WdfUsbTargetPipeWriteSynchronously(
		pipeHandle,
		NULL,
		NULL,
		&writeBufDesc,
		&bytesWritten
		);
	LOGD("WdfUsbTargetPipeWriteSynchronously %x\n", status);

	if (NULL != wdfMemory)
		WdfObjectDelete(wdfMemory);

	return status;

}

/*************usb part*******************/
NTSTATUS get_usb_dev_string_info(_In_ WDFDEVICE Device, TCHAR * stringBuf ) {

	NTSTATUS status, ntStatus;
	USHORT  numCharacters;
	
	WDFMEMORY  memoryHandle;
	USB_DEVICE_DESCRIPTOR udesc;
	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);

	WdfUsbTargetDeviceGetDeviceDescriptor(pDeviceContext->UsbDevice, &udesc);

	LOGI("udesc vid&pid:%x %x\n",udesc.idVendor,udesc.idProduct);

	status = WdfUsbTargetDeviceQueryString(
		pDeviceContext->UsbDevice,
		NULL,
		NULL,
		NULL,
		&numCharacters,
		udesc.iProduct,
		0x0409
		);
#if  0
	ntStatus = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		0,
		(numCharacters+1) * sizeof(WCHAR),
		&memoryHandle,
		(PVOID*)&stringBuf
		);
	if (!NT_SUCCESS(ntStatus)) {
		return ntStatus;
#endif
	status = WdfUsbTargetDeviceQueryString(
		pDeviceContext->UsbDevice,
		NULL,
		NULL,
		stringBuf,
		&numCharacters,
		udesc.iProduct,
		0x0409
		);
	stringBuf[numCharacters] = '\0';
	LOGI("product %d %S\n", numCharacters, stringBuf);
	return status;
}

#if 1
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
	_In_ WDFDEVICE Device
	)
	/*++

	Routine Description:

	This helper routine selects the configuration, interface and
	creates a context for every pipe (end point) in that interface.

	Arguments:

	Device - Handle to a framework device

	Return Value:

	NT status value

	--*/
{
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
	NTSTATUS                            status = STATUS_SUCCESS;
	//PDEVICE_CONTEXT                     pDeviceContext;
	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
	WDFUSBPIPE                          pipe;
	WDF_USB_PIPE_INFORMATION            pipeInfo;
	UCHAR                               index;
	UCHAR                               numberConfiguredPipes;
	WDFUSBINTERFACE                     usbInterface;

	PAGED_CODE();
	

	//pDeviceContext = GetDeviceContext(Device);

	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

	usbInterface =
		WdfUsbTargetDeviceGetInterface(pDeviceContext->UsbDevice, 0);

	if (NULL == usbInterface) {
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	configParams.Types.SingleInterface.ConfiguredUsbInterface =
		usbInterface;

	configParams.Types.SingleInterface.NumberConfiguredPipes =
		WdfUsbInterfaceGetNumConfiguredPipes(usbInterface);

	pDeviceContext->UsbInterface =
		configParams.Types.SingleInterface.ConfiguredUsbInterface;

	numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

	//
	// Get pipe handles
	//
	for (index = 0; index < numberConfiguredPipes; index++) {

		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		pipe = WdfUsbInterfaceGetConfiguredPipe(
			pDeviceContext->UsbInterface,
			index, //PipeIndex,
			&pipeInfo
			);
		//
		// Tell the framework that it's okay to read less than
		// MaximumPacketSize
		//
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);



		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsInEndpoint(pipe)) {

			pDeviceContext->BulkReadPipe = pipe;
			pDeviceContext->max_in_pkg_size = pipeInfo.MaximumPacketSize;
			LOGI("BulkInput Pipe is 0x%p ep_max:%d\n", pipe, pDeviceContext->max_in_pkg_size);
		}

		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsOutEndpoint(pipe)) {
			
			pDeviceContext->BulkWritePipe = pipe;
			pDeviceContext->max_out_pkg_size = pipeInfo.MaximumPacketSize;
			LOGI("BulkOutput Pipe is 0x%p ep_max:%d\n", pipe, pDeviceContext->max_out_pkg_size);
		}

	}

	//
	// If we didn't find all the 2 pipes, fail the start.
	//
	if (!(pDeviceContext->BulkWritePipe
		&& pDeviceContext->BulkReadPipe)) {
		status = STATUS_INVALID_DEVICE_STATE;
		LOGW("Device is not configured properly %x\n",
			status);

		return status;
	}

	return status;
}

#endif

VOID registry_config_dev_info(WDFDEVICE Device)
{
    LOGD(("<---ReadRegistryInformation"));
		TCHAR	tchar_udisp_registry[UDISP_CONFIG_STR_LEN];

        HKEY hOpenKey = NULL;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,                     // handle to open key
                         TEXT("SYSTEM\\CurrentControlset\\Services\\xfz1986_usb_graphic\\Parameters"),       // address of name of subkey to open
                         0,                        // options (must be NULL)
                         KEY_QUERY_VALUE|KEY_READ, // just want to QUERY a value
                         &hOpenKey                 // address of handle to open key
                        ) == ERROR_SUCCESS) {

            LONG regErr;
            auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);

            DWORD szType = REG_SZ;
            DWORD reqSize = UDISP_CONFIG_STR_LEN;
            regErr=RegQueryValueEx(hOpenKey,
                        TEXT("udisp"),
                        NULL, 
                        &szType,
                        (PBYTE)tchar_udisp_registry,
                        &reqSize);
		if(regErr == NO_ERROR){		
			WideCharToMultiByte(CP_ACP,0, tchar_udisp_registry,-1, pDeviceContext->udisp_registry_dev_info.cstr,UDISP_CONFIG_STR_LEN,NULL,NULL);
        	LOGI(("udisp_registry Value = %s %d"), pDeviceContext->udisp_registry_dev_info.cstr, reqSize);
		}else {
            pDeviceContext->udisp_registry_dev_info.cstr[0] = '\0';
			 LOGD(("queryValue error:%x"),regErr);
		}
					
        } else {
            LOGD(("Could not open config section"));
        }
    
    LOGD(("ReadRegistryInformation--->"));
}

void decision_runtime_policy(WDFDEVICE Device){
	struct SampleMonitorMode mode;

	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
	int reg,w=0,h=0,enc,quality,fps,blimit;
	registry_config_dev_info(Device);
	if(strlen(pDeviceContext->udisp_registry_dev_info.cstr)>3){
			parse_usb_dev_info(pDeviceContext->udisp_registry_dev_info.cstr,&reg,&w,&h,&enc,&quality,&fps,&blimit);
			LOGI("config %s by registry\n", pDeviceContext->udisp_registry_dev_info.cstr);

		}
	else {
		parse_usb_dev_info(pDeviceContext->udisp_dev_info.cstr,&reg,&w,&h,&enc,&quality,&fps,&blimit);
	}
	pDeviceContext->w=w;
	pDeviceContext->h=h;
	pDeviceContext->enc=enc;
	pDeviceContext->quality=quality;
	pDeviceContext->fps = fps;
	pDeviceContext->blimit=blimit;
	pDeviceContext->dbg_mode =0;
	LOGI("w%d h%d enc%d quaility%d fps:%d blimit:%d",w,h,enc,quality,fps,blimit);
	if(w != 0) {
		mode.Width = w;
		mode.Height = h;
		mode.VSync = 60;
		pDeviceContext->pContext->monitor_modes.push_back(mode);
		if(w <= scale_res ) {
			mode.Width = w*2;
			mode.Height = h*2;
			mode.VSync = 60;
			pDeviceContext->pContext->monitor_modes.push_back(mode);
		}
	} else {
			for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(s_SampleDefaultModes); ModeIndex++)
			{
			mode.Width = s_SampleDefaultModes[ModeIndex].Width;
			mode.Height = s_SampleDefaultModes[ModeIndex].Height;
			mode.VSync = s_SampleDefaultModes[ModeIndex].VSync;	
			pDeviceContext->pContext->monitor_modes.push_back(mode);
			}		  
	}
	LOGI("%s scale_res:%d monitor size:%d",__func__,scale_res,pDeviceContext->pContext->monitor_modes.size());
}

NTSTATUS
idd_usbdisp_evt_device_prepareHardware(
	WDFDEVICE Device,
	WDFCMRESLIST ResourceList,
	WDFCMRESLIST ResourceListTranslated
	)
	/*++

	Routine Description:

	In this callback, the driver does whatever is necessary to make the
	hardware ready to use.  In the case of a USB device, this involves
	reading and selecting descriptors.

	Arguments:

	Device - handle to a device

	ResourceList - handle to a resource-list object that identifies the
	raw hardware resources that the PnP manager assigned
	to the device

	ResourceListTranslated - handle to a resource-list object that
	identifies the translated hardware resources
	that the PnP manager assigned to the device

	Return Value:

	NT status value

	--*/
{
	NTSTATUS                            status;
	//PDEVICE_CONTEXT                     pDeviceContext;
	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
	WDF_USB_DEVICE_INFORMATION          deviceInfo;
	ULONG                               waitWakeEnable;

	UNREFERENCED_PARAMETER(ResourceList);
	UNREFERENCED_PARAMETER(ResourceListTranslated);
	waitWakeEnable = FALSE;
	PAGED_CODE();

	LOG("--> EvtDevicePrepareHardware\n");

	//pDeviceContext = GetDeviceContext(Device);


	//
	// Create a USB device handle so that we can communicate with the
	// underlying USB stack. The WDFUSBDEVICE handle is used to query,
	// configure, and manage all aspects of the USB device.
	// These aspects include device properties, bus properties,
	// and I/O creation and synchronization. We only create device the first
	// the PrepareHardware is called. If the device is restarted by pnp manager
	// for resource rebalance, we will use the same device handle but then select
	// the interfaces again because the USB stack could reconfigure the device on
	// restart.
	//
	if (pDeviceContext->UsbDevice == NULL) {

#if UMDF_VERSION_MINOR >= 25
		WDF_USB_DEVICE_CREATE_CONFIG createParams;

		WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams,
			USBD_CLIENT_CONTRACT_VERSION_602);

		status = WdfUsbTargetDeviceCreateWithParameters(
			Device,
			&createParams,
			WDF_NO_OBJECT_ATTRIBUTES,
			&pDeviceContext->UsbDevice);

#else
		status = WdfUsbTargetDeviceCreate(Device,
			WDF_NO_OBJECT_ATTRIBUTES,
			&pDeviceContext->UsbDevice);


#endif

		if (!NT_SUCCESS(status)) {
			LOG("WdfUsbTargetDeviceCreate failed with Status code %x\n", status);
			return status;
		}
	}

	//
	// Retrieve USBD version information, port driver capabilites and device
	// capabilites such as speed, power, etc.
	//
	WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);

	status = WdfUsbTargetDeviceRetrieveInformation(
		pDeviceContext->UsbDevice,
		&deviceInfo);
	if (NT_SUCCESS(status)) {


		waitWakeEnable = deviceInfo.Traits &
			WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

		//
		// Save these for use later.
		//
		pDeviceContext->UsbDeviceTraits = deviceInfo.Traits;
	}
	else {
		pDeviceContext->UsbDeviceTraits = 0;
	}
#if 1
	status = SelectInterfaces(Device);
	if (!NT_SUCCESS(status)) {
		LOG("SelectInterfaces failed 0x%x\n", status);
		return status;
	}
#endif


	get_usb_dev_string_info(Device,pDeviceContext->tchar_udisp_devinfo);
	WideCharToMultiByte(CP_ACP,0,pDeviceContext->tchar_udisp_devinfo,-1,pDeviceContext->udisp_dev_info.cstr,UDISP_CONFIG_STR_LEN,NULL,NULL);
	decision_runtime_policy(Device);

	LOG("<-- EvtDevicePrepareHardware\n");

	return status;
}

int usb_transf_init(SLIST_HEADER * urb_list){
    int i = 0;
    purb_itm_t purb;
	NTSTATUS status;

	LOG("%s init urb list\n",__func__);
	InitializeSListHead(urb_list);
    // Insert into the list.
#define MAX_URB_SIZE 3  //1 for encoder, 2 for usb transfer with pingpong mode.
    for(i = 1; i <= MAX_URB_SIZE; i++) {
        purb = (urb_itm_t *)_aligned_malloc(sizeof(urb_itm_t),
                                            MEMORY_ALLOCATION_ALIGNMENT);
        if(NULL == purb) {
            LOGE("Memory allocation failed.\n");
            break;
        }
		status = WdfRequestCreate(
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL,
                     &purb->Request
                 );

        if(!NT_SUCCESS(status)) {
            LOGE("create request NG\n");
            break;//return status;
        }
        purb->id = i;
        purb->urb_list = urb_list;
        InterlockedPushEntrySList(urb_list,
                                  &(purb->node));
		LOGD("create urb item %d\n",purb->id);
       
    }
	return 0;
}



int usb_transf_exit(SLIST_HEADER * urb_list){
    int i = 0;
    purb_itm_t purb;
	LOGD("%s exit urb list\n",__func__);

    for(i = 1; i <= MAX_URB_SIZE; i++) {

        PSLIST_ENTRY 	pentry = InterlockedPopEntrySList(urb_list);
		purb = (urb_itm_t*)pentry;

        if(NULL == purb) {
            LOGW("List is empty.\n");
            break;
        }        
        LOGD("id is %d\n", purb->id);

        // This example assumes that the SLIST_ENTRY structure is the
        // first member of the structure. If your structure does not
        // follow this convention, you must compute the starting address
        // of the structure before calling the free function.
        WdfObjectDelete(purb->Request);
        _aligned_free(pentry);


    }
	return 0;
}


