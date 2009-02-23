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
 *        ioipc.c
 *
 * Abstract:
 *
 *        IO lwmsg IPC Implementaion
 *
 * Authors: Danilo Almeida (dalmeida@likewisesoftware.com)
 */

#include "iop.h"
#include "ntipcmsg.h"
#include "ntlogmacros.h"
#include "ioapi.h"
#include "ioipc.h"

static
VOID
IopIpcCleanupFileHandle(
    PVOID pHandle
    )
{
    IO_FILE_HANDLE fileHandle = (IO_FILE_HANDLE) pHandle;
    assert(fileHandle);
    if (fileHandle)
    {
        NTSTATUS status = IoCloseFile(fileHandle);
        if (status)
        {
            SMB_LOG_ERROR("failed to cleanup handle (status = 0x%08x)", status);
            assert(FALSE);
        }
    }
}

static
NTSTATUS
IopIpcRegisterFileHandle(
    IN LWMsgAssoc* pAssoc,
    IN IO_FILE_HANDLE FileHandle
    )
{
    NTSTATUS status = 0;
    status = NtIpcLWMsgStatusToNtStatus(lwmsg_assoc_register_handle(
                    pAssoc,
                    "IO_FILE_HANDLE",
                    FileHandle,
                    IopIpcCleanupFileHandle));
    return status;
}

static
NTSTATUS
IopIpcRetainFileHandle(
    IN LWMsgAssoc* pAssoc,
    IN IO_FILE_HANDLE FileHandle
    )
{
    NTSTATUS status = 0;
    status = NtIpcLWMsgStatusToNtStatus(lwmsg_assoc_retain_handle(
                    pAssoc,
                    FileHandle));
    assert(!status);
    return status;
}

static
NTSTATUS
IopIpcGetProcessSecurity(
    IN LWMsgAssoc* pAssoc,
    OUT uid_t* pUid,
    OUT gid_t* pGid
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    LWMsgSecurityToken* token = NULL;
    uid_t uid = (uid_t) -1;
    gid_t gid = (gid_t) -1;

    status = NtIpcLWMsgStatusToNtStatus(lwmsg_assoc_get_peer_security_token(pAssoc, &token));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    if (token == NULL || strcmp(lwmsg_security_token_get_type(token), "local"))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP_EE(EE);
    }

    status = NtIpcLWMsgStatusToNtStatus(lwmsg_local_token_get_eid(token, &uid, &gid));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

cleanup:
    *pUid = uid;
    *pGid = gid;

    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return status;
}

static
LWMsgStatus
IopIpcCreateFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_CREATE_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_CREATE_FILE_RESULT;
    PNT_IPC_MESSAGE_CREATE_FILE pMessage = (PNT_IPC_MESSAGE_CREATE_FILE) pRequest->object;
    PNT_IPC_MESSAGE_CREATE_FILE_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };
    IO_FILE_HANDLE fileHandle = NULL;
    IO_FILE_NAME fileName = { 0 };
    IO_CREATE_SECURITY_CONTEXT securityContext = { {0, 0}, NULL };
    PIO_ECP_LIST pEcpList = NULL;

    assert(messageType == pRequest->tag);

    status = IopIpcGetProcessSecurity(
                    pAssoc,
                    &securityContext.Process.Uid,
                    &securityContext.Process.Gid);
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_CREATE_FILE_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    securityContext.pAccessToken = pMessage->pSecurityToken;

    fileName = pMessage->FileName;

    if (pMessage->EcpCount)
    {
        ULONG ecpIndex = 0;

        pReply->Status = IoRtlEcpListAllocate(&pEcpList);
        GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);

        for (ecpIndex = 0; ecpIndex < pMessage->EcpCount; ecpIndex++)
        {
            pReply->Status = IoRtlEcpListInsert(
                                    pEcpList,
                                    pMessage->EcpList[ecpIndex].pszType,
                                    pMessage->EcpList[ecpIndex].pData,
                                    pMessage->EcpList[ecpIndex].Size,
                                    NULL);
            GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);
        }
    }

    // TODO -- SDs, etc.
    pReply->Status = IoCreateFile(
                            &fileHandle,
                            NULL,
                            &ioStatusBlock,
                            &securityContext,
                            &fileName,
                            NULL,
                            NULL,
                            pMessage->DesiredAccess,
                            pMessage->AllocationSize,
                            pMessage->FileAttributes,
                            pMessage->ShareAccess,
                            pMessage->CreateDisposition,
                            pMessage->CreateOptions,
                            pMessage->EaBuffer,
                            pMessage->EaLength,
                            pEcpList);

    // Register handle with lwmsg if it was created
    if (fileHandle)
    {
        status = IopIpcRegisterFileHandle(pAssoc, fileHandle);
        GOTO_CLEANUP_ON_STATUS_EE(status, EE);

        pReply->FileHandle = fileHandle;
        fileHandle = NULL;

        status = IopIpcRetainFileHandle(pAssoc, pReply->FileHandle);
        GOTO_CLEANUP_ON_STATUS_EE(status, EE);
    }

    pReply->Status = ioStatusBlock.Status;
    pReply->CreateResult = ioStatusBlock.CreateResult;

