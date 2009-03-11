/*
 * Copyright (c) Likewise Software.  All rights Reserved.
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
 * Module Name:
 *
 *        security-sid.c
 *
 * Abstract:
 *
 *        SID Functions in Security Module.
 *
 * Authors: Danilo Almeida (dalmeida@likewise.com)
 *
 */

#include "security-includes.h"
// TODO-Move s*printf replacement functions to rtlstring.h
// For snprintf
#include <stdio.h>

//
// SID Functions
//

NTSTATUS
RtlInitializeSid(
    OUT PSID Sid,
    IN PSID_IDENTIFIER_AUTHORITY IdentifierAuthority,
    IN UCHAR SubAuthorityCount
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (!Sid || !IdentifierAuthority)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    if (SubAuthorityCount > SID_MAX_SUB_AUTHORITIES)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    Sid->Revision = SID_REVISION;
    Sid->SubAuthorityCount = SubAuthorityCount;
    memcpy(&Sid->IdentifierAuthority, IdentifierAuthority, sizeof(*IdentifierAuthority));
    memset(Sid->SubAuthority, 0, sizeof(Sid->SubAuthority[0]) * SubAuthorityCount);

cleanup:
    return status;
}

ULONG
RtlLengthRequiredSid(
    IN ULONG SubAuthorityCount
    )
{
    return _SID_GET_SIZE_REQUIRED(SubAuthorityCount);
}

ULONG
RtlLengthSid(
    IN PSID Sid
    )
{
    return RtlLengthRequiredSid(Sid->SubAuthorityCount);
}

BOOLEAN
RtlValidSid(
    IN PSID Sid
    )
{
    return ((Sid != NULL) &&
            (Sid->Revision == SID_REVISION) &&
            (Sid->SubAuthorityCount <= SID_MAX_SUB_AUTHORITIES));
}

BOOLEAN
RtlEqualSid(
    IN PSID Sid1,
    IN PSID Sid2
    )
{
    return ((Sid1->SubAuthorityCount == Sid2->SubAuthorityCount) &&
            RtlEqualMemory(Sid1, Sid2, RtlLengthSid(Sid1)));
}

BOOLEAN
RtlEqualPrefixSid(
    IN PSID Sid1,
    IN PSID Sid2
    )
{
    BOOLEAN isEqual = FALSE;
    if (Sid1->SubAuthorityCount == Sid2->SubAuthorityCount)
    {
        UCHAR count = Sid1->SubAuthorityCount;
        if (count > 0)
        {
            count--;
        }
        isEqual = RtlEqualMemory(Sid1, Sid2, RtlLengthRequiredSid(count));
    }
    return isEqual;
}

BOOLEAN
RtlIsPrefixSid(
    IN PSID Prefix,
    IN PSID Sid
    )
{
    return ((Prefix->SubAuthorityCount <= Sid->SubAuthorityCount) &&
            RtlEqualMemory(Prefix, Sid, RtlLengthSid(Prefix)));
}

NTSTATUS
RtlCopySid(
    IN ULONG DestinationSidLength,
    OUT PSID DestinationSid,
    IN PSID SourceSid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG length = RtlLengthSid(SourceSid);

    if (DestinationSidLength < length)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    RtlCopyMemory(DestinationSid, SourceSid, length);

cleanup:
    return status;
}

BOOLEAN
RtlpIsValidLittleEndianSidBuffer(
    IN PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG BufferUsed
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSID littleEndianSid = (PSID) Buffer;
    ULONG size = 0;

    if (BufferSize < SID_MIN_SIZE)
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    size = RtlLengthRequiredSid(LW_LTOH8(littleEndianSid->SubAuthorityCount));
    if (!RtlpIsBufferAvailable(BufferSize, 0, size))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    // This is ok since it only looks at 1-byte fields:
    status = RtlValidSid(littleEndianSid) ? STATUS_SUCCESS : STATUS_INVALID_SID;

cleanup:
    *BufferUsed = NT_SUCCESS(status) ? size : 0;

    return NT_SUCCESS(status);
}

