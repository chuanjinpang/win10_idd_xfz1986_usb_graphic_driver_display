/*++

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF

--*/

#include "Driver.h"
#include "Driver.tmh"
#include "log.h"
#include "usb_transf.h"
#include "enc_base.h"
#include "enc_raw_rgb.h"
#include "enc_jpg.h"
#include <stdarg.h>
using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;
long get_system_us(void);
long get_fps(fps_mgr_t * mgr);
void put_fps_data(fps_mgr_t* mgr,long t);

NTSTATUS get_usb_dev_string_info(_In_ WDFDEVICE Device, TCHAR * stringBuf );

// Default modes reported for edid-less monitors. The first mode is set as preferred
static const struct IndirectSampleMonitor::SampleMonitorMode s_SampleDefaultModes[] = 
{
    { 240, 240, 60 },
    { 280, 240, 60 },
    { 320, 240, 60 },
    { 480, 272, 60 },
    { 480, 320, 60 },
	{ 480, 480, 60 },
	{ 640, 480, 60 },
	{ 800 ,480, 60 },
	{ 800 ,600, 60 },	
    { 960 ,960, 60 },
    { 1024 ,600, 60 },
    { 1024 ,768, 60 },
    { 1280 ,600, 60 },
    { 1280 ,720, 60 },
    { 1280 ,800, 60 },

};

// FOR SAMPLE PURPOSES ONLY, Static info about monitors that will be reported to OS
static const struct IndirectSampleMonitor s_SampleMonitors[] =
{
    // Modified EDID from Dell S2719DGF
    {
        {
            0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x10,0xAC,0xE6,0xD0,0x55,0x5A,0x4A,0x30,0x24,0x1D,0x01,
            0x04,0xA5,0x3C,0x22,0x78,0xFB,0x6C,0xE5,0xA5,0x55,0x50,0xA0,0x23,0x0B,0x50,0x54,0x00,0x02,0x00,
            0xD1,0xC0,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x58,0xE3,0x00,
            0xA0,0xA0,0xA0,0x29,0x50,0x30,0x20,0x35,0x00,0x55,0x50,0x21,0x00,0x00,0x1A,0x00,0x00,0x00,0xFF,
            0x00,0x37,0x4A,0x51,0x58,0x42,0x59,0x32,0x0A,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFC,0x00,
            0x53,0x32,0x37,0x31,0x39,0x44,0x47,0x46,0x0A,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFD,0x00,0x28,
            0x9B,0xFA,0xFA,0x40,0x01,0x0A,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x2C
        },
        {
            { 480, 480, 60 },
        },
        0
    },
};
static inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, DWORD Width, DWORD Height, DWORD VSync, bool bMonitorMode)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;

    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
    Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;

    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.hSyncFreq.Denominator = 1;

    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;

    Mode.pixelRate = ((UINT64) VSync) * ((UINT64) Width) * ((UINT64) Height);
}

static IDDCX_MONITOR_MODE CreateIddCxMonitorMode(DWORD Width, DWORD Height, DWORD VSync, IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER)
{
    IDDCX_MONITOR_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    Mode.Origin = Origin;
    FillSignalInfo(Mode.MonitorVideoSignalInfo, Width, Height, VSync, true);

    return Mode;
}

static IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width, DWORD Height, DWORD VSync)
{
    IDDCX_TARGET_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync, false);

    return Mode;
}

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;


EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;


struct IndirectDeviceContextWrapper {
    IndirectDeviceContext* pContext;
    //usb disp
    WDFUSBDEVICE                    UsbDevice;

    WDFUSBINTERFACE                 UsbInterface;

    WDFUSBPIPE                      BulkReadPipe;
	ULONG max_in_pkg_size;

    WDFUSBPIPE                      BulkWritePipe;
	ULONG max_out_pkg_size;
    ULONG							UsbDeviceTraits;
    //

	config_cstr_t udisp_dev_info;
	TCHAR   tchar_udisp_devinfo[UDISP_CONFIG_STR_LEN];
	int w;
	int h;
	int enc;
	int quality;

