/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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
 *        ntlmsrvapi.h
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Inter-process Communication API header (NTLM Server)
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Marc Guy (mguy@likewisesoftware.com)
 */

#ifndef __NTLMSRVAPI_H__
#define __NTLMSRVAPI_H__

#include <ntlm/ntlm.h>

DWORD
NtlmServerAcceptSecurityContext(
    PCredHandle phCredential,
    PCtxtHandle phContext,
    PSecBufferDesc pInput,
    ULONG fContextReq,
    ULONG TargetDataRep,
    PCtxtHandle phNewContext,
    PSecBufferDesc pOutput,
    PULONG pfContextAttr,
    PTimeStamp ptsTimeStamp
    );

DWORD
NtlmServerAcquireCredentialsHandle(
    SEC_CHAR *pszPrincipal,
    SEC_CHAR *pszPackage,
    ULONG fCredentialUse,
    PLUID pvLogonID,
    PVOID pAuthData,
    // NOT NEEDED BY NTLM - SEC_GET_KEY_FN pGetKeyFn,
    // NOT NEEDED BY NTLM - PVOID pvGetKeyArgument,
    PCredHandle phCredential,
    PTimeStamp ptsExpiry
    );

DWORD
NtlmServerDecryptMessage(
    PCtxtHandle phContext,
    PSecBufferDesc pMessage,
    ULONG MessageSeqNo,
    PULONG pfQoP
    );

DWORD
NtlmServerEncryptMessage(
    PCtxtHandle phContext,
    ULONG fQoP,
    PSecBufferDesc pMessage,
    ULONG MessageSeqNo
    );

DWORD
NtlmServerExportSecurityContext(
    PCtxtHandle phContext,
    ULONG fFlags,
    PSecBuffer pPackedContext,
    HANDLE *pToken
    );

DWORD
NtlmServerFreeCredentialsHandle(
    PCredHandle phCredential
    );

DWORD
NtlmServerImportSecurityContext(
    PSECURITY_STRING *pszPackage,
    PSecBuffer pPackedContext,
    HANDLE pToken,
    PCtxtHandle phContext
    );

DWORD
NtlmServerInitializeSecurityContext(
    PCredHandle phCredential,
    PCtxtHandle phContext,
    SEC_CHAR * pszTargetName,
    ULONG fContextReq,
    ULONG Reserved1,
    ULONG TargetDataRep,
    PSecBufferDesc pInput,
    ULONG Reserved2,
    PCtxtHandle phNewContext,
    PSecBufferDesc pOutput,
    PULONG pfContextAttr,
    PTimeStamp ptsExpiry
    );

DWORD
NtlmServerMakeSignature(
    PCtxtHandle phContext,
    ULONG fQoP,
    PSecBufferDesc pMessage,
    ULONG MessageSeqNo
    );

DWORD
NtlmServerQueryCredentialsAttributes(
    PCredHandle phCredential,
    ULONG ulAttribute,
    PVOID pBuffer
    );

DWORD
NtlmServerQueryContextAttributes(
    PCtxtHandle phContext,
    ULONG ulAttribute,
    PVOID pBuffer
    );

DWORD
NtlmServerVerifySignature(
    PCtxtHandle phContext,
    PSecBufferDesc pMessage,
    ULONG MessageSeqNo
    );

#endif // __NTLMSRVAPI_H__
