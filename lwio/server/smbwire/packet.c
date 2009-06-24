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
 *        session_setup.c
 *
 * Abstract:
 *
 *        Likewise SMB Subsystem (LWIO)
 *
 *        SMB Packet Marshalling
 *
 * Author: Kaya Bekiroglu (kaya@likewisesoftware.com)
 *
 * @todo: add error logging code
 * @todo: switch to NT error codes where appropriate
 */

#include "includes.h"

static uchar8_t smbMagic[4] = { 0xFF, 'S', 'M', 'B' };

BOOLEAN
SMBIsAndXCommand(
    uint8_t command
    )
{
    BOOLEAN bResult = FALSE;

    switch(command)
    {
        case COM_LOCKING_ANDX:
        case COM_OPEN_ANDX:
        case COM_READ_ANDX:
        case COM_WRITE_ANDX:
        case COM_SESSION_SETUP_ANDX:
        case COM_LOGOFF_ANDX:
        case COM_TREE_CONNECT_ANDX:
        case COM_NT_CREATE_ANDX:
        {
            bResult = TRUE;
            break;
        }
    }

    return bResult;
}

NTSTATUS
SMBPacketCreateAllocator(
    IN ULONG ulNumMaxPackets,
    OUT PLWIO_PACKET_ALLOCATOR* phPacketAllocator
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_PACKET_ALLOCATOR pPacketAllocator = NULL;

    if (!ulNumMaxPackets || ulNumMaxPackets < 10)
    {
        ulNumMaxPackets = 10;
    }

    ntStatus = SMBAllocateMemory(
                    sizeof(LWIO_PACKET_ALLOCATOR),
                    (PVOID*)&pPacketAllocator);
    BAIL_ON_NT_STATUS(ntStatus);

    pthread_mutex_init(&pPacketAllocator->mutex, NULL);
    pPacketAllocator->pMutex = &pPacketAllocator->mutex;
    pPacketAllocator->ulNumMaxPackets = ulNumMaxPackets;

    *phPacketAllocator = (HANDLE)pPacketAllocator;

cleanup:

    return ntStatus;

error:

    *phPacketAllocator = NULL;

    goto cleanup;
}

NTSTATUS
SMBPacketAllocate(
    IN PLWIO_PACKET_ALLOCATOR hPacketAllocator,
    OUT PSMB_PACKET* ppPacket
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_PACKET_ALLOCATOR pPacketAllocator = NULL;
    BOOLEAN bInLock = FALSE;
    PSMB_PACKET pPacket = NULL;

    pPacketAllocator = hPacketAllocator;

    LWIO_LOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    if (pPacketAllocator->pFreePacketStack)
    {
        pPacket = (PSMB_PACKET) pPacketAllocator->pFreePacketStack;

        SMBStackPopNoFree(&pPacketAllocator->pFreePacketStack);

        pPacketAllocator->freePacketCount--;
    }
    else
    {
        ntStatus = SMBAllocateMemory(
                        sizeof(SMB_PACKET),
                        (PVOID *) &pPacket);
        BAIL_ON_NT_STATUS(ntStatus);
    }

    *ppPacket = pPacket;

cleanup:

    LWIO_UNLOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    return ntStatus;

error:

    *ppPacket = NULL;

    goto cleanup;
}

VOID
SMBPacketFree(
    IN PLWIO_PACKET_ALLOCATOR hPacketAllocator,
    IN OUT PSMB_PACKET pPacket
    )
{
    PLWIO_PACKET_ALLOCATOR pPacketAllocator = NULL;
    BOOLEAN bInLock = FALSE;

    if (pPacket->pRawBuffer)
    {
        SMBPacketBufferFree(
            hPacketAllocator,
            pPacket->pRawBuffer,
            pPacket->bufferLen);
    }

    pPacketAllocator = hPacketAllocator;

    LWIO_LOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    /* If the len is greater than our current allocator len, adjust */
    /* @todo: make free list configurable */
    if (pPacketAllocator->freePacketCount < pPacketAllocator->ulNumMaxPackets)
    {
        assert(sizeof(SMB_PACKET) > sizeof(SMB_STACK));
        SMBStackPushNoAlloc(&pPacketAllocator->pFreePacketStack, (PSMB_STACK) pPacket);
        pPacketAllocator->freePacketCount++;
    }
    else
    {
        SMBFreeMemory(pPacket);
    }

    LWIO_UNLOCK_MUTEX(bInLock, &pPacketAllocator->mutex);
}