    void Cleanup() {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WPP_INIT_TRACING(pDriverObject, pRegistryPath);

    LOGI("idd Driver xfz1986 v2.0  build 20240725\n");

    WDF_DRIVER_CONFIG_INIT(&Config,

                           IddSampleDeviceAdd
                          );

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    return Status;
}







_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{

    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);
	LOGI("IddSampleDeviceAdd\n");
    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    PnpPowerCallbacks.EvtDevicePrepareHardware = idd_usbdisp_evt_device_prepareHardware;

    //
    // These two callbacks start and stop the wdfusb pipe continuous reader
    // as we go in and out of the D0-working state.
    //

    //PnpPowerCallbacks.EvtDeviceD0Entry = idd_usbdisp_evt_device_D0Entry;
//    PnpPowerCallbacks.EvtDeviceD0Exit  = idd_usbdisp_evt_device_D0Exit;
//   PnpPowerCallbacks.EvtDeviceSelfManagedIoFlush = idd_usbdisp_evt_device_SelfManagedIoFlush;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;

    IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;

    IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if(!NT_SUCCESS(Status)) {
		LOG("IddCxDeviceInitConfig failed with Status code %x\n", Status);
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if(pContext) {
            pContext->Cleanup();
        }
    };

    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if(!NT_SUCCESS(Status)) {
        LOG("WdfDeviceCreate failed with Status code %x\n", Status);
        return Status;
    }

