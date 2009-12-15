/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software    2004-2008
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
 *        lpdomain.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Local Authentication Provider
 *
 *        Domain Management
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 */

#include "includes.h"

static
DWORD
LocalGetSingleStringAttrValue(
    PATTRIBUTE_VALUE pAttrs,
    DWORD            dwNumAttrs,
    PSTR*            ppszValue
    );

static
DWORD
LocalGetSingleLargeIntegerAttrValue(
    PATTRIBUTE_VALUE pAttrs,
    DWORD            dwNumAttrs,
    PLONG64          pllAttrValue
    );

DWORD
LocalGetDomainInfo(
    PWSTR   pwszUserDN,
    PWSTR   pwszCredentials,
    ULONG   ulMethod,
    PSTR*   ppszNetBIOSName,
    PSTR*   ppszDomainName,
    PSID*   ppDomainSID,
    PLONG64 pllMaxPwdAge,
    PLONG64 pllPwdChangeTime
    )
{
    DWORD  dwError = 0;
    HANDLE hDirectory  = NULL;
    DWORD  objectClass = LOCAL_OBJECT_CLASS_DOMAIN;
    PCSTR  pszFilterTemplate = LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR   pszFilter = NULL;
    PWSTR  pwszFilter = NULL;
    wchar16_t wszAttrNameDomain[]      = LOCAL_DIR_ATTR_DOMAIN;
    wchar16_t wszAttrNameNetBIOSName[] = LOCAL_DIR_ATTR_NETBIOS_NAME;
    wchar16_t wszAttrNameObjectSID[]   = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameMaxPwdAge[]   = LOCAL_DIR_ATTR_MAX_PWD_AGE;
    wchar16_t wszAttrNamePwdChangeTime[] = LOCAL_DIR_ATTR_PWD_PROMPT_TIME;
    PWSTR  wszAttrs[] =
    {
            &wszAttrNameDomain[0],
            &wszAttrNameNetBIOSName[0],
            &wszAttrNameObjectSID[0],
            &wszAttrNameMaxPwdAge[0],
            &wszAttrNamePwdChangeTime[0],
            NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry   = NULL;
    DWORD            dwNumEntries = 0;
    DWORD            iAttr = 0;
    ULONG            ulScope = 0;
    PSTR   pszDomainName  = NULL;
    PSTR   pszNetBIOSName = NULL;
    PSTR   pszDomainSID   = NULL;
    PSID   pDomainSID     = NULL;
    LONG64 llMaxPwdAge = 0;
    LONG64 llPwdChangeTime = 0;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    objectClass);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectoryOpen(&hDirectory);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectoryBind(
                    hDirectory,
                    pwszUserDN,
                    pwszCredentials,
                    ulMethod);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    hDirectory,
                    NULL,
                    ulScope,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (!dwNumEntries)
    {
        dwError = LW_ERROR_NO_SUCH_DOMAIN;
        BAIL_ON_LSA_ERROR(dwError);
    }
    else
    if (dwNumEntries > 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    for (; iAttr < pEntry->ulNumAttributes; iAttr++)
    {
        PDIRECTORY_ATTRIBUTE pAttr = &pEntry->pAttributes[iAttr];

        if (!wc16scasecmp(pAttr->pwszName, &wszAttrNameDomain[0]))
        {
            dwError = LocalGetSingleStringAttrValue(
                            pAttr->pValues,
                            pAttr->ulNumValues,
                            &pszDomainName);
        }
        else
        if (!wc16scasecmp(pAttr->pwszName, &wszAttrNameNetBIOSName[0]))
        {
            dwError = LocalGetSingleStringAttrValue(
                            pAttr->pValues,
                            pAttr->ulNumValues,
                            &pszNetBIOSName);
        }
        else
        if (!wc16scasecmp(pAttr->pwszName, &wszAttrNameMaxPwdAge[0]))
        {
            dwError = LocalGetSingleLargeIntegerAttrValue(
                            pAttr->pValues,
                            pAttr->ulNumValues,
                            &llMaxPwdAge);
        }
        else
        if (!wc16scasecmp(pAttr->pwszName, &wszAttrNamePwdChangeTime[0]))
        {
            dwError = LocalGetSingleLargeIntegerAttrValue(
                            pAttr->pValues,
                            pAttr->ulNumValues,
                            &llPwdChangeTime);
        }
        else
        if (!wc16scasecmp(pAttr->pwszName, &wszAttrNameObjectSID[0]))
        {
            dwError = LocalGetSingleStringAttrValue(
                            pAttr->pValues,
                            pAttr->ulNumValues,
                            &pszDomainSID);
        }
        else
        {
            dwError = LW_ERROR_DATA_ERROR;
        }
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = RtlAllocateSidFromCString(
                    &pDomainSID,
                    pszDomainSID);
    BAIL_ON_LSA_ERROR(dwError);

    *ppszDomainName = pszDomainName;
    *ppszNetBIOSName = pszNetBIOSName;
    *ppDomainSID = pDomainSID;
    *pllMaxPwdAge = llMaxPwdAge;
    *pllPwdChangeTime = llPwdChangeTime;

cleanup:

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    if (hDirectory)
    {
        DirectoryClose(hDirectory);
    }

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_STRING(pszDomainSID);
    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    return dwError;

error:

    *ppszDomainName = NULL;
    *ppszNetBIOSName = NULL;
    *ppDomainSID = NULL;
    *pllMaxPwdAge = 0;
    *pllPwdChangeTime = 0;

    LW_SAFE_FREE_STRING(pszDomainName);
    LW_SAFE_FREE_STRING(pszNetBIOSName);
    RTL_FREE(&pDomainSID);

    goto cleanup;
}

static
DWORD
LocalGetSingleStringAttrValue(
    PATTRIBUTE_VALUE pAttrs,
    DWORD            dwNumAttrs,
    PSTR*            ppszValue
    )
{
    DWORD dwError = 0;
    PSTR  pszValue = NULL;

    if ((dwNumAttrs != 1) ||
        (pAttrs[0].Type != DIRECTORY_ATTR_TYPE_UNICODE_STRING))
    {
        dwError = LW_ERROR_INVALID_PARAMETER;
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pAttrs[0].data.pwszStringValue)
    {
        dwError = LsaWc16sToMbs(
                        pAttrs[0].data.pwszStringValue,
                        &pszValue);
        BAIL_ON_LSA_ERROR(dwError);
    }

    *ppszValue = pszValue;

cleanup:

    return dwError;

error:

    LW_SAFE_FREE_STRING(pszValue);

    *ppszValue = NULL;

    goto cleanup;
}

static
DWORD
LocalGetSingleLargeIntegerAttrValue(
    PATTRIBUTE_VALUE pAttrs,
    DWORD            dwNumAttrs,
    PLONG64          pllAttrValue
    )
{
    DWORD dwError = 0;

    if ((dwNumAttrs != 1) ||
        (pAttrs[0].Type != DIRECTORY_ATTR_TYPE_LARGE_INTEGER))
    {
        dwError = LW_ERROR_INVALID_PARAMETER;
        BAIL_ON_LSA_ERROR(dwError);
    }

    *pllAttrValue = pAttrs[0].data.llValue;

cleanup:

    return dwError;

error:

    *pllAttrValue = 0;

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