NTSTATUS
SMBPacketBufferAllocate(
    IN PLWIO_PACKET_ALLOCATOR hPacketAllocator,
    IN size_t len,
    OUT uint8_t** ppBuffer,
    OUT size_t* pAllocatedLen
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_PACKET_ALLOCATOR pPacketAllocator = NULL;
    BOOLEAN bInLock = FALSE;

    pPacketAllocator = hPacketAllocator;

    LWIO_LOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    /* If the len is greater than our current allocator len, adjust */
    if (len > pPacketAllocator->freeBufferLen)
    {
        SMBStackFree(pPacketAllocator->pFreeBufferStack);
        pPacketAllocator->pFreeBufferStack = NULL;

        pPacketAllocator->freeBufferLen = len;
    }

    if (pPacketAllocator->pFreeBufferStack)
    {
        *ppBuffer = (uint8_t *) pPacketAllocator->pFreeBufferStack;
        *pAllocatedLen = pPacketAllocator->freeBufferLen;
        SMBStackPopNoFree(&pPacketAllocator->pFreeBufferStack);
        memset(*ppBuffer, 0, *pAllocatedLen);
        pPacketAllocator->freeBufferCount--;
    }
    else
    {
        ntStatus = SMBAllocateMemory(
                        pPacketAllocator->freeBufferLen,
                        (PVOID *) ppBuffer);
        BAIL_ON_NT_STATUS(ntStatus);

        *pAllocatedLen = pPacketAllocator->freeBufferLen;
    }

cleanup:

    LWIO_UNLOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    return ntStatus;

error:

    goto cleanup;
}

VOID
SMBPacketBufferFree(
    IN PLWIO_PACKET_ALLOCATOR hPacketAllocator,
    OUT uint8_t* pBuffer,
    IN size_t bufferLen
    )
{
    BOOLEAN bInLock = FALSE;
    PLWIO_PACKET_ALLOCATOR pPacketAllocator = NULL;

    pPacketAllocator = hPacketAllocator;

    LWIO_LOCK_MUTEX(bInLock, &pPacketAllocator->mutex);

    /* If the len is greater than our current allocator len, adjust */
    /* @todo: make free list configurable */
    if (bufferLen == pPacketAllocator->freeBufferLen &&
        pPacketAllocator->freeBufferCount < pPacketAllocator->ulNumMaxPackets)
    {
        assert(bufferLen > sizeof(SMB_STACK));

        SMBStackPushNoAlloc(&pPacketAllocator->pFreeBufferStack, (PSMB_STACK) pBuffer);

        pPacketAllocator->freeBufferCount++;
    }
    else
    {
        SMBFreeMemory(pBuffer);
    }

    LWIO_UNLOCK_MUTEX(bInLock, &pPacketAllocator->mutex);
}

VOID
SMBPacketResetBuffer(
    PSMB_PACKET pPacket
    )
{
    if (pPacket->pRawBuffer && pPacket->bufferLen)
    {
        memset(pPacket->pRawBuffer, 0, pPacket->bufferLen);
    }
}

VOID
SMBPacketFreeAllocator(
    IN OUT PLWIO_PACKET_ALLOCATOR hPacketAllocator
    )
{
    PLWIO_PACKET_ALLOCATOR pAllocator = hPacketAllocator;

    if (pAllocator->pMutex)
    {
        pthread_mutex_destroy(&pAllocator->mutex);
        pAllocator->pMutex = NULL;
    }

    if (pAllocator->pFreeBufferStack)
    {
        SMBStackFree(pAllocator->pFreeBufferStack);
    }

    if (pAllocator->pFreePacketStack)
    {
        SMBStackFree(pAllocator->pFreePacketStack);
    }

    SMBFreeMemory(pAllocator);
}

static
NTSTATUS
ConsumeBuffer(
    IN PVOID pBuffer,
    IN uint32_t BufferLength,
    IN OUT uint32_t* BufferLengthUsed,
    IN uint32_t BytesNeeded
    )
{
    NTSTATUS ntStatus = 0;

    if (*BufferLengthUsed + BytesNeeded > BufferLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        *BufferLengthUsed += BytesNeeded;
    }

    return ntStatus;
}

VOID
SMBPacketHTOLSmbHeader(
    IN OUT SMB_HEADER* pHeader
    )
{
    SMB_HTOL32_INPLACE(pHeader->error);
    SMB_HTOL16_INPLACE(pHeader->flags2);
    SMB_HTOL16_INPLACE(pHeader->extra.pidHigh);
    SMB_HTOL16_INPLACE(pHeader->tid);
    SMB_HTOL16_INPLACE(pHeader->pid);
    SMB_HTOL16_INPLACE(pHeader->uid);
    SMB_HTOL16_INPLACE(pHeader->mid);
}