NTSTATUS
RtlpEncodeLittleEndianSid(
    IN PSID Sid,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG BufferUsed
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSID littleEndianSid = (PSID) Buffer;
    ULONG size = RtlLengthSid(Sid);
    ULONG i = 0;

    if (BufferSize < size)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        GOTO_CLEANUP();
    }

    littleEndianSid->Revision = LW_HTOL8(Sid->Revision);
    littleEndianSid->SubAuthorityCount = LW_HTOL8(Sid->SubAuthorityCount);
    // sequence of bytes
    littleEndianSid->IdentifierAuthority = Sid->IdentifierAuthority;

    for (i = 0; i < Sid->SubAuthorityCount; i++)
    {
        littleEndianSid->SubAuthority[i] = LW_HTOL32(Sid->SubAuthority[i]);
    }

    status = STATUS_SUCCESS;

cleanup:
    *BufferUsed = NT_SUCCESS(status) ? size : 0;

    return status;
}

VOID
RtlpDecodeLittleEndianSid(
    IN PSID LittleEndianSid,
    OUT PSID Sid
    )
{
    ULONG i = 0;

    Sid->Revision = LW_LTOH8(LittleEndianSid->Revision);
    Sid->SubAuthorityCount = LW_LTOH8(LittleEndianSid->SubAuthorityCount);
    // sequence of bytes
    Sid->IdentifierAuthority = LittleEndianSid->IdentifierAuthority;

    for (i = 0; i < Sid->SubAuthorityCount; i++)
    {
        Sid->SubAuthority[i] = LW_LTOH32(LittleEndianSid->SubAuthority[i]);
    }
}

//
// SID <-> String Conversion Functions
//

//
// A string SID is represented as:
//
//   "S-" REV "-" AUTH ("-" SUB_AUTH ) * SubAuthorityCount
//
// where:
//
//   - REV is a decimal UCHAR (max 3 characters).
//
//   - AUTH is either a decimal ULONG (max 10 characters) or
//     "0x" followed by a 6-byte hex value (max 2 + 12 = 14 characters).
//
//   - SUB_AUTH is a decimal ULONG (max 10 characters).
//

#define RTLP_STRING_SID_MAX_CHARS(SubAuthorityCount) \
    (2 + 3 + 1 + 14 + (1 + 10) * (SubAuthorityCount) + 1)

NTSTATUS
RtlAllocateUnicodeStringFromSid(
    OUT PUNICODE_STRING StringSid,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PWSTR resultBuffer = NULL;
    UNICODE_STRING result = { 0 };

    if (!StringSid)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlAllocateWC16StringFromSid(&resultBuffer, Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlUnicodeStringInitEx(&result, resultBuffer);
    GOTO_CLEANUP_ON_STATUS(status);
    resultBuffer = NULL;

    status = STATUS_SUCCESS;

cleanup:
    if (!NT_SUCCESS(status))
    {
        RtlUnicodeStringFree(&result);
    }
    RTL_FREE(&resultBuffer);

    if (StringSid)
    {
        *StringSid = result;
    }

    return status;
}

NTSTATUS
RtlAllocateAnsiStringFromSid(
    OUT PANSI_STRING StringSid,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSTR resultBuffer = NULL;
    ANSI_STRING result = { 0 };

    if (!StringSid)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlAllocateCStringFromSid(&resultBuffer, Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlAnsiStringInitEx(&result, resultBuffer);
    GOTO_CLEANUP_ON_STATUS(status);
    resultBuffer = NULL;

    status = STATUS_SUCCESS;

cleanup:
    if (!NT_SUCCESS(status))
    {
        RtlAnsiStringFree(&result);
    }
    RTL_FREE(&resultBuffer);

    if (StringSid)
    {
        *StringSid = result;
    }

    return status;
}

NTSTATUS
RtlAllocateWC16StringFromSid(
    OUT PWSTR* StringSid,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PWSTR result = NULL;
    PSTR convertString = NULL;

    if (!StringSid)
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    status = RtlAllocateCStringFromSid(&convertString, Sid);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlWC16StringAllocateFromCString(&result, convertString);
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&result);
    }
    RTL_FREE(&convertString);

    if (StringSid)
    {
        *StringSid = result;
    }

    return status;

}

