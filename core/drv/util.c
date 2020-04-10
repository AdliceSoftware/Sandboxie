/*
 * Copyright 2004-2020 Sandboxie Holdings, LLC 
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------


#include "util.h"
#include "process.h"
#include "common/my_version.h"


//---------------------------------------------------------------------------
// MyIsCurrentProcessRunningAsLocalSystem
//---------------------------------------------------------------------------


_FX BOOLEAN MyIsCurrentProcessRunningAsLocalSystem(void)
{
    extern const WCHAR *Driver_S_1_5_18;
    UNICODE_STRING SidString;
    ULONG SessionId;
    BOOLEAN s_1_5_18 = FALSE;
    NTSTATUS status = Process_GetSidStringAndSessionId(
                        NtCurrentProcess(), NULL, &SidString, &SessionId);
    if (NT_SUCCESS(status)) {
        if (_wcsicmp(SidString.Buffer, Driver_S_1_5_18) == 0)
            s_1_5_18 = TRUE;
        RtlFreeUnicodeString(&SidString);
    }
    return s_1_5_18;
}


//---------------------------------------------------------------------------
// MyIsProcessRunningAsSystemAccount
//---------------------------------------------------------------------------


_FX BOOLEAN MyIsProcessRunningAsSystemAccount(HANDLE ProcessId)
{
    UNICODE_STRING SidString;
    ULONG SessionId;
    BOOLEAN system;

    NTSTATUS status = Process_GetSidStringAndSessionId(
                            NULL, ProcessId, &SidString, &SessionId);

    if (NT_SUCCESS(status)) {

        extern const WCHAR *Driver_S_1_5_18;
        extern const WCHAR *Driver_S_1_5_19;
        extern const WCHAR *Driver_S_1_5_20;

        system = (_wcsicmp(SidString.Buffer, Driver_S_1_5_18) == 0 ||
                  _wcsicmp(SidString.Buffer, Driver_S_1_5_19) == 0 ||
                  _wcsicmp(SidString.Buffer, Driver_S_1_5_20) == 0);

        RtlFreeUnicodeString(&SidString);

    } else
        system = FALSE;

    return system;
}


//---------------------------------------------------------------------------
// MyIsCallerMyServiceProcess
//---------------------------------------------------------------------------


_FX BOOLEAN MyIsCallerMyServiceProcess(void)
{
    BOOLEAN ok = FALSE;

    if (MyIsCurrentProcessRunningAsLocalSystem()) {

        void *nbuf;
        ULONG nlen;
        WCHAR *nptr;

        const ULONG ProcessId = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
        Process_GetProcessName(Driver_Pool, ProcessId, &nbuf, &nlen, &nptr);
        if (nbuf) {

            UNICODE_STRING *uni = (UNICODE_STRING *)nbuf;

            if ((uni->Length > Driver_HomePathNt_Len * sizeof(WCHAR)) &&
                    (0 == _wcsnicmp(uni->Buffer, Driver_HomePathNt,
                                    Driver_HomePathNt_Len))) {

                if (_wcsicmp(nptr, SBIESVC_EXE) == 0) {

                    ok = TRUE;
                }
            }

            Mem_Free(nbuf, nlen);
        }
    }

    return ok;
}


//---------------------------------------------------------------------------
// MyGetSessionId
//---------------------------------------------------------------------------


_FX NTSTATUS MyGetSessionId(ULONG *SessionId)
{
    NTSTATUS status;
    PROCESS_SESSION_INFORMATION info;
    ULONG len;

    len = sizeof(info);
    status = ZwQueryInformationProcess(
        NtCurrentProcess(), ProcessSessionInformation,
        &info, sizeof(info), &len);

    if (NT_SUCCESS(status))
        *SessionId = info.SessionId;
    else
        *SessionId = 0;

    return status;
}


//---------------------------------------------------------------------------
// MyGetParentId
//---------------------------------------------------------------------------


_FX NTSTATUS MyGetParentId(HANDLE *ParentId)
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION info;
    ULONG len;

    len = sizeof(info);
    status = ZwQueryInformationProcess(
        NtCurrentProcess(), ProcessBasicInformation,
        &info, len, &len);

    if (NT_SUCCESS(status))
        *ParentId = (HANDLE)info.InheritedFromUniqueProcessId;
    else
        *ParentId = NULL;

    return status;
}


PWCHAR SearchUnicodeString(PCUNICODE_STRING pString1, PWCHAR pString2, BOOLEAN boolCaseInSensitive)
{
    if ((pString1 == NULL) || (pString1->Buffer == NULL) || (pString1->Length == 0) || (pString2 == NULL))
        return NULL;

    PWCHAR  pEOS;       // End Of String
    UNICODE_STRING  usSearch;
    UNICODE_STRING  usStr = *pString1;

    // make the search string a UNICODE_STRING
    RtlInitUnicodeString(&usSearch, pString2);
    if (usStr.Length < usSearch.Length)
        return NULL;

    usStr.Length = usSearch.Length;
    pEOS = pString1->Buffer + (pString1->Length - usSearch.Length) / 2;

    while (usStr.Buffer <= pEOS)
    {
        if (RtlCompareUnicodeString(&usStr, &usSearch, boolCaseInSensitive) == 0)
            return usStr.Buffer;
        usStr.Buffer++;
    }
    return NULL;
}

BOOLEAN UnicodeStringStartsWith(PCUNICODE_STRING pString1, PWCHAR pString2, BOOLEAN boolCaseInSensitive)
{
    if ((pString1 == NULL) || (pString1->Buffer == NULL) || (pString1->Length == 0) || (pString2 == NULL))
        return FALSE;

    UNICODE_STRING  usSearch;
    UNICODE_STRING  usStr = *pString1;

    // make a UNICODE_STRING from the search string
    RtlInitUnicodeString(&usSearch, pString2);
    if (usStr.Length < usSearch.Length)
        return FALSE;

    usStr.Length = usSearch.Length;
    return (RtlCompareUnicodeString(&usStr, &usSearch, boolCaseInSensitive) == 0);
}

BOOLEAN UnicodeStringEndsWith(PCUNICODE_STRING pString1, PWCHAR pString2, BOOLEAN boolCaseInSensitive)
{
    if ((pString1 == NULL) || (pString1->Buffer == NULL) || (pString1->Length == 0) || (pString2 == NULL))
        return FALSE;

    UNICODE_STRING  usSearch;
    UNICODE_STRING  usStr = *pString1;

    // make a UNICODE_STRING from the search string
    RtlInitUnicodeString(&usSearch, pString2);
    if (usStr.Length < usSearch.Length)
        return FALSE;

    usStr.Buffer += (usStr.Length - usSearch.Length) / 2;
    usStr.Length = usSearch.Length;
    return (RtlCompareUnicodeString(&usStr, &usSearch, boolCaseInSensitive) == 0);
}


BOOLEAN DoesRegValueExist(ULONG RelativeTo, WCHAR *Path, WCHAR *ValueName)
{
    NTSTATUS status;
    RTL_QUERY_REGISTRY_TABLE qrt[2];
    UNICODE_STRING uni;

    // we don't care about the value, but we have to give it a NULL object
    uni.Length = 0;
    uni.MaximumLength = 0;
    uni.Buffer = NULL;

    memzero(qrt, sizeof(qrt));
    qrt[0].Flags = RTL_QUERY_REGISTRY_REQUIRED |
        RTL_QUERY_REGISTRY_DIRECT |
        RTL_QUERY_REGISTRY_NOVALUE |
        RTL_QUERY_REGISTRY_NOEXPAND;
    qrt[0].Name = ValueName;
    qrt[0].EntryContext = &uni;
    qrt[0].DefaultType = REG_NONE;

    status = RtlQueryRegistryValues(
        RelativeTo, Path, qrt, NULL, NULL);

    return (status == STATUS_SUCCESS);
}

void *memmem(const void *pSearchBuf,
    size_t nBufSize,
    const void *pPattern,
    size_t nPatternSize)
{
    UCHAR *pBuf = (UCHAR *)pSearchBuf;
    UCHAR *pEos = pBuf + nBufSize - nPatternSize;

    if (!(pBuf && pEos && nBufSize && nPatternSize))
        return NULL;

    while (pBuf <= pEos) {
        if (*pBuf == *(UCHAR*)pPattern)
            if (memcmp(pBuf, pPattern, nPatternSize) == 0)
                return pBuf;
        ++pBuf;
    }

    return NULL;
}