static
VOID
SMBPacketLTOHSmbHeader(
    IN OUT SMB_HEADER* pHeader
    )
{
    SMB_LTOH32_INPLACE(pHeader->error);
    SMB_LTOH16_INPLACE(pHeader->flags2);
    SMB_LTOH16_INPLACE(pHeader->extra.pidHigh);
    SMB_LTOH16_INPLACE(pHeader->tid);
    SMB_LTOH16_INPLACE(pHeader->pid);
    SMB_LTOH16_INPLACE(pHeader->uid);
    SMB_LTOH16_INPLACE(pHeader->mid);
}

/* @todo: support AndX */
NTSTATUS
SMBPacketMarshallHeader(
    uint8_t    *pBuffer,
    uint32_t    bufferLen,
    uint8_t     command,
    uint32_t    error,
    uint32_t    isResponse,
    uint16_t    tid,
    uint32_t    pid,
    uint16_t    uid,
    uint16_t    mid,
    BOOLEAN     bCommandAllowsSignature,
    PSMB_PACKET pPacket
    )
{
    NTSTATUS ntStatus = 0;
    uint32_t bufferUsed = 0;
    SMB_HEADER* pHeader = NULL;

    pPacket->allowSignature = bCommandAllowsSignature;

    pPacket->pNetBIOSHeader = (NETBIOS_HEADER *) (pBuffer + bufferUsed);

    ntStatus = ConsumeBuffer(pBuffer, bufferLen, &bufferUsed, sizeof(NETBIOS_HEADER));
    BAIL_ON_NT_STATUS(ntStatus);

    pPacket->pSMBHeader = (SMB_HEADER *) (pBuffer + bufferUsed);

    ntStatus = ConsumeBuffer(pBuffer, bufferLen, &bufferUsed, sizeof(SMB_HEADER));
    BAIL_ON_NT_STATUS(ntStatus);

    pHeader = pPacket->pSMBHeader;
    memcpy(&pHeader->smb, smbMagic, sizeof(smbMagic));
    pHeader->command = command;
    pHeader->error = error;
    pHeader->flags = isResponse ? FLAG_RESPONSE : 0;
    pHeader->flags |= FLAG_CASELESS_PATHS | FLAG_OBSOLETE_2;
    pHeader->flags2 = ((isResponse ? 0 : FLAG2_KNOWS_LONG_NAMES) |
                       (isResponse ? 0 : FLAG2_IS_LONG_NAME) |
                       FLAG2_KNOWS_EAS | FLAG2_EXT_SEC |
                       FLAG2_ERR_STATUS | FLAG2_UNICODE);
    memset(pHeader->pad, 0, sizeof(pHeader->pad));
    pHeader->extra.pidHigh = pid >> 16;
    pHeader->tid = tid;
    pHeader->pid = pid;
    pHeader->uid = uid;
    pHeader->mid = mid;

    if (SMBIsAndXCommand(command))
    {
        pPacket->pAndXHeader = (ANDX_HEADER *) (pBuffer + bufferUsed);

        ntStatus = ConsumeBuffer(pBuffer, bufferLen, &bufferUsed, sizeof(ANDX_HEADER));
        BAIL_ON_NT_STATUS(ntStatus);

        pPacket->pAndXHeader->andXCommand = 0xFF;
        pPacket->pAndXHeader->andXOffset = 0;
        pPacket->pAndXHeader->andXReserved = 0;
    }
    else
    {
        pPacket->pAndXHeader = NULL;
    }

    pPacket->pParams = pBuffer + bufferUsed;
    pPacket->pData = NULL;
    pPacket->bufferLen = bufferLen;
    pPacket->bufferUsed = bufferUsed;

    assert(bufferUsed <= bufferLen);

cleanup:
    return ntStatus;

error:
    pPacket->pNetBIOSHeader = NULL;
    pPacket->pSMBHeader = NULL;
    pPacket->pAndXHeader = NULL;
    pPacket->pParams = NULL;
    pPacket->pData = NULL;
    pPacket->bufferLen = bufferLen;
    pPacket->bufferUsed = 0;

    goto cleanup;
}

NTSTATUS
SMBPacketMarshallFooter(
    PSMB_PACKET pPacket
    )
{
    pPacket->pNetBIOSHeader->len = htonl(pPacket->bufferUsed - sizeof(NETBIOS_HEADER));

    return 0;
}

BOOLEAN
SMBPacketIsSigned(
    PSMB_PACKET pPacket
    )
{
    return (pPacket->pSMBHeader->flags2 & FLAG2_SECURITY_SIG);
}

