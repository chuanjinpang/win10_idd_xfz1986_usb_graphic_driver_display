/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    BulkUsr.h

Abstract:

Environment:

    User & Kernel mode

--*/

#ifndef _IOCTL_H
#define _IOCTL_H

#include <initguid.h>

// {6068EB61-98E7-4c98-9E20-1F068295909A}
DEFINE_GUID(GUID_DEVINTERFACE_UDISP1986, 


0x1986bf10, 0x6530, 0x11d2, 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed);


#define IOCTL_INDEX             0x0000


#define IOCTL_UDISP_CONFIG CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX+0,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)
                                                   
#define IOCTL_UDISP_ISSUE_URB          CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX + 1, \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#define IOCTL_UDISP_EXIT_DBG            CTL_CODE(FILE_DEVICE_UNKNOWN,     \
                                                     IOCTL_INDEX + 2, \
                                                     METHOD_BUFFERED,         \
                                                     FILE_ANY_ACCESS)

#endif
