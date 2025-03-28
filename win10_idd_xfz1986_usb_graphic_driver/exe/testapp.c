/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    TESTAPP.C

Abstract:

    Console test app for usbsamp.SYS driver

Environment:

    user mode only

--*/


#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <windows.h>

#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "devioctl.h"
#include "ioctl.h"
#include "strsafe.h"

#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int
#pragma warning(disable:4189)  // bit field types other than int
#pragma warning(disable:4101) 
#pragma warning(disable:4100) 
#pragma warning(disable:4005) 


#include <setupapi.h>
#include <basetyps.h>
#include "usbdi.h"

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)


#define NOISY(_x_) printf _x_ ;
#define MAX_LENGTH 256

char inPipe[MAX_LENGTH] = "PIPE00";     // pipe name for bulk input pipe on our test board
char outPipe[MAX_LENGTH] = "PIPE01";    // pipe name for bulk output pipe on our test board
char completeDeviceName[MAX_LENGTH] = "";  //generated from the GUID registered by the driver itself

BOOL fDumpUsbConfig = FALSE;    // flags set in response to console command line switches
BOOL fDumpReadData = FALSE;
BOOL fRead = FALSE;
BOOL fWrite = FALSE;
BOOL fCompareData = TRUE;

int gDebugLevel = 1;      // higher == more verbose, default is 1, 0 turns off all

ULONG IterationCount = 1; //count of iterations of the test we are to perform
int WriteLen = 0;         // #bytes to write
int ReadLen = 0;          // #bytes to read

// functions


HANDLE
OpenOneDevice (
    _In_  HDEVINFO                    HardwareDeviceInfo,
    _In_  PSP_DEVICE_INTERFACE_DATA   DeviceInfoData,
    _In_  PSTR                        devName
    )
/*++
Routine Description:

    Given the HardwareDeviceInfo, representing a handle to the plug and
    play information, and deviceInfoData, representing a specific usb device,
    open that device and fill in all the relevant information in the given
    USB_DEVICE_DESCRIPTOR structure.

Arguments:

    HardwareDeviceInfo:  handle to info obtained from Pnp mgr via SetupDiGetClassDevs()
    DeviceInfoData:      ptr to info obtained via SetupDiEnumDeviceInterfaces()

Return Value:

    return HANDLE if the open and initialization was successfull,
        else INVLAID_HANDLE_VALUE.

--*/
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA     functionClassDeviceData = NULL;
    ULONG                                predictedLength = 0;
    ULONG                                requiredLength = 0;
    HANDLE                               hOut = INVALID_HANDLE_VALUE;

    //
    // allocate a function class device data structure to receive the
    // goods about this particular device.
    //
    SetupDiGetDeviceInterfaceDetail (
            HardwareDeviceInfo,
            DeviceInfoData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node


    predictedLength = requiredLength;
    // sizeof (SP_FNCLASS_DEVICE_DATA) + 512;

    functionClassDeviceData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc (predictedLength);
    if(NULL == functionClassDeviceData) {
        return INVALID_HANDLE_VALUE;
    }
    functionClassDeviceData->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

    //
    // Retrieve the information from Plug and Play.
    //
    if (! SetupDiGetDeviceInterfaceDetail (
               HardwareDeviceInfo,
               DeviceInfoData,
               functionClassDeviceData,
               predictedLength,
               &requiredLength,
               NULL)) {
                free( functionClassDeviceData );
        return INVALID_HANDLE_VALUE;
    }

        (void)StringCchCopy( devName, MAX_LENGTH, functionClassDeviceData->DevicePath) ;
        printf( "Attempting to open %s\n", devName );

    hOut = CreateFile (
                  functionClassDeviceData->DevicePath,
                  0 ,
                 0,
                  NULL, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
                  0, // No special attributes
                  NULL); // No template file

    if (INVALID_HANDLE_VALUE == hOut) {
                
                printf("Failed to open (%s) = %u\n", devName, GetLastError());
    }else
    	{
		printf("open (%s) pass\n", devName);
    	}
      free( functionClassDeviceData );
        return hOut;
}


