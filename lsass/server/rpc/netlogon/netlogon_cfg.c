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
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        netlogon_cfg.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        Netlogon rpc server configuration
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 *          Adam Bernstein (abernstein@vmware.com)
 */


#include "includes.h"


DWORD
NetlogonSrvInitialiseConfig(
    PNETLOGON_SRV_CONFIG pConfig
    )
{
    DWORD dwError = 0;

    memset(pConfig, 0, sizeof(*pConfig));

    dwError = LwAllocateString(
                NETLOGON_RPC_CFG_DEFAULT_LPC_SOCKET_PATH,
                &pConfig->pszLpcSocketPath);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateString(
                NETLOGON_RPC_CFG_DEFAULT_LOGIN_SHELL,
                &pConfig->pszDefaultLoginShell);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateString(
                NETLOGON_RPC_CFG_DEFAULT_HOMEDIR_PREFIX,
                &pConfig->pszHomedirPrefix);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateString(
                NETLOGON_RPC_CFG_DEFAULT_HOMEDIR_TEMPLATE,
                &pConfig->pszHomedirTemplate);
    BAIL_ON_LSA_ERROR(dwError);

    pConfig->bRegisterTcpIp = FALSE;

cleanup:
    return dwError;

error:
    NetlogonSrvFreeConfigContents(pConfig);
    goto cleanup;
}

VOID
NetlogonSrvFreeConfigContents(
    PNETLOGON_SRV_CONFIG pConfig
    )
{
    if (pConfig)
    {
        LW_SAFE_FREE_STRING(pConfig->pszLpcSocketPath);
        LW_SAFE_FREE_STRING(pConfig->pszDefaultLoginShell);
        LW_SAFE_FREE_STRING(pConfig->pszHomedirPrefix);
        LW_SAFE_FREE_STRING(pConfig->pszHomedirTemplate);
    }
}

DWORD
NetlogonSrvReadRegistry(
    PNETLOGON_SRV_CONFIG pConfig
    )
{
    DWORD dwError = 0;

    PLSA_CONFIG_REG pReg = NULL;

    dwError = LsaOpenConfig(
            "Services\\lsass\\Parameters\\RPCServers\\netlogon",
            "Policy\\Services\\lsass\\Parameters\\RPCServers\\netlogon",
            &pReg);
    BAIL_ON_LSA_ERROR(dwError);

    if (!pReg)
    {
        goto error;
    }

    dwError = LsaReadConfigString(
                pReg,
                "LpcSocketPath",
                FALSE,
                &pConfig->pszLpcSocketPath,
                NULL);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaReadConfigString(
                pReg,
                "LoginShellTemplate",
                TRUE,
                &pConfig->pszDefaultLoginShell,
                NULL);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaReadConfigString(
                pReg,
                "HomeDirPrefix",
                TRUE,
                &pConfig->pszHomedirPrefix,
                NULL);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaReadConfigString(
                pReg,
                "HomeDirTemplate",
                TRUE,
                &pConfig->pszHomedirTemplate,
                NULL);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaReadConfigBoolean(
                pReg,
                "RegisterTcpIp",
                TRUE,
                &pConfig->bRegisterTcpIp);
    BAIL_ON_LSA_ERROR(dwError);

cleanup:
    LsaCloseConfig(pReg);
    pReg = NULL;

    return dwError;

error:
    goto cleanup;
}

DWORD
NetlogonSrvConfigGetLpcSocketPath(
    PSTR *ppszLpcSocketPath
    )
{
    DWORD dwError = 0;
    BOOL bLocked = 0;
    PSTR pszLpcSocketPath = NULL;

    GLOBAL_DATA_LOCK(bLocked);

    if (LW_IS_NULL_OR_EMPTY_STR(gNetlogonSrvConfig.pszLpcSocketPath)) {
        goto cleanup;
    }

    dwError = LwAllocateString(gNetlogonSrvConfig.pszLpcSocketPath,
                                &pszLpcSocketPath);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszLpcSocketPath = pszLpcSocketPath;

cleanup:
    GLOBAL_DATA_UNLOCK(bLocked);
    return dwError;

error:
    goto cleanup;
}


DWORD
NetlogonSrvConfigGetDefaultLoginShell(
    PSTR *ppszDefaultLoginShell
    )
{
    DWORD dwError = 0;
    BOOL bLocked = 0;
    PSTR pszDefaultLoginShell = NULL;

    GLOBAL_DATA_LOCK(bLocked);

    if (LW_IS_NULL_OR_EMPTY_STR(gNetlogonSrvConfig.pszDefaultLoginShell)) {
        goto cleanup;
    }

    dwError = LwAllocateString(gNetlogonSrvConfig.pszDefaultLoginShell,
                               &pszDefaultLoginShell);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszDefaultLoginShell = pszDefaultLoginShell;

cleanup:
    GLOBAL_DATA_UNLOCK(bLocked);
    return dwError;

error:
    goto cleanup;
}

DWORD
NetlogonSrvConfigGetHomedirPrefix(
    PSTR *ppszHomedirPrefix
    )
{
    DWORD dwError = 0;
    BOOL bLocked = 0;
    PSTR pszHomedirPrefix = NULL;

    GLOBAL_DATA_LOCK(bLocked);

    if (LW_IS_NULL_OR_EMPTY_STR(gNetlogonSrvConfig.pszHomedirPrefix)) {
        goto cleanup;
    }

    dwError = LwAllocateString(gNetlogonSrvConfig.pszHomedirPrefix,
                               &pszHomedirPrefix);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszHomedirPrefix = pszHomedirPrefix;

cleanup:
    GLOBAL_DATA_UNLOCK(bLocked);
    return dwError;

error:
    goto cleanup;
}


DWORD
NetlogonSrvConfigGetHomedirTemplate(
    PSTR *ppszHomedirTemplate
    )
{
    DWORD dwError = 0;
    BOOL bLocked = 0;
    PSTR pszHomedirTemplate = NULL;

    GLOBAL_DATA_LOCK(bLocked);

    if (LW_IS_NULL_OR_EMPTY_STR(gNetlogonSrvConfig.pszHomedirTemplate)) {
        goto cleanup;
    }

    dwError = LwAllocateString(gNetlogonSrvConfig.pszHomedirTemplate,
                                &pszHomedirTemplate);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszHomedirTemplate = pszHomedirTemplate;

cleanup:
    GLOBAL_DATA_UNLOCK(bLocked);
    return dwError;

error:
    goto cleanup;
}

DWORD
NetlogonSrvConfigShouldRegisterTcpIp(
    BOOLEAN* pbResult
    )
{
    DWORD dwError = 0;
    BOOL bLocked = 0;

    GLOBAL_DATA_LOCK(bLocked);

    *pbResult = gNetlogonSrvConfig.bRegisterTcpIp;

cleanup:
    GLOBAL_DATA_UNLOCK(bLocked);
    return dwError;

error:
    *pbResult = FALSE;
    goto cleanup;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
