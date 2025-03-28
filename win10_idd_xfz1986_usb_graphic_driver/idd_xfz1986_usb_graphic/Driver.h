#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "usbdi.h"
#include <wdf.h>
#include <wdfusb.h>
#include "Trace.h"
#include "enc_base.h"
namespace Microsoft
{
namespace WRL
{
namespace Wrappers
{
// Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
}


}
}

#define DISP_MAX_WIDTH  1920
#define DISP_MAX_HEIGHT 1080


typedef struct {
    SLIST_ENTRY node;
    WDFUSBPIPE pipe;
    int id;
    uint8_t		urb_msg[DISP_MAX_HEIGHT*DISP_MAX_WIDTH*4];
    PSLIST_HEADER urb_list;
    WDFREQUEST Request;
    WDFMEMORY  wdfMemory;
} urb_itm_t, *purb_itm_t;

#define FPS_STAT_MAX 8

typedef struct {
    int64_t tb[FPS_STAT_MAX];
    int cur;
    long last_fps;
} fps_mgr_t;

namespace Microsoft
{


namespace IndirectDisp
{
/// <summary>
/// Manages the creation and lifetime of a Direct3D render device.
/// </summary>
        struct IndirectSampleMonitor
        {
            static constexpr size_t szEdidBlock = 128;
            static constexpr size_t szModeList = 1;

            const BYTE pEdidBlock[szEdidBlock];
            const struct SampleMonitorMode {
                DWORD Width;
                DWORD Height;
                DWORD VSync;
            } pModeList[szModeList];
            const DWORD ulPreferredModeIdx;
        };
struct Direct3DDevice {
    Direct3DDevice(LUID AdapterLuid);
    Direct3DDevice();
    HRESULT Init();

    LUID AdapterLuid;
    Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};

#define  UDISP_CONFIG_STR_LEN  256

typedef struct{
	char   cstr[UDISP_CONFIG_STR_LEN];
} config_cstr_t;
/// <summary>
/// Manages a thread that consumes buffers from an indirect display swap-chain object.
/// </summary>
class SwapChainProcessor
{
public:
    SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, std::shared_ptr<Direct3DDevice> Device, WDFDEVICE      WdfDevice, HANDLE NewFrameEvent);
    ~SwapChainProcessor();

private:


    static DWORD CALLBACK RunThread(LPVOID Argument);

    void Run();
    void RunCore();
    void decision_runtime_encoder(WDFDEVICE Device);
public:
    IDDCX_SWAPCHAIN m_hSwapChain;
    std::shared_ptr<Direct3DDevice> m_Device;
    WDFDEVICE  mp_WdfDevice;
    uint8_t		fb_buf[DISP_MAX_HEIGHT*DISP_MAX_WIDTH*4];
	
    fps_mgr_t fps_mgr ;
    
	class enc_base * encoder;

    uint16_t gfid;
    SLIST_HEADER urb_list;
	int max_out_pkg_size;
    //urb_itm_t * curr_urb;
    HANDLE m_hAvailableBufferEvent;
    Microsoft::WRL::Wrappers::Thread m_hThread;
    Microsoft::WRL::Wrappers::Event m_hTerminateEvent;

};

struct SampleMonitorMode {
    DWORD Width;
    DWORD Height;
    DWORD VSync;
};

using namespace std;
/// <summary>
/// Provides a sample implementation of an indirect display driver.
/// </summary>
class IndirectDeviceContext
{
public:
    IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
    virtual ~IndirectDeviceContext();

    void InitAdapter();
    void FinishInit();

    void CreateMonitor(unsigned int index);

    void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent);
    void UnassignSwapChain();

protected:

    WDFDEVICE m_WdfDevice;
    IDDCX_ADAPTER m_Adapter;
    IDDCX_MONITOR m_Monitor;
    IDDCX_MONITOR m_Monitor2;

    std::unique_ptr<SwapChainProcessor> m_ProcessingThread;

public:
    static const DISPLAYCONFIG_VIDEO_SIGNAL_INFO s_KnownMonitorModes[];
    static const BYTE s_KnownMonitorEdid[];
	vector<struct SampleMonitorMode > monitor_modes;
};
}
}