HANDLE
OpenUsbDevice(
    _In_ LPGUID  pGuid,
    _In_ PSTR    outNameBuf
    )
/*++
Routine Description:

   Do the required PnP things in order to find
   the next available proper device in the system at this time.

Arguments:

    pGuid:      ptr to GUID registered by the driver itself
    outNameBuf: the generated name for this device

Return Value:

    return HANDLE if the open and initialization was successful,
        else INVLAID_HANDLE_VALUE.
--*/
{
   ULONG NumberDevices;
   HANDLE hOut = INVALID_HANDLE_VALUE;
   HDEVINFO                 hardwareDeviceInfo;
   SP_DEVICE_INTERFACE_DATA deviceInfoData;
   ULONG                    i;
   BOOLEAN                  done;
   PUSB_DEVICE_DESCRIPTOR   usbDeviceInst;
   PUSB_DEVICE_DESCRIPTOR  *UsbDevices = &usbDeviceInst;
   PUSB_DEVICE_DESCRIPTOR   tempDevDesc;

   *UsbDevices = NULL;
   tempDevDesc = NULL;
   NumberDevices = 0;

   //
   // Open a handle to the plug and play dev node.
   // SetupDiGetClassDevs() returns a device information set that contains
   // info on all installed devices of a specified class.
   //
   hardwareDeviceInfo =
       SetupDiGetClassDevs ( pGuid,
                             NULL, // Define no enumerator (global)
                             NULL, // Define no
                             (DIGCF_PRESENT |           // Only Devices present
                               DIGCF_DEVICEINTERFACE)); // Function class devices.

   if (hardwareDeviceInfo == INVALID_HANDLE_VALUE) {
       return INVALID_HANDLE_VALUE ;
   }
   //
   // Take a wild guess at the number of devices we have;
   // Be prepared to realloc and retry if there are more than we guessed
   //
   NumberDevices = 4;
   done = FALSE;
   deviceInfoData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

   i=0;
   while (!done) {
      NumberDevices *= 2;

      if (*UsbDevices) {
          tempDevDesc = (PUSB_DEVICE_DESCRIPTOR)
             realloc (*UsbDevices, (NumberDevices * sizeof (USB_DEVICE_DESCRIPTOR)));
          if(tempDevDesc) {
              *UsbDevices = tempDevDesc;
              tempDevDesc = NULL;
          }
          else {
              free(*UsbDevices);
              *UsbDevices = NULL;
          }
      } else {
         *UsbDevices = (PUSB_DEVICE_DESCRIPTOR)calloc (NumberDevices, sizeof (USB_DEVICE_DESCRIPTOR));
      }

      if (NULL == *UsbDevices) {

         // SetupDiDestroyDeviceInfoList destroys a device information set
         // and frees all associated memory.

         SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
         return INVALID_HANDLE_VALUE;
      }

      for (; i < NumberDevices; i++) {
          // SetupDiEnumDeviceInterfaces() returns information about device
          // interfaces exposed by one or more devices. Each call returns
          // information about one interface; the routine can be called
          // repeatedly to get information about several interfaces exposed
          // by one or more devices.

         if (SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                                         0, // We don't care about specific PDOs
                                         pGuid,
                                         i,
                                         &deviceInfoData)) {

            hOut = OpenOneDevice (hardwareDeviceInfo, &deviceInfoData, outNameBuf);
                        if ( hOut != INVALID_HANDLE_VALUE ) {
               done = TRUE;
               break;
                        }
         } else {
             //printf("Failed to open (%s) = %u\n", completeDeviceName, GetLastError());
            if (ERROR_NO_MORE_ITEMS == GetLastError()) {
               done = TRUE;
               break;
            }
         }
      }
   }

   // SetupDiDestroyDeviceInfoList() destroys a device information set
   // and frees all associated memory.

   SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
   free ( *UsbDevices );
   return hOut;
}