NTSTATUS
RtlAllocateCStringFromSid(
    OUT PSTR* StringSid,
    IN PSID Sid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSTR result = NULL;
    size_t size = 0;
    int count = 0;
    ULONG i = 0;

    if (!StringSid || !RtlValidSid(Sid))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    size = RTLP_STRING_SID_MAX_CHARS(Sid->SubAuthorityCount);

    status = RTL_ALLOCATE(&result, CHAR, size);
    GOTO_CLEANUP_ON_STATUS(status);

    if (Sid->IdentifierAuthority.Value[0] || Sid->IdentifierAuthority.Value[1])
    {
        count += snprintf(result + count,
                          size - count,
                          "S-%u-0x%.2X%.2X%.2X%.2X%.2X%.2X",
                          Sid->Revision,
                          Sid->IdentifierAuthority.Value[0],
                          Sid->IdentifierAuthority.Value[1],
                          Sid->IdentifierAuthority.Value[2],
                          Sid->IdentifierAuthority.Value[3],
                          Sid->IdentifierAuthority.Value[4],
                          Sid->IdentifierAuthority.Value[5]);
    }
    else
    {
        ULONG value = 0;

        value |= (ULONG) Sid->IdentifierAuthority.Value[5];
        value |= (ULONG) Sid->IdentifierAuthority.Value[4] << 8;
        value |= (ULONG) Sid->IdentifierAuthority.Value[3] << 16;
        value |= (ULONG) Sid->IdentifierAuthority.Value[2] << 24;

        count += snprintf(result + count,
                          size - count,
                          "S-%u-%u",
                          Sid->Revision,
                          value);
    }

    for (i = 0; i < Sid->SubAuthorityCount; i++)
    {
        count += snprintf(result + count,
                          size - count,
                          "-%u",
                          Sid->SubAuthority[i]);
    }

    status = STATUS_SUCCESS;

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&result);
    }

    if (StringSid)
    {
        *StringSid = result;
    }

    return status;
}

