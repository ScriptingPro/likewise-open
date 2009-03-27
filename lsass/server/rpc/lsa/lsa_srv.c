/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software    2004-2008
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
 * Abstract: Lsa rpc server management functions (rpc server library)
 *
 * Authors: Rafal Szczesniak (rafal@likewisesoftware.com)
 */

#include "includes.h"


DWORD
LsaInitializeRpcSrv(
    PCSTR pszConfigFilePath,
    PSTR* ppszRpcSrvName,
    PLSA_RPCSRV_FUNCTION_TABLE* ppFnTable
    )
{
    DWORD dwError = 0;
    NTSTATUS status = STATUS_SUCCESS;

    pthread_mutex_init(&gLsaSrvDataMutex, NULL);

    status = LsaSrvInitMemory();
    BAIL_ON_NTSTATUS_ERROR(status);

    dwError = RpcSvcRegisterRpcInterface(lsa_v0_0_s_ifspec);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszRpcSrvName = (PSTR)gpszRpcSrvName;
    *ppFnTable      = &gLsaRpcFuncTable;

    bLsaSrvInitialised = TRUE;

error:
    return dwError;
}


DWORD
LsaShutdownRpcSrv(
    PCSTR pszProviderName,
    PLSA_RPCSRV_FUNCTION_TABLE pFnTable
    )
{
    DWORD dwError = 0;
    NTSTATUS status = STATUS_SUCCESS;

    dwError = RpcSvcUnregisterRpcInterface(lsa_v0_0_s_ifspec);
    BAIL_ON_LSA_ERROR(dwError);

    status = LsaSrvDestroyMemory();
    BAIL_ON_NTSTATUS_ERROR(status);

    pthread_mutex_destroy(&gLsaSrvDataMutex);

    bLsaSrvInitialised = FALSE;

error:
    return dwError;
}


DWORD
LsaRpcStartServer(
    void
    )
{
    PCSTR pszDescription = "Local Security Authority";

    ENDPOINT EndPoints[] = {
        { "ncacn_np",      "\\\\pipe\\\\lsarpc" },
        { "ncacn_np",      "\\\\pipe\\\\lsass" },
        { "ncacn_ip_tcp",  NULL },
        { NULL,            NULL }
    };
    DWORD dwError = 0;

    dwError = RpcSvcBindRpcInterface(gpLsaSrvBinding,
                                     lsa_v0_0_s_ifspec,
                                     EndPoints,
                                     pszDescription);
    BAIL_ON_LSA_ERROR(dwError);

error:
    return dwError;
}


DWORD
LsaRpcStopServer(
    void
    )
{
    DWORD dwError = 0;

    dwError = RpcSvcUnbindRpcInterface(gpLsaSrvBinding,
                                       lsa_v0_0_s_ifspec);
    BAIL_ON_LSA_ERROR(dwError);

error:
    return dwError;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