BOOL
GetUsbDeviceFileName(
    _In_ LPGUID  pGuid,
    _In_ PSTR    outNameBuf
    )
/*++
Routine Description:

    Given a ptr to a driver-registered GUID, give us a string with the device name
    that can be used in a CreateFile() call.
    Actually briefly opens and closes the device and sets outBuf if successfull;
    returns FALSE if not

Arguments:

    pGuid:      ptr to GUID registered by the driver itself
    outNameBuf: the generated zero-terminated name for this device

Return Value:

    TRUE on success else FALSE

--*/
{
    HANDLE hDev = OpenUsbDevice( pGuid, outNameBuf );

    if ( hDev != INVALID_HANDLE_VALUE ) {
        CloseHandle( hDev );
        return TRUE;
    }
    return FALSE;
}

HANDLE
open_dev()
/*++
Routine Description:

    Called by dumpUsbConfig() to open an instance of our device

Arguments:

    None

Return Value:

    Device handle on success else NULL

--*/
{
    HANDLE hDEV = OpenUsbDevice( (LPGUID)&GUID_DEVINTERFACE_UDISP1986,
                                 completeDeviceName);

    if (hDEV == INVALID_HANDLE_VALUE) {
        printf("Failed to open (%s) = %u", completeDeviceName, GetLastError());
    } else {
        printf("DeviceName = (%s)\n", completeDeviceName);
    }

    return hDEV;

}


HANDLE
open_file(
    _In_ PSTR filename
    )
