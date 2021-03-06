/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software    2004-2010
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        accesstoken.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        Access token handling functions
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 *          Sriram Nambakam  (snambakam@likewise.com)
 */

#include "includes.h"

static
DWORD
SrvSvcSrvInitNpAuthInfo(
    rpc_transport_info_handle_t hTransportInfo, /* IN     */
    PSRVSVC_SRV_CONTEXT         pSrvCtx         /* IN OUT */
    );

static
DWORD
SrvSvcSrvInitLpcAuthInfo(
    rpc_transport_info_handle_t hTransportInfo, /* IN     */
    PSRVSVC_SRV_CONTEXT         pSrvCtx         /* IN OUT */
    );

DWORD
SrvSvcSrvInitAuthInfo(
    handle_t            hBinding, /* IN     */
    PSRVSVC_SRV_CONTEXT pSrvCtx   /* IN OUT */
    )
{
    DWORD dwError = ERROR_SUCCESS;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    unsigned32 rpcStatus = 0;
    rpc_transport_info_handle_t hTransportInfo = NULL;
    DWORD dwProtSeq = rpc_c_invalid_protseq_id;

    rpc_binding_inq_access_token_caller(
        hBinding,
        &pSrvCtx->pUserToken,
        &rpcStatus);

    if (rpcStatus)
    {
        ntStatus = LwRpcStatusToNtStatus(rpcStatus);
        BAIL_ON_NT_STATUS(ntStatus);
    }

    rpc_binding_inq_transport_info(hBinding,
                                   &hTransportInfo,
                                   &rpcStatus);

    if (rpcStatus)
    {
        ntStatus = LwRpcStatusToNtStatus(rpcStatus);
        BAIL_ON_NT_STATUS(ntStatus);
    }

    if (hTransportInfo)
    {
        rpc_binding_inq_prot_seq(hBinding,
                                 (unsigned32*)&dwProtSeq,
                                 &rpcStatus);
        if (rpcStatus)
        {
            ntStatus = LwRpcStatusToNtStatus(rpcStatus);
            BAIL_ON_NT_STATUS(ntStatus);
        }

        switch (dwProtSeq)
        {
        case rpc_c_protseq_id_ncacn_np:
            ntStatus = SrvSvcSrvInitNpAuthInfo(hTransportInfo,
                                             pSrvCtx);
            break;

        case rpc_c_protseq_id_ncalrpc:
            ntStatus = SrvSvcSrvInitLpcAuthInfo(hTransportInfo,
                                              pSrvCtx);
            break;
        }
        BAIL_ON_NT_STATUS(ntStatus);
    }

cleanup:

    if (ntStatus != STATUS_SUCCESS)
    {
        dwError = LwNtStatusToWin32Error(ntStatus);
    }

    return dwError;

error:

    if (pSrvCtx)
    {
        SrvSvcSrvFreeAuthInfoContents(pSrvCtx);
    }

    goto cleanup;
}


static
DWORD
SrvSvcSrvInitNpAuthInfo(
    rpc_transport_info_handle_t hTransportInfo, /* IN     */
    PSRVSVC_SRV_CONTEXT         pSrvCtx         /* IN OUT */
    )
{
    DWORD dwError = ERROR_SUCCESS;
    PUCHAR pucSessionKey = NULL;
    unsigned16 SessionKeyLen = 0;
    PBYTE pSessionKey = NULL;
    DWORD dwSessionKeyLen = 0;

    rpc_smb_transport_info_inq_session_key(
                                   hTransportInfo,
                                   (unsigned char**)&pucSessionKey,
                                   &SessionKeyLen);

    dwSessionKeyLen = SessionKeyLen;
    if (dwSessionKeyLen)
    {
        dwError = LwAllocateMemory(dwSessionKeyLen,
                                   OUT_PPVOID(&pSessionKey));
        BAIL_ON_SRVSVC_ERROR(dwError);

        memcpy(pSessionKey, pucSessionKey, dwSessionKeyLen);
    }

    pSrvCtx->pSessionKey     = pSessionKey;
    pSrvCtx->dwSessionKeyLen = dwSessionKeyLen;

error:

    return dwError;
}


static
DWORD
SrvSvcSrvInitLpcAuthInfo(
    rpc_transport_info_handle_t hTransportInfo, /* IN     */
    PSRVSVC_SRV_CONTEXT         pSrvCtx         /* IN OUT */
    )
{
    DWORD dwError = ERROR_SUCCESS;
    PUCHAR pucSessionKey = NULL;
    unsigned16 SessionKeyLen = 0;
    PBYTE pSessionKey = NULL;
    DWORD dwSessionKeyLen = 0;

    rpc_lrpc_transport_info_inq_session_key(
                                   hTransportInfo,
                                   (unsigned char**)&pucSessionKey,
                                   &SessionKeyLen);

    dwSessionKeyLen = SessionKeyLen;
    if (dwSessionKeyLen)
    {
        dwError = LwAllocateMemory(dwSessionKeyLen,
                                   OUT_PPVOID(&pSessionKey));
        BAIL_ON_SRVSVC_ERROR(dwError);

        memcpy(pSessionKey, pucSessionKey, dwSessionKeyLen);
    }

    pSrvCtx->pSessionKey     = pSessionKey;
    pSrvCtx->dwSessionKeyLen = dwSessionKeyLen;

error:

    return dwError;
}


VOID
SrvSvcSrvFreeAuthInfoContents(
    PSRVSVC_SRV_CONTEXT pSrvCtx /* IN OUT */
    )
{
    if (pSrvCtx->pUserToken)
    {
        RtlReleaseAccessToken(&pSrvCtx->pUserToken);
        pSrvCtx->pUserToken = NULL;
    }

    if (pSrvCtx->pSessionKey)
    {
        LW_SAFE_FREE_MEMORY(pSrvCtx->pSessionKey);
        pSrvCtx->pSessionKey     = NULL;
        pSrvCtx->dwSessionKeyLen = 0;
    }
}

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
