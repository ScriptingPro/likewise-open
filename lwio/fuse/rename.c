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

#include "includes.h"

NTSTATUS
LwIoFuseRename(
    const char* pszOldPath,
    const char* pszNewPath
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    IO_STATUS_BLOCK ioStatus = {0};
    IO_FILE_HANDLE handle = NULL;
    PIO_FUSE_CONTEXT pFuseContext = NULL;
    IO_FILE_NAME filename = {0};
    PFILE_RENAME_INFORMATION pRenameInfo = NULL;
    PWSTR pwszNewPath = NULL;
    size_t newPathLength = 0;

    pFuseContext = LwIoFuseGetContext();

    status = LwIoFuseGetDriverRelativePath(
        pFuseContext,
        pszNewPath,
        &pwszNewPath);
    BAIL_ON_NT_STATUS(status);

    newPathLength = LwRtlWC16StringNumChars(pwszNewPath);

    status = RTL_ALLOCATE(
        &pRenameInfo,
        FILE_RENAME_INFORMATION,
        sizeof(*pRenameInfo) + (newPathLength + 1) * sizeof(WCHAR));
    BAIL_ON_NT_STATUS(status);

    pRenameInfo->ReplaceIfExists = TRUE;
    pRenameInfo->RootDirectory = NULL;
    pRenameInfo->FileNameLength = newPathLength * sizeof(WCHAR);

    memcpy(pRenameInfo->FileName, pwszNewPath, newPathLength * sizeof(WCHAR));

    status = LwIoFuseGetNtFilename(
        pFuseContext,
        pszOldPath,
        &filename);
    BAIL_ON_NT_STATUS(status);

    status = LwNtCreateFile(
        &handle,                 /* File handle */
        NULL,                    /* Async control block */
        &ioStatus,               /* IO status block */
        &filename,               /* Filename */
        NULL,                    /* Security descriptor */
        NULL,                    /* Security QOS */
        DELETE,                  /* Desired access mask */
        0,                       /* Allocation size */
        0,                       /* File attributes */
        FILE_SHARE_READ |
        FILE_SHARE_WRITE |
        FILE_SHARE_DELETE,       /* Share access */
        FILE_OPEN,               /* Create disposition */
        0,                       /* Create options */
        NULL,                    /* EA buffer */
        0,                       /* EA length */
        NULL,                    /* ECP list */
        NULL);
    BAIL_ON_NT_STATUS(status);

    status = LwNtSetInformationFile(
        handle, /* File handle */
        NULL, /* Async control block */
        &ioStatus, /* IO status block */
        pRenameInfo, /* File information */
        sizeof(*pRenameInfo) + (newPathLength + 1) * sizeof(WCHAR), /* File information size */
        FileRenameInformation); /* Information class */
    BAIL_ON_NT_STATUS(status);

cleanup:

    if (handle)
    {
        LwNtCloseFile(handle);
    }

    RTL_UNICODE_STRING_FREE(&filename.Name);
    RTL_FREE(&pwszNewPath);
    RTL_FREE(&pRenameInfo);

    return status;

error:

    goto cleanup;
}