BOOLEAN
SMB2PacketIsSigned(
    PSMB_PACKET pPacket
    )
{
    return (pPacket->pSMB2Header->ulFlags & SMB2_FLAGS_SIGNED);
}

NTSTATUS
SMBPacketVerifySignature(
    PSMB_PACKET pPacket,
    ULONG       ulExpectedSequence,
    PBYTE       pSessionKey,
    ULONG       ulSessionKeyLength
    )
{
    NTSTATUS ntStatus = 0;
    uint8_t digest[16];
    uint8_t origSignature[8];
    MD5_CTX md5Value;
    uint32_t littleEndianSequence = SMB_HTOL32(ulExpectedSequence);

    assert (sizeof(origSignature) == sizeof(pPacket->pSMBHeader->extra.securitySignature));

    memcpy(origSignature, pPacket->pSMBHeader->extra.securitySignature, sizeof(pPacket->pSMBHeader->extra.securitySignature));
    memset(&pPacket->pSMBHeader->extra.securitySignature[0], 0, sizeof(pPacket->pSMBHeader->extra.securitySignature));
    memcpy(&pPacket->pSMBHeader->extra.securitySignature[0], &littleEndianSequence, sizeof(littleEndianSequence));

    MD5_Init(&md5Value);

    if (pSessionKey)
    {
        MD5_Update(&md5Value, pSessionKey, ulSessionKeyLength);
    }

    MD5_Update(&md5Value, (PBYTE)pPacket->pSMBHeader, pPacket->pNetBIOSHeader->len);
    MD5_Final(digest, &md5Value);

    if (memcmp(&origSignature[0], &digest[0], sizeof(origSignature)))
    {
        ntStatus = STATUS_INVALID_NETWORK_RESPONSE;
    }

    // restore signature
    memcpy(&pPacket->pSMBHeader->extra.securitySignature[0], &origSignature[0], sizeof(origSignature));

    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    return ntStatus;

error:

    LWIO_LOG_WARNING("SMB Packet verification failed (status = 0x%08X)", ntStatus);

    goto cleanup;
}

NTSTATUS
SMB2PacketVerifySignature(
    PSMB_PACKET pPacket,
    PBYTE       pSessionKey,
    ULONG       ulSessionKeyLength
    )
{
    NTSTATUS ntStatus = 0;
    uint8_t  digest[32];
    uint8_t  origSignature[16];
    SHA256_CTX sha256Value;

    memcpy(origSignature, pPacket->pSMB2Header->signature, sizeof(pPacket->pSMB2Header->signature));
    memset(&pPacket->pSMB2Header->signature[0], 0, sizeof(pPacket->pSMB2Header->signature));

    SHA256_Init(&sha256Value);

    if (pSessionKey)
    {
        SHA256_Update(&sha256Value, pSessionKey, ulSessionKeyLength);
    }

    SHA256_Update(&sha256Value, (PBYTE)pPacket->pSMB2Header, pPacket->pNetBIOSHeader->len);
    SHA256_Final(digest, &sha256Value);

    if (memcmp(&origSignature[0], &digest[0], sizeof(origSignature)))
    {
        ntStatus = STATUS_INVALID_NETWORK_RESPONSE;
    }

    // restore signature
    memcpy(&pPacket->pSMB2Header->signature[0], &origSignature[0], sizeof(origSignature));

    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    return ntStatus;

error:

    LWIO_LOG_WARNING("SMB2 Packet verification failed (status = 0x%08X)", ntStatus);

    goto cleanup;
}

NTSTATUS
SMBPacketDecodeHeader(
    IN OUT PSMB_PACKET pPacket,
    IN BOOLEAN bVerifySignature,
    IN DWORD dwExpectedSequence,
    IN OPTIONAL PBYTE pSessionKey,
    IN DWORD dwSessionKeyLength
    )
{
    NTSTATUS ntStatus = 0;

    if (bVerifySignature)
    {
        ntStatus = SMBPacketVerifySignature(
                        pPacket,
                        dwExpectedSequence,
                        pSessionKey,
                        dwSessionKeyLength);
        BAIL_ON_NT_STATUS(ntStatus);
    }

    SMBPacketLTOHSmbHeader(pPacket->pSMBHeader);

error:
    return ntStatus;
}