    Status = IddCxDeviceInitialize(Device);

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{

}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if(FAILED(hr)) {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if(FAILED(hr)) {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if(FAILED(hr)) {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, WDFDEVICE  WdfDevice, HANDLE NewFrameEvent)
    : m_hSwapChain(hSwapChain), m_Device(Device), mp_WdfDevice(WdfDevice), m_hAvailableBufferEvent(NewFrameEvent)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
	usb_transf_init(&urb_list);

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());
	usb_transf_exit(&urb_list);

    if(m_hThread.Get()) {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void test_usb_dev_info(void);
void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    

	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(mp_WdfDevice);
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);
    
	decision_runtime_encoder(mp_WdfDevice);
    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
	

    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}


#if 1

void scale_for_sample_2to1(uint32_t * dst, uint32_t * src, D3D11_TEXTURE2D_DESC * fd)
{
	int i = 0, j = 0;
	int line = fd->Width;
	int len=fd->Width * fd->Height;

	for (i = 0; i < len / 4; i++) {
		*dst++ = *src++;
		src++;
		j += 2;
		if (j >= line) {
			j = 0;
			src += line;
		}
	}
	fd->Width=fd->Width/2;
	fd->Height=fd->Height/2;
}

#endif


int enc_grab_surface( std::shared_ptr<Direct3DDevice> m_Device,ComPtr<IDXGIResource> AcquiredBuffer, uint8_t *fb_buf,D3D11_TEXTURE2D_DESC *);

void SwapChainProcessor::RunCore()
{
    // Get the DXGI device interface
    ComPtr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->Device.As(&DxgiDevice);
    if(FAILED(hr)) {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if(FAILED(hr)) {
        return;
    }

    // Acquire and release buffers in a loop
    for(;;) {
        ComPtr<IDXGIResource> AcquiredBuffer;
        
        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if(hr == E_PENDING) {
            // We must wait for a new buffer
            HANDLE WaitHandles [] = {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if(WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT) {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            } else if(WaitResult == WAIT_OBJECT_0 + 1) {
                // We need to terminate
                break;
            } else {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        } else if(SUCCEEDED(hr)) {
            AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            // ==============================
            //LOG("-dxgi fid:%d dirty:%d\n",Buffer.MetaData.PresentationFrameNumber,Buffer.MetaData.DirtyRectCount);
            {
#if 1
				D3D11_TEXTURE2D_DESC frameDescriptor;
				auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(this->mp_WdfDevice);

	            enc_grab_surface(m_Device , AcquiredBuffer , this->fb_buf ,&frameDescriptor);
			#if 1
                if(pContext->w*2 == frameDescriptor.Width) {
					LOGD("scale...%d to %d \n",frameDescriptor.Width,pContext->w);
                    scale_for_sample_2to1((uint32_t *)this->fb_buf, (uint32_t *)this->fb_buf, &frameDescriptor);
                   
                } 
			#endif	
				int total_bytes = encoder->enc(&this->msg_buf[sizeof(udisp_frame_header_t)],this->fb_buf,0, 0, frameDescriptor.Width-1, frameDescriptor.Height-1, frameDescriptor.Width);
				encoder->enc_header(this->msg_buf,0,0, frameDescriptor.Width - 1, frameDescriptor.Height - 1, total_bytes);

				


				if(STATUS_SUCCESS == usb_transf_msg(&urb_list,pContext->BulkWritePipe,this->msg_buf,total_bytes+sizeof(udisp_frame_header_t))){
					put_fps_data(&fps_mgr,get_system_us());
					LOGI("%d fps:%.2f\n", total_bytes+sizeof(udisp_frame_header_t), ((float)get_fps(&fps_mgr))/10);
				} 
					
#else
#if 0
				int total_bytes = 64-16;
				encoder->enc_header(this->msg_buf,0,0, 8, 8, total_bytes);

				auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(this->mp_WdfDevice);
				usb_transf_msg(&urb_list,pContext->BulkWritePipe,this->msg_buf,total_bytes+sizeof(udisp_frame_header_t));
#endif
				//	LOG("dryrun... \n");

#endif
            }

next:

            AcquiredBuffer.Reset();
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if(FAILED(hr)) {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        } else {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
}

IndirectDeviceContext::~IndirectDeviceContext()
{
    m_ProcessingThread.reset();
}

#define NUM_VIRTUAL_DISPLAYS 1

void IndirectDeviceContext::InitAdapter()
{
    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================

    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported = NUM_VIRTUAL_DISPLAYS;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IddSample Model";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;


    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;


    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if(NT_SUCCESS(Status)) {
        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

void IndirectDeviceContext::FinishInit()
{
	LOG("IndirectDeviceContext::FinishInit\n");
    for(unsigned int i = 0; i < NUM_VIRTUAL_DISPLAYS; i++) {
        CreateMonitor(i);
    }
}

void IndirectDeviceContext::CreateMonitor(unsigned int ConnectorIndex)
{
    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDID
    // provided here is purely for demonstration, as it describes only 640x480 @ 60 Hz and 800x600 @ 60 Hz. Monitor
    // manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS to optimize
    // settings like viewing distance and scale factor. Manufacturers should also use a unique serial number every
    // single device to ensure the OS can tell the monitors apart.
    // ==============================

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDDCX_MONITOR_INFO MonitorInfo = {};
    MonitorInfo.Size = sizeof(MonitorInfo);
    MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    MonitorInfo.ConnectorIndex = ConnectorIndex;

    MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
    MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    if (ConnectorIndex >= ARRAYSIZE(s_SampleMonitors))
    {
        MonitorInfo.MonitorDescription.DataSize = 0;
        MonitorInfo.MonitorDescription.pData = nullptr;
    }
    else
    {
        MonitorInfo.MonitorDescription.DataSize = IndirectSampleMonitor::szEdidBlock;
        MonitorInfo.MonitorDescription.pData = const_cast<BYTE*>(s_SampleMonitors[ConnectorIndex].pEdidBlock);
    }

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    // ==============================

    // Create a container ID
    CoCreateGuid(&MonitorInfo.MonitorContainerId);

    IDARG_IN_MONITORCREATE MonitorCreate = {};
    MonitorCreate.ObjectAttributes = &Attr;
    MonitorCreate.pMonitorInfo = &MonitorInfo;

    // Create a monitor object with the specified monitor descriptor
    IDARG_OUT_MONITORCREATE MonitorCreateOut;

    LOG("IddCxMonitorCreate\n");
    NTSTATUS Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
    if(NT_SUCCESS(Status)) {
        m_Monitor = MonitorCreateOut.MonitorObject;

        // Associate the monitor with this device context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorCreateOut.MonitorObject);
        pContext->pContext = this;

        // Tell the OS that the monitor has been plugged in
        IDARG_OUT_MONITORARRIVAL ArrivalOut;
        LOG("IddCxMonitorArrival\n");
        Status = IddCxMonitorArrival(m_Monitor, &ArrivalOut);
    }
}

void IndirectDeviceContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    LOGI("AssignSwapChain\n");
    if(FAILED(Device->Init())) {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
    } else {
        // Create a new swap-chain processing thread
         LOGI("AssignSwapChain new SwapChainProcessor\n");
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, this->m_WdfDevice, NewFrameEvent));
    }
}

void IndirectDeviceContext::UnassignSwapChain()
{
    // Stop processing the last swap-chain
    LOGI("UnassignSwapChain\n");
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
    if(NT_SUCCESS(pInArgs->AdapterInitStatus)) {
        pContext->pContext->FinishInit();
    }

    LOG("IddSampleAdapterInitFinished\n");


    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
    // this sample driver, we hard-code the EDID, so this function can generate known modes.
    // ==============================

    pOutArgs->MonitorModeBufferOutputCount = ARRAYSIZE(s_SampleDefaultModes);
	LOG("%s %d %d\n",__func__, pInArgs->MonitorModeBufferInputCount , ARRAYSIZE(s_SampleDefaultModes));
    if (pInArgs->MonitorModeBufferInputCount < ARRAYSIZE(s_SampleDefaultModes))
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }
    else
    {
        // In the sample driver, we have reported some static information about connected monitors
        // Check which of the reported monitors this call is for by comparing it to the pointer of
        // our known EDID blocks.

        if (pInArgs->MonitorDescription.DataSize != IndirectSampleMonitor::szEdidBlock)
            return STATUS_INVALID_PARAMETER;

        DWORD SampleMonitorIdx = 0;
        for(; SampleMonitorIdx < ARRAYSIZE(s_SampleMonitors); SampleMonitorIdx++)
        {
            if (memcmp(pInArgs->MonitorDescription.pData, s_SampleMonitors[SampleMonitorIdx].pEdidBlock, IndirectSampleMonitor::szEdidBlock) == 0)
            {
                // Copy the known modes to the output buffer
                for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(s_SampleDefaultModes); ModeIndex++)
                {
                    pInArgs->pMonitorModes[ModeIndex] = CreateIddCxMonitorMode(
                        s_SampleDefaultModes[ModeIndex].Width,
                        s_SampleDefaultModes[ModeIndex].Height,
                        s_SampleDefaultModes[ModeIndex].VSync,
                        IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR
                    );

    
					LOGI("%s %d %d %d\n", __func__, ModeIndex, s_SampleDefaultModes[ModeIndex].Width,
                        s_SampleDefaultModes[ModeIndex].Height,
                        s_SampleDefaultModes[ModeIndex].VSync);
                }

                // Set the preferred mode as represented in the EDID
                pOutArgs->PreferredMonitorModeIdx = 2;
        		LOGI("%s perf:%d\n", __func__,pOutArgs->PreferredMonitorModeIdx);
                return STATUS_SUCCESS;
            }
        }

        // This EDID block does not belong to the monitors we reported earlier
        return STATUS_INVALID_PARAMETER;
    }
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    //UNREFERENCED_PARAMETER(MonitorObject);
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);

    // Should never be called since we create a single monitor with a known EDID in this sample driver.

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    // ==============================
	LOG("%s %d\n",__func__,pInArgs->DefaultMonitorModeBufferInputCount );

    if (pInArgs->DefaultMonitorModeBufferInputCount == 0)
    {
        pOutArgs->DefaultMonitorModeBufferOutputCount = pContext->pContext->monitor_modes.size();
    }
    else
    {
        for (DWORD ModeIndex = 0; ModeIndex < pContext->pContext->monitor_modes.size() ; ModeIndex++)
        {
            pInArgs->pDefaultMonitorModes[ModeIndex] = CreateIddCxMonitorMode(
                pContext->pContext->monitor_modes[ModeIndex].Width,
                pContext->pContext->monitor_modes[ModeIndex].Height,
                pContext->pContext->monitor_modes[ModeIndex].VSync,
                IDDCX_MONITOR_MODE_ORIGIN_DRIVER
            );
				LOGI("%s %d %d %d\n", __func__, ModeIndex, pContext->pContext->monitor_modes[ModeIndex].Width,
                        pContext->pContext->monitor_modes[ModeIndex].Height,
                        pContext->pContext->monitor_modes[ModeIndex].VSync);
        }

        pOutArgs->DefaultMonitorModeBufferOutputCount = pContext->pContext->monitor_modes.size();
        pOutArgs->PreferredMonitorModeIdx = 0;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
   // UNREFERENCED_PARAMETER(MonitorObject);
	auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes;

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

#if 0
    TargetModes.push_back(CreateIddCxTargetMode(1024, 600, 60));
    TargetModes.push_back(CreateIddCxTargetMode(800, 600, 60));
	#endif
	for (DWORD ModeIndex = 0; ModeIndex < pContext->pContext->monitor_modes.size() ; ModeIndex++)
	{
		TargetModes.push_back(CreateIddCxTargetMode(
                pContext->pContext->monitor_modes[ModeIndex].Width,
                pContext->pContext->monitor_modes[ModeIndex].Height,
                pContext->pContext->monitor_modes[ModeIndex].VSync));
			LOGI("%s %d %d %d\n", __func__, ModeIndex, pContext->pContext->monitor_modes[ModeIndex].Width,
                pContext->pContext->monitor_modes[ModeIndex].Height,
                pContext->pContext->monitor_modes[ModeIndex].VSync);
	}
    

	LOGD("%s %p %p in:%d out:%d\n", __func__,pContext,pContext->pContext, pInArgs->TargetModeBufferInputCount,pOutArgs->TargetModeBufferOutputCount);

    pOutArgs->TargetModeBufferOutputCount = (UINT) TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);

    LOG("IddSampleMonitorAssignSwapChain\n");
    pContext->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(MonitorObject);
    LOG("IddSampleMonitorUnassignSwapChain\n");
    pContext->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}



int enc_grab_surface( std::shared_ptr<Direct3DDevice> m_Device,ComPtr<IDXGIResource> AcquiredBuffer, uint8_t *fb_buf, D3D11_TEXTURE2D_DESC * pframeDescriptor){
    HRESULT hr;
#define RESET_OBJECT(obj) { if(obj) obj->Release(); obj = NULL; }
            ID3D11Texture2D *hAcquiredDesktopImage = NULL;
            hr = AcquiredBuffer->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&hAcquiredDesktopImage));
            if(FAILED(hr)) {
                LOGE("dxgi 2d NG\n");
                goto next;
            }

            // copy old description
            //
            D3D11_TEXTURE2D_DESC frameDescriptor;
            hAcquiredDesktopImage->GetDesc(&frameDescriptor);


            //
            // create a new staging buffer for cpu accessable, or only GPU can access
            //
            ID3D11Texture2D *hNewDesktopImage = NULL;
            frameDescriptor.Usage = D3D11_USAGE_STAGING;
            frameDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            frameDescriptor.BindFlags = 0;
            frameDescriptor.MiscFlags = 0;
            frameDescriptor.MipLevels = 1;
            frameDescriptor.ArraySize = 1;
            frameDescriptor.SampleDesc.Count = 1;
            *pframeDescriptor = frameDescriptor;
            hr = m_Device->Device->CreateTexture2D(&frameDescriptor, NULL, &hNewDesktopImage);
            if(FAILED(hr)) {
                RESET_OBJECT(hAcquiredDesktopImage);

                LOGE("dxgi create 2d NG\n");
                goto next;
            }

            //
            // copy next staging buffer to new staging buffer
            //
            m_Device->DeviceContext->CopyResource(hNewDesktopImage, hAcquiredDesktopImage);


            //
            // create staging buffer for map bits
            //
            IDXGISurface *hStagingSurf = NULL;
            hr = hNewDesktopImage->QueryInterface(__uuidof(IDXGISurface), (void **)(&hStagingSurf));
            RESET_OBJECT(hNewDesktopImage);//release it
            if(FAILED(hr)) {
                LOGE("dxgi surf NG\n");
                goto next;
            }

            //
            // copy bits to user space
            //
            DXGI_MAPPED_RECT mappedRect;
            hr = hStagingSurf->Map(&mappedRect, DXGI_MAP_READ);
            if(SUCCEEDED(hr)) {

				{
                    memcpy(fb_buf, mappedRect.pBits, frameDescriptor.Width * frameDescriptor.Height * 4);
                    
                }

                hStagingSurf->Unmap();
            } else {

                LOGE("dxgi map NG %x\n", hr);
            }

			RESET_OBJECT(hStagingSurf);
            RESET_OBJECT(hAcquiredDesktopImage);
        next:
            return 0;

}

LONG debug_level=LOG_LEVEL_WARN;
LONG scale_res=320;

#define  USB_INFO_STR_SIZE  16
void parse_usb_dev_info(char * str,int * reg,int * w, int * h, int * enc, int * quality){
char whstr[USB_INFO_STR_SIZE];
char enc_str[USB_INFO_STR_SIZE],type_str[USB_INFO_STR_SIZE];
int cnt;
	cnt = sscanf(str,"%[^_]_%[^_]_%[^_]",type_str,whstr,enc_str);
	if(cnt<3){
	LOGW("%s dev info string violation xfz1986 udisp SPEC, so use default\n",str);
	}
	int len=strlen(type_str);
	cnt=sscanf(&type_str[len-1],"%d",reg);
	if(cnt ==1){
		LOGD("udisp reg idx:%d\n",*reg);
	}else {
		*reg=0;
		LOGD("default udisp reg idx:%d\n",*reg);
		
	}

	
	cnt=sscanf(whstr,"R%dx%d",w,h);
	if(cnt == 2){
		LOGD("udisp w%d h%d\n",*w, *h);
	}else {
		*w=0;
		*h=0;
		LOGD("default udisp w%d h%d\n",*w, *h);
	}

	switch(enc_str[1]){
	case 'j':
		cnt=sscanf(enc_str,"Ejpg%d",quality);
		if(cnt==1){
			*enc=UDISP_TYPE_JPG;
			LOGD("enc:%d quality:%d\n",*enc,*quality);
		}else {
			*quality=4;
			LOGE("wrong Enc str %s",enc_str);	

		}
		break;
	case 'r':
		cnt=sscanf(enc_str,"Ergb%d",quality);
		if(cnt==1){
			if(*quality ==16 ){
				*enc=UDISP_TYPE_RGB565;
				LOGD("enc:%d quality:%d\n",*enc,*quality);
			}else if (*quality == 32)
				{
				*enc=UDISP_TYPE_RGB888;
				LOGD("enc:%d quality:%d\n",*enc,*quality);
			}
		}else {
			LOGE("wrong Enc str %s",enc_str);	

		}
		break;
	default:
		*enc=UDISP_TYPE_JPG;
		*quality=4;
		LOGD("default enc:%d quality:%d\n",*enc,*quality);
	}

}


void SwapChainProcessor::decision_runtime_encoder(WDFDEVICE Device){

	auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);

	LOG("%s enc%d quality%d\n",__func__,pDeviceContext->enc,pDeviceContext->quality);

	switch(pDeviceContext->enc){

	case UDISP_TYPE_RGB565:
		this->encoder = new enc_rgb565();
		break;
	case UDISP_TYPE_RGB888:
		this->encoder = new enc_rgb888a();
		break;
	case UDISP_TYPE_JPG:
		this->encoder = new enc_jpg(pDeviceContext->quality);
		break;
    default:
		this->encoder = new enc_jpg(4);
		break;
	}

}

#include "misc_helper.c"
#include "usb_transf.cpp"


#pragma endregion