/*++
Routine Description:

    Called by main() to open an instance of our device after obtaining its name

Arguments:

    None

Return Value:

    Device handle on success else NULL

--*/
{
    int success = 1;
    HANDLE h;

    if ( !GetUsbDeviceFileName(
            (LPGUID) &GUID_DEVINTERFACE_UDISP1986,
            completeDeviceName) )
    {
            NOISY(("Failed to GetUsbDeviceFileName err - %u\n", GetLastError()));
            return  INVALID_HANDLE_VALUE;
    }

    (void) StringCchCat (completeDeviceName, MAX_LENGTH, "\\" );

    if(FAILED(StringCchCat (completeDeviceName, MAX_LENGTH, filename))) {
        NOISY(("Failed to open handle - possibly long filename\n"));
        return INVALID_HANDLE_VALUE;
    }

    printf("completeDeviceName = (%s)\n", completeDeviceName);

    h = CreateFile(completeDeviceName,
            GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

    if (h == INVALID_HANDLE_VALUE) {
        NOISY(("Failed to open (%s) = %u", completeDeviceName, GetLastError()));
        success = 0;
    } else {
        NOISY(("Opened successfully.\n"));
    }

    return h;
}

void
usage()
/*++
Routine Description:

    Called by main() to dump usage info to the console when
    the app is called with no parms or with an invalid parm

Arguments:

    None

Return Value:

    None

--*/
{
    static int i=1;

    if (i) {
        printf("Usage for Read/Write test:\n");
        printf("-r [n] where n is number of bytes to read\n");
        printf("-w [n] where n is number of bytes to write\n");
        printf("-c [n] where n is number of iterations (default = 1)\n");
        printf("-i [s] where s is the input pipe\n");
        printf("-o [s] where s is the output pipe\n");
        printf("-v verbose -- dumps read data\n");
        printf("-x to skip validation of read and write data\n");

        printf("\nUsage for USB and Endpoint info:\n");
        printf("-u to dump USB configuration and pipe info \n");
        i = 0;
    }
}


void
parse(
    _In_ int argc,
    _In_reads_(argc) char *argv[]
    )
/*++
Routine Description:

    Called by main() to parse command line parms

Arguments:

    argc and argv that was passed to main()

Return Value:

    Sets global flags as per user function request

--*/
{
    int i;

    if ( argc < 2 ) // give usage if invoked with no parms
        usage();

    for (i=0; i<argc; i++) {
        if (argv[i][0] == '-' ||
            argv[i][0] == '/') {
            switch(argv[i][1]) {
            case 'r':
            case 'R':
                if (i+1 >= argc) {
                    usage();
                    exit(1);
                }
                else {
#pragma warning(suppress: 6385)
                    ReadLen = atoi(&argv[i+1][0]);
                    if (ReadLen == 0) {
                        usage();
                        exit(1);
                    }
                    fRead = TRUE;
                }
                i++;
                break;
            case 'w':
            case 'W':
                if (i+1 >= argc) {
                    usage();
                    exit(1);
                }
                else {
                    WriteLen = atoi(&argv[i+1][0]);
                    fWrite = TRUE;
                }
                i++;
                break;
            case 'c':
            case 'C':
                if (i+1 >= argc) {
                    usage();
                    exit(1);
                }
                else {
                    IterationCount = atoi(&argv[i+1][0]);
                    if (IterationCount == 0) {
                        usage();
                        exit(1);
                    }
                }
                i++;
                break;
            case 'i':
            case 'I':
                if (i+1 >= argc) {
                    usage();
                    exit(1);
                }
                else {
                    (void)StringCchCopy(inPipe, MAX_LENGTH, &argv[i+1][0]);
                }
                i++;
                break;
            case 'u':
            case 'U':
                fDumpUsbConfig = TRUE;
                                i++;
                break;
            case 'v':
            case 'V':
                fDumpReadData = TRUE;
                i++;
                break;
            case 'x':
            case 'X':
                fCompareData = FALSE;
                i++;
                break;
             case 'o':
             case 'O':
                 if (i+1 >= argc) {
                     usage();
                     exit(1);
                 }
                 else {
                     (void)StringCchCopy(outPipe, MAX_LENGTH,  &argv[i+1][0]);
                 }
                i++;
                break;
            default:
                usage();
            }
        }
    }
}

BOOL
compare_buffs(
    _In_reads_bytes_(length) char *buff1,
    _In_reads_bytes_(length) char *buff2,
    _In_ int   length
    )
/*++
Routine Description:

    Called to verify read and write buffers match for loopback test

Arguments:

    buffers to compare and length

Return Value:

    TRUE if buffers match, else FALSE

--*/
{
    int ok = 1;

    if (memcmp(buff1, buff2, length )) {
        // Edi, and Esi point to the mismatching char and ecx indicates the
        // remaining length.
        ok = 0;
    }

    return ok;
}

#define NPERLN 8

void
dump(
   UCHAR *b,
   int len
)
/*++
Routine Description:

    Called to do formatted ascii dump to console of the io buffer

Arguments:

    buffer and length

Return Value:

    none

--*/
{
    ULONG i;
    ULONG longLen = (ULONG)len / sizeof( ULONG );
    PULONG pBuf = (PULONG) b;

    // dump an ordinal ULONG for each sizeof(ULONG)'th byte
    printf("\n****** BEGIN DUMP LEN decimal %d, 0x%x\n", len,len);
    for (i=0; i<longLen; i++) {
        printf("%04X ", *pBuf++);
        if (i % NPERLN == (NPERLN - 1)) {
            printf("\n");
        }
    }
    if (i % NPERLN != 0) {
        printf("\n");
    }
    printf("\n****** END DUMP LEN decimal %d, 0x%x\n", len,len);
}

#define LOGD printf
#define LOGE printf

#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG		3

#include <stdint.h>

typedef struct _udisp_frame_header_t {  //16bytes
	uint16_t crc16;//payload crc16
    uint8_t  type; //raw rgb,yuv,jpg,other
    uint8_t  cmd;    
    uint16_t x;  //32bit
    uint16_t y;
    uint16_t width;//32bit
    uint16_t height;
	uint32_t frame_id:10;
    uint32_t payload_total:22; //payload max 4MB
}  udisp_frame_header_t;

#define cpu_to_le16(x)  (uint16_t)(x)

int disp_setup_frame_header(uint8_t * msg, int x, int y, int right, int bottom, uint8_t op_flg ,uint32_t total)
{
	udisp_frame_header_t * pfh;
	static int fid=0;

	pfh = (udisp_frame_header_t *)msg;
	pfh->type = op_flg;
	pfh->crc16 = 0;
	pfh->x = cpu_to_le16(x);
	pfh->y = cpu_to_le16(y);
	pfh->y = cpu_to_le16(y);
	pfh->width = cpu_to_le16(right + 1 - x);
	pfh->height = cpu_to_le16(bottom + 1 - y);
	pfh->payload_total = total;
	pfh->frame_id=fid++;

	LOGD("fid:%d enc:%d %d\n",pfh->frame_id, pfh->type,pfh->payload_total);

	return sizeof(udisp_frame_header_t);

}




uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}