cleanup:

    if (status)
    {
        if (fileHandle)
        {
            IoCloseFile(fileHandle);
        }
    }

    IoRtlEcpListFree(&pEcpList);

    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcCloseFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_CLOSE_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_CLOSE_FILE_RESULT;
    PNT_IPC_MESSAGE_GENERIC_FILE pMessage = (PNT_IPC_MESSAGE_GENERIC_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT pReply = NULL;

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    pReply->Status = IoCloseFile(pMessage->FileHandle);
    if (!pReply->Status)
    {
        NtIpcUnregisterFileHandle(pAssoc, pMessage->FileHandle);
    }

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

// TODO -- ASK BRIAN ABOUT HANDLING on pResponse on error (wrt free).

LWMsgStatus
IopIpcReadFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_READ_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_READ_FILE_RESULT;
    PNT_IPC_MESSAGE_READ_FILE pMessage = (PNT_IPC_MESSAGE_READ_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    // TODO--make sure allocate handles 0...
    pReply->Status = IO_ALLOCATE(&pReply->Buffer, VOID, pMessage->Length);
    GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);

    pReply->Status = IoReadFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pReply->Buffer,
                            pMessage->Length,
                            pMessage->ByteOffset,
                            pMessage->Key);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcWriteFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_WRITE_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_WRITE_FILE_RESULT;
    PNT_IPC_MESSAGE_WRITE_FILE pMessage = (PNT_IPC_MESSAGE_WRITE_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    pReply->Status = IoWriteFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pMessage->Buffer,
                            pMessage->Length,
                            pMessage->ByteOffset,
                            pMessage->Key);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcDeviceIoControlFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_DEVICE_IO_CONTROL_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_DEVICE_IO_CONTROL_FILE_RESULT;
    PNT_IPC_MESSAGE_GENERIC_CONTROL_FILE pMessage = (PNT_IPC_MESSAGE_GENERIC_CONTROL_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    // TODO--make sure allocate handles 0...
    pReply->Status = IO_ALLOCATE(&pReply->Buffer, VOID, pMessage->OutputBufferLength);
    GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);

    pReply->Status = IoDeviceIoControlFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pMessage->ControlCode,
                            pMessage->InputBuffer,
                            pMessage->InputBufferLength,
                            pReply->Buffer,
                            pMessage->OutputBufferLength);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcFsControlFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_FS_CONTROL_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_FS_CONTROL_FILE_RESULT;
    PNT_IPC_MESSAGE_GENERIC_CONTROL_FILE pMessage = (PNT_IPC_MESSAGE_GENERIC_CONTROL_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    // TODO--make sure allocate handles 0...
    if (pMessage->OutputBufferLength){
        pReply->Status = IO_ALLOCATE(&pReply->Buffer, VOID, pMessage->OutputBufferLength);
        GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);
    }

    pReply->Status = IoFsControlFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pMessage->ControlCode,
                            pMessage->InputBuffer,
                            pMessage->InputBufferLength,
                            pReply->Buffer,
                            pMessage->OutputBufferLength);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcFlushBuffersFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_FLUSH_BUFFERS_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_FLUSH_BUFFERS_FILE_RESULT;
    PNT_IPC_MESSAGE_GENERIC_FILE pMessage = (PNT_IPC_MESSAGE_GENERIC_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    pReply->Status = IoFlushBuffersFile(pMessage->FileHandle, NULL, &ioStatusBlock);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcQueryInformationFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_QUERY_INFORMATION_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_QUERY_INFORMATION_FILE_RESULT;
    PNT_IPC_MESSAGE_QUERY_INFORMATION_FILE pMessage = (PNT_IPC_MESSAGE_QUERY_INFORMATION_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    // TODO--make sure allocate handles 0...
    pReply->Status = IO_ALLOCATE(&pReply->Buffer, VOID, pMessage->Length);
    GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);

    pReply->Status = IoQueryInformationFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pReply->Buffer,
                            pMessage->Length,
                            pMessage->FileInformationClass);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcSetInformationFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_SET_INFORMATION_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_SET_INFORMATION_FILE_RESULT;
    PNT_IPC_MESSAGE_SET_INFORMATION_FILE pMessage = (PNT_IPC_MESSAGE_SET_INFORMATION_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_IO_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    pReply->Status = IoSetInformationFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pMessage->FileInformation,
                            pMessage->Length,
                            pMessage->FileInformationClass);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

