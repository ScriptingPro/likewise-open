/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.  You should have received a copy of the GNU General
 * Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */



/*
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        driver.c
 *
 * Abstract:
 *
 *        Likewise Posix File System Driver (PVFS)
 *
 *        Driver Entry Function
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Danilo Almeida (dalmeida@likewisesoftware.com)
 */

#include "rdr.h"


VOID
RdrDriverShutdown(
    IN IO_DRIVER_HANDLE DriverHandle
    )
{
    IO_LOG_ENTER_LEAVE("");
}

NTSTATUS
RdrDriverDispatch(
    IN IO_DEVICE_HANDLE DeviceHandle,
    IN PIRP pIrp
    )
{
    NTSTATUS ntStatus = 0;
    int EE = 0;

    switch (pIrp->Type)
    {
        case IRP_TYPE_CREATE:
            ntStatus = RdrCreate(
                            DeviceHandle,
                            pIrp
                            );
            break;

        case IRP_TYPE_CLOSE:
            ntStatus = RdrClose(
                            DeviceHandle,
                            pIrp
                            );
            break;


        case IRP_TYPE_READ:
            ntStatus = RdrRead(
                            DeviceHandle,
                            pIrp
                            );
             break;

        case IRP_TYPE_WRITE:
            ntStatus = RdrWrite(
                            DeviceHandle,
                            pIrp
                            );
            break;

        case IRP_TYPE_DEVICE_IO_CONTROL:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;

        case IRP_TYPE_FS_CONTROL:
            ntStatus = RdrFsctl(
                            DeviceHandle,
                            pIrp
                            );
            break;
        case IRP_TYPE_FLUSH_BUFFERS:
            ntStatus = STATUS_NOT_IMPLEMENTED;
            break;
        case IRP_TYPE_QUERY_INFORMATION:
            ntStatus = RdrQueryInformation(
                            DeviceHandle,
                            pIrp
                            );
            break;
        case IRP_TYPE_SET_INFORMATION:
            ntStatus = RdrSetInformation(
                            DeviceHandle,
                            pIrp
                            );
            break;
    default:
        ntStatus = STATUS_UNSUCCESSFUL;
        GOTO_CLEANUP_ON_STATUS_EE(ntStatus, EE);
    }

cleanup:
    IO_LOG_ENTER_LEAVE_STATUS_EE(ntStatus, EE);
    return ntStatus;
}

NTSTATUS
DriverEntry(
    IN IO_DRIVER_HANDLE DriverHandle,
    IN ULONG InterfaceVersion
    )
{
    NTSTATUS ntStatus = 0;
    int EE = 0;
    IO_DEVICE_HANDLE deviceHandle = NULL;

    if (IO_DRIVER_ENTRY_INTERFACE_VERSION != InterfaceVersion)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        GOTO_CLEANUP_ON_STATUS_EE(ntStatus, EE);
    }

    ntStatus = IoDriverInitialize(DriverHandle,
                                  NULL,
                                  RdrDriverShutdown,
                                  RdrDriverDispatch);
    GOTO_CLEANUP_ON_STATUS_EE(ntStatus, EE);

    ntStatus = IoDeviceCreate(&deviceHandle,
                              DriverHandle,
                              "rdr",
                              NULL);
    GOTO_CLEANUP_ON_STATUS_EE(ntStatus, EE);

cleanup:
    IO_LOG_ENTER_LEAVE_STATUS_EE(ntStatus, EE);
    return ntStatus;
}