static
NTSTATUS
RtlpConvertUnicodeStringSidToSidEx(
    IN PUNICODE_STRING StringSid,
    OUT OPTIONAL PSID* AllocateSid,
    OUT OPTIONAL PSID SidBuffer,
    IN OUT OPTIONAL PULONG SidBufferSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR sidBuffer[SID_MAX_SIZE] = { 0 };
    PSID sid = (PSID) sidBuffer;
    BOOLEAN haveRevision = FALSE;
    BOOLEAN haveAuthority = FALSE;
    UNICODE_STRING remaining = { 0 };
    PSID newSid = NULL;
    ULONG sizeRequired = 0;

    if (!StringSid ||
        (AllocateSid && (SidBuffer || SidBufferSize)) ||
        !(AllocateSid || SidBufferSize))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Must have at least 2 characters and they must be "S-"
    if (!((StringSid->Length > (2 * sizeof(StringSid->Buffer[0]))) &&
          ((StringSid->Buffer[0] == 'S') || (StringSid->Buffer[0] == 's')) &&
          (StringSid->Buffer[1] == '-')))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    // Skip the "S-" prefix
    remaining.Buffer = &StringSid->Buffer[2];
    remaining.Length = StringSid->Length - (2 * sizeof(StringSid->Buffer[0]));
    remaining.MaximumLength = remaining.Length;

    // TODO-Handle S-1-0xHEX-... for more than 4 bytes in IdentifierAuth.
    for (;;)
    {
        ULONG value = 0;

        status = LwRtlUnicodeStringParseULONG(&value, &remaining, &remaining);
        if (!NT_SUCCESS(status))
        {
            break;
        }

        if (remaining.Length)
        {
            if (remaining.Buffer[0] != '-')
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            remaining.Buffer++;
            remaining.Length -= sizeof(remaining.Buffer[0]);
            remaining.MaximumLength = remaining.Length;
        }

        if (!haveRevision)
        {
            if (value > MAXUCHAR)
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            sid->Revision = (UCHAR) value;
            haveRevision = TRUE;
        }
        else if (!haveAuthority)
        {
            // Authority is represented as a 32-bit number.
            sid->IdentifierAuthority.Value[5] = (value & 0x000000FF);
            sid->IdentifierAuthority.Value[4] = (value & 0x0000FF00) >> 8;
            sid->IdentifierAuthority.Value[3] = (value & 0x00FF0000) >> 16;
            sid->IdentifierAuthority.Value[2] = (value & 0xFF000000) >> 24;
            haveAuthority = TRUE;
        }
        else
        {
            if (sid->SubAuthorityCount >= SID_MAX_SUB_AUTHORITIES)
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            sid->SubAuthority[sid->SubAuthorityCount] = value;
            sid->SubAuthorityCount++;
        }
    }

    if (!haveAuthority || remaining.Length || !RtlValidSid(sid))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    sizeRequired = RtlLengthSid(sid);

    if (AllocateSid)
    {
        status = RTL_ALLOCATE(&newSid, SID, sizeRequired);
        GOTO_CLEANUP_ON_STATUS(status);

        RtlCopyMemory(newSid, sid, sizeRequired);
    }
    else if (SidBufferSize)
    {
        if (*SidBufferSize < sizeRequired)
        {
            status = STATUS_BUFFER_TOO_SMALL;
            GOTO_CLEANUP();
        }
        if (SidBuffer)
        {
            RtlCopyMemory(SidBuffer, sid, sizeRequired);
        }
    }
    else
    {
        status = STATUS_ASSERTION_FAILURE;
        GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&newSid);
        sid = NULL;
    }

    if (AllocateSid)
    {
        *AllocateSid = newSid;
    }

    if (SidBufferSize)
    {
        *SidBufferSize = sizeRequired;
    }

    return status;
}