NTSTATUS
SMBPacketSign(
    PSMB_PACKET pPacket,
    ULONG       ulSequence,
    PBYTE       pSessionKey,
    ULONG       ulSessionKeyLength
    )
{
    NTSTATUS ntStatus = 0;
    uint8_t digest[16];
    MD5_CTX md5Value;
    uint32_t littleEndianSequence = SMB_HTOL32(ulSequence);

    memset(&pPacket->pSMBHeader->extra.securitySignature[0], 0, sizeof(pPacket->pSMBHeader->extra.securitySignature));
    memcpy(&pPacket->pSMBHeader->extra.securitySignature[0], &littleEndianSequence, sizeof(littleEndianSequence));

    MD5_Init(&md5Value);

    if (pSessionKey)
    {
        MD5_Update(&md5Value, pSessionKey, ulSessionKeyLength);
    }

    MD5_Update(&md5Value, (PBYTE)pPacket->pSMBHeader, ntohl(pPacket->pNetBIOSHeader->len));
    MD5_Final(digest, &md5Value);

    memcpy(&pPacket->pSMBHeader->extra.securitySignature[0], &digest[0], sizeof(pPacket->pSMBHeader->extra.securitySignature));

    return ntStatus;
}

NTSTATUS
SMB2PacketSign(
    PSMB_PACKET pPacket,
    PBYTE       pSessionKey,
    ULONG       ulSessionKeyLength
    )
{
    NTSTATUS ntStatus = 0;
    uint8_t digest[32];
    SHA256_CTX sha256Value;

    memset(&pPacket->pSMB2Header->signature[0], 0, sizeof(pPacket->pSMB2Header->signature));

    SHA256_Init(&sha256Value);

    if (pSessionKey)
    {
        SHA256_Update(&sha256Value, pSessionKey, ulSessionKeyLength);
    }

    SHA256_Update(&sha256Value, (PBYTE)pPacket->pSMB2Header, ntohl(pPacket->pNetBIOSHeader->len));

    SHA256_Final(digest, &sha256Value);

    memcpy(&pPacket->pSMB2Header->signature[0], &digest[0], sizeof(pPacket->pSMB2Header->signature));

    pPacket->pSMB2Header->ulFlags |= SMB2_FLAGS_SIGNED;

    return ntStatus;
}

NTSTATUS
SMBPacketUpdateAndXOffset(
    PSMB_PACKET pPacket
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (!SMBIsAndXCommand(pPacket->pSMBHeader->command)) {
        /* No op */
        return STATUS_SUCCESS;
    }

    pPacket->pAndXHeader->andXOffset = pPacket->bufferUsed - sizeof(NETBIOS_HEADER);

    return ntStatus;
}

NTSTATUS
SMBPacketAppendUnicodeString(
    OUT PBYTE pBuffer,
    IN ULONG BufferLength,
    IN OUT PULONG BufferUsed,
    IN PCWSTR pwszString
    )
{
    NTSTATUS ntStatus = 0;
    ULONG bytesNeeded = 0;
    ULONG bufferUsed = *BufferUsed;
    wchar16_t* pOutputBuffer = NULL;
    size_t writeLength = 0;

    bytesNeeded = sizeof(pwszString[0]) * (wc16slen(pwszString) + 1);
    if (bufferUsed + bytesNeeded > BufferLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    pOutputBuffer = (wchar16_t *) (pBuffer + bufferUsed);
    writeLength = wc16stowc16les(pOutputBuffer, pwszString, bytesNeeded / sizeof(pwszString[0]));
    // Verify that expected write length was returned.  Note that the
    // returned length does not include the NULL though the NULL gets
    // written out.
    if (writeLength == (size_t) -1)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        BAIL_ON_NT_STATUS(ntStatus);
    }
    else if (((writeLength + 1) * sizeof(wchar16_t)) != bytesNeeded)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    bufferUsed += bytesNeeded;

error:
    *BufferUsed = bufferUsed;
    return ntStatus;
}

NTSTATUS
SMBPacketAppendString(
    OUT PBYTE pBuffer,
    IN ULONG BufferLength,
    IN OUT PULONG BufferUsed,
    IN PCSTR pszString
    )
{
    NTSTATUS ntStatus = 0;
    ULONG bytesNeeded = 0;
    ULONG bufferUsed = *BufferUsed;
    char* pOutputBuffer = NULL;
    char* pszCursor = NULL;

    bytesNeeded = sizeof(pszString[0]) * (strlen(pszString) + 1);
    if (bufferUsed + bytesNeeded > BufferLength)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    pOutputBuffer = (char *) (pBuffer + bufferUsed);

    pszCursor = lsmb_stpncpy(pOutputBuffer, pszString, bytesNeeded / sizeof(pszString[0]));
    *pszCursor = 0;
    if ((pszCursor - pOutputBuffer) != (bytesNeeded - 1))
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    bufferUsed += bytesNeeded;

error:
    *BufferUsed = bufferUsed;
    return ntStatus;
}

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