int lcd_colorbar(uint32_t    * buffer, int LCD_WIDTH,int LCD_HEIGHT)
{
    int fact = LCD_HEIGHT/24;
	int bcnt=0;
	int ofs=0;
    
        for (int y = 0; y < LCD_HEIGHT; y += fact) {
			for (int i = 0; i < LCD_WIDTH * fact; i++) {
            	//buffer[i] = 1<<bcnt;
            	buffer[ofs++]=0x1<< bcnt;
       		}
			bcnt++;            
        }

    return 0;
}
#include"tiny_jpeg.h"


int enc_jpg_enc( uint8_t * enc, uint8_t * src,int x, int y, int right, int bottom, int line_width,int limit)
{
	stream_mgr_t m_mgr;
	stream_mgr_t * mgr = &m_mgr;
	int jpg_quality=4;
#if 0
	int len=rgb888a_to_rgb888(enc,(uint32_t*)src,x,y,right,bottom,line_width);
	memcpy(src,enc,len);
#endif
	//ok we use jpeg transfer data
	retry:
	mgr->data = enc;
	mgr->max = JPEG_MAX_SIZE;
	mgr->dp = 0;
	if (!tje_encode_to_ctx(mgr, (right - x + 1), (bottom - y + 1), 4, src, jpg_quality)) {
		LOGE("Could not encode JPEG\n");
	}

	LOGD("jpg: total:%d q:%d\n", mgr->dp, jpg_quality);
	if(mgr->dp > limit && jpg_quality>1){
			jpg_quality--;
			goto retry;
	}
	return mgr->dp;
}