static
NTSTATUS
RtlpConvertAnsiStringSidToSidEx(
    IN PANSI_STRING StringSid,
    OUT OPTIONAL PSID* AllocateSid,
    OUT OPTIONAL PSID SidBuffer,
    IN OUT OPTIONAL PULONG SidBufferSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    UCHAR sidBuffer[SID_MAX_SIZE] = { 0 };
    PSID sid = (PSID) sidBuffer;
    BOOLEAN haveRevision = FALSE;
    BOOLEAN haveAuthority = FALSE;
    ANSI_STRING remaining = { 0 };
    PSID newSid = NULL;
    ULONG sizeRequired = 0;

    if (!StringSid ||
        (AllocateSid && (SidBuffer || SidBufferSize)) ||
        !(AllocateSid || SidBufferSize))
    {
        status = STATUS_INVALID_PARAMETER;
        GOTO_CLEANUP();
    }

    // Must have at least 2 characters and they must be "S-"
    if (!((StringSid->Length > (2 * sizeof(StringSid->Buffer[0]))) &&
          ((StringSid->Buffer[0] == 'S') || (StringSid->Buffer[0] == 's')) &&
          (StringSid->Buffer[1] == '-')))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    // Skip the "S-" prefix
    remaining.Buffer = &StringSid->Buffer[2];
    remaining.Length = StringSid->Length - (2 * sizeof(StringSid->Buffer[0]));
    remaining.MaximumLength = remaining.Length;

    // TODO-Handle S-1-0xHEX-... for more than 4 bytes in IdentifierAuth.
    for (;;)
    {
        ULONG value = 0;

        status = LwRtlAnsiStringParseULONG(&value, &remaining, &remaining);
        if (!NT_SUCCESS(status))
        {
            break;
        }

        if (remaining.Length)
        {
            if (remaining.Buffer[0] != '-')
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            remaining.Buffer++;
            remaining.Length -= sizeof(remaining.Buffer[0]);
            remaining.MaximumLength = remaining.Length;
        }

        if (!haveRevision)
        {
            if (value > MAXUCHAR)
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            sid->Revision = (UCHAR) value;
            haveRevision = TRUE;
        }
        else if (!haveAuthority)
        {
            // Authority is represented as a 32-bit number.
            sid->IdentifierAuthority.Value[5] = (value & 0x000000FF);
            sid->IdentifierAuthority.Value[4] = (value & 0x0000FF00) >> 8;
            sid->IdentifierAuthority.Value[3] = (value & 0x00FF0000) >> 16;
            sid->IdentifierAuthority.Value[2] = (value & 0xFF000000) >> 24;
            haveAuthority = TRUE;
        }
        else
        {
            if (sid->SubAuthorityCount >= SID_MAX_SUB_AUTHORITIES)
            {
                status = STATUS_INVALID_SID;
                GOTO_CLEANUP();
            }
            sid->SubAuthority[sid->SubAuthorityCount] = value;
            sid->SubAuthorityCount++;
        }
    }

    if (!haveAuthority || remaining.Length || !RtlValidSid(sid))
    {
        status = STATUS_INVALID_SID;
        GOTO_CLEANUP();
    }

    sizeRequired = RtlLengthSid(sid);

    if (AllocateSid)
    {
        status = RTL_ALLOCATE(&newSid, SID, sizeRequired);
        GOTO_CLEANUP_ON_STATUS(status);

        RtlCopyMemory(newSid, sid, sizeRequired);
    }
    else if (SidBufferSize)
    {
        if (*SidBufferSize < sizeRequired)
        {
            status = STATUS_BUFFER_TOO_SMALL;
            GOTO_CLEANUP();
        }
        if (SidBuffer)
        {
            RtlCopyMemory(SidBuffer, sid, sizeRequired);
        }
    }
    else
    {
        status = STATUS_ASSERTION_FAILURE;
        GOTO_CLEANUP();
    }

    status = STATUS_SUCCESS;

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&newSid);
        sid = NULL;
    }

    if (AllocateSid)
    {
        *AllocateSid = newSid;
    }

    if (SidBufferSize)
    {
        *SidBufferSize = sizeRequired;
    }

    return status;
}

NTSTATUS
RtlAllocateSidFromUnicodeString(
    OUT PSID* Sid,
    IN PUNICODE_STRING StringSid
    )
{
    return RtlpConvertUnicodeStringSidToSidEx(StringSid, Sid, NULL, NULL);
}

NTSTATUS
RtlAllocateSidFromAnsiString(
    OUT PSID* Sid,
    IN PANSI_STRING StringSid
    )
{
    return RtlpConvertAnsiStringSidToSidEx(StringSid, Sid, NULL, NULL);
}

NTSTATUS
RtlAllocateSidFromCString(
    OUT PSID* Sid,
    IN PCSTR StringSid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSID sid = NULL;
    ANSI_STRING stringSid = { 0 };

    status = RtlAnsiStringInitEx(&stringSid, StringSid);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlpConvertAnsiStringSidToSidEx(&stringSid, &sid, NULL, NULL);
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&sid);
    }

    *Sid = sid;

    return status;
}

NTSTATUS
RtlAllocateSidFromWC16String(
    OUT PSID* Sid,
    IN PCWSTR StringSid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PSID sid = NULL;
    UNICODE_STRING stringSid = { 0 };

    status = RtlUnicodeStringInitEx(&stringSid, StringSid);
    GOTO_CLEANUP_ON_STATUS(status);

    status = RtlAllocateSidFromUnicodeString(&sid, &stringSid);
    GOTO_CLEANUP_ON_STATUS(status);

cleanup:
    if (!NT_SUCCESS(status))
    {
        RTL_FREE(&sid);
    }

    *Sid = sid;

    return status;
}

//
// Well-Known SID Functions
//

NTSTATUS
RtlCreateWellKnownSid(
    IN WELL_KNOWN_SID_TYPE WellKnownSidType,
    IN OPTIONAL PSID DomainOrComputerSid,
    OUT PSID* Sid,
    IN OUT PULONG SidSize
    );