LWMsgStatus
IopIpcQueryDirectoryFile(
    IN LWMsgAssoc* pAssoc,
    IN const LWMsgMessage* pRequest,
    OUT LWMsgMessage* pResponse,
    IN void* pData
    )
{
    NTSTATUS status = 0;
    int EE = 0;
    const LWMsgMessageTag messageType = NT_IPC_MESSAGE_TYPE_QUERY_DIRECTORY_FILE;
    const LWMsgMessageTag replyType = NT_IPC_MESSAGE_TYPE_QUERY_DIRECTORY_FILE_RESULT;
    PNT_IPC_MESSAGE_QUERY_DIRECTORY_FILE pMessage = (PNT_IPC_MESSAGE_QUERY_DIRECTORY_FILE) pRequest->object;
    PNT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT pReply = NULL;
    IO_STATUS_BLOCK ioStatusBlock = { 0 };

    assert(messageType == pRequest->tag);

    status = IO_ALLOCATE(&pReply, NT_IPC_MESSAGE_GENERIC_FILE_BUFFER_RESULT, sizeof(*pReply));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

    pResponse->tag = replyType;
    pResponse->object = pReply;

    // TODO--make sure allocate handles 0...
    pReply->Status = IO_ALLOCATE(&pReply->Buffer, VOID, pMessage->Length);
    GOTO_CLEANUP_ON_STATUS_EE(pReply->Status, EE);

    pReply->Status = IoQueryDirectoryFile(
                            pMessage->FileHandle,
                            NULL,
                            &ioStatusBlock,
                            pReply->Buffer,
                            pMessage->Length,
                            pMessage->FileInformationClass,
                            pMessage->ReturnSingleEntry,
                            pMessage->FileSpec,
                            pMessage->RestartScan);
    pReply->Status = ioStatusBlock.Status;
    pReply->BytesTransferred = ioStatusBlock.BytesTransferred;

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return NtIpcNtStatusToLWMsgStatus(status);
}

static
LWMsgDispatchSpec gIopIpcDispatchSpec[] =
{
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_CREATE_FILE,        IopIpcCreateFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_CLOSE_FILE,         IopIpcCloseFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_READ_FILE,          IopIpcReadFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_WRITE_FILE,         IopIpcWriteFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_DEVICE_IO_CONTROL_FILE, IopIpcDeviceIoControlFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_FS_CONTROL_FILE,        IopIpcFsControlFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_FLUSH_BUFFERS_FILE,     IopIpcFlushBuffersFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_QUERY_INFORMATION_FILE, IopIpcQueryInformationFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_SET_INFORMATION_FILE,   IopIpcSetInformationFile),
    LWMSG_DISPATCH(NT_IPC_MESSAGE_TYPE_QUERY_DIRECTORY_FILE,   IopIpcQueryDirectoryFile),
    LWMSG_DISPATCH_END
};

NTSTATUS
IoIpcAddProtocolSpec(
    IN OUT LWMsgProtocol* pProtocol
    )
{
    return NtIpcAddProtocolSpec(pProtocol);
}

NTSTATUS
IoIpcAddDispatch(
    IN OUT LWMsgServer* pServer
    )
{
    NTSTATUS status = 0;
    int EE = 0;

    status = NtIpcLWMsgStatusToNtStatus(lwmsg_server_add_dispatch_spec(
                    pServer,
                    gIopIpcDispatchSpec));
    GOTO_CLEANUP_ON_STATUS_EE(status, EE);

cleanup:
    LOG_LEAVE_IF_STATUS_EE(status, EE);
    return status;
}