void
rw_dev( HANDLE hDEV , int op )
/*++
Routine Description:

    Called to do formatted ascii dump to console of  USB
    configuration, interface, and endpoint descriptors
    (Cmdline "rwbulk -u" )

Arguments:

    handle to device

Return Value:

    none

--*/
{
#define TEST_BUF_SIZE 240*512
#define MSG_SIZE   1024*50
    UINT success;
    int siz, nBytes;
    uint32_t buf[TEST_BUF_SIZE] = {0};
	uint8_t msg[MSG_SIZE];
    PUSB_COMMON_DESCRIPTOR    commonDesc      = NULL;
    PUSB_CONFIGURATION_DESCRIPTOR cd;
    BOOL  displayUnknown;

    siz = sizeof(buf);
#if 0
	for(int i=0;i<TEST_BUF_SIZE;i++){
		buf[i]=i;
	
	}
#endif

	
	switch(op){
		case 0: //exit dbg mode
            siz = 1;
		    success = DeviceIoControl(hDEV,
                    IOCTL_UDISP_EXIT_DBG,
                    msg,
                    siz,
                    msg,
                    siz,
                    (PULONG) &nBytes,
                    NULL);
		break;
		case 1:			
			success = DeviceIoControl(hDEV,
						IOCTL_UDISP_CONFIG,
						msg,
                         MSG_SIZE,
						msg,
						MSG_SIZE,
						(PULONG) &nBytes,
						NULL);
			printf("%s %d\n",msg, nBytes);
		break;
        case 2: {
            //
            lcd_colorbar(buf, 240, 240);
            siz = enc_jpg_enc(&msg[sizeof(udisp_frame_header_t)], (uint8_t*)buf, 0, 0, 239, 239, 239, MSG_SIZE);
            disp_setup_frame_header(msg, 0, 0, 239, 239, UDISP_TYPE_JPG, siz);
            siz += sizeof(udisp_frame_header_t);

            if (hDEV == INVALID_HANDLE_VALUE) {
                NOISY(("DEV not open"));
                return;
            }

            success = DeviceIoControl(hDEV,
                IOCTL_UDISP_ISSUE_URB,
                msg,
                siz,
                msg,
                siz,
                (PULONG)&nBytes,
                NULL);
        }
		break;
		default:
            success=0;
            nBytes = 0;
		}

    NOISY(("request complete, success = %u nBytes = %d\n", success, nBytes));

    if (success) {

      
    }
    else {
        printf("Ioct - GetConfigDesc failed %d\n", GetLastError());
    }

    return;
}


int  dumpUsbConfig()
/*++
Routine Description:

    Called to do formatted ascii dump to console of  USB
    configuration, interface, and endpoint descriptors
    (Cmdline "rwbulk -u" )

Arguments:

    none

Return Value:

    none

--*/
{
    HANDLE hDEV = open_dev();

    if (hDEV != INVALID_HANDLE_VALUE)
    {
        rw_dev( hDEV ,2);
        CloseHandle(hDEV);
    }

    return 0;

}
//  End, routines for USB configuration and pipe info dump  (Cmdline "rwbulk -u" )



int
_cdecl
main(
    _In_ int   argc,
    _In_reads_(argc) char *argv[]
    )
/*++
Routine Description:

    Entry point to rwbulk.exe
    Parses cmdline, performs user-requested tests

Arguments:

    argc, argv  standard console  'c' app arguments

Return Value:

    Zero

--*/

{
    char * pinBuf = NULL;
    char * poutBuf = NULL;
    int    nBytesRead;
    int    nBytesWrite;
    ULONG  i;
    ULONG  j;
    int    ok;
    UINT   success;
    HANDLE hRead = INVALID_HANDLE_VALUE;
    HANDLE hWrite = INVALID_HANDLE_VALUE;
    ULONG  fail = 0L;

    parse(argc, argv );

    // dump USB configuation and pipe info
    if( fDumpUsbConfig ) {
            dumpUsbConfig();
    }

    // doing a read, write, or both test
    if ((fRead) || (fWrite)) {

        if (fRead) {
            //
            // open the output file
            //
            if ( fDumpReadData ) { // round size to sizeof ULONG for readable dumping

                while( ReadLen % sizeof( ULONG ) ) {
                    ReadLen++;
                }
            }



        }

        if (fWrite) {
            if ( fDumpReadData ) { // round size to sizeof ULONG for readable dumping
                while( WriteLen % sizeof( ULONG ) ) {
                    WriteLen++;
                }
            }



			HANDLE hDEV = open_dev();

		    if (hDEV != INVALID_HANDLE_VALUE)
		    {
		        rw_dev( hDEV ,WriteLen );//test case by write len
		        CloseHandle(hDEV);
		    }

        }

      

        if (pinBuf) {
            free(pinBuf);
        }

        if (poutBuf) {
            free(poutBuf);
        }

        // close devices if needed
        if(hRead != INVALID_HANDLE_VALUE)
                CloseHandle(hRead);

        if(hWrite != INVALID_HANDLE_VALUE)
                CloseHandle(hWrite);
    }

    return 0;
}
