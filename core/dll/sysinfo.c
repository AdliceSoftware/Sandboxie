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
// System Information and Jobs
//---------------------------------------------------------------------------


#include "dll.h"
#include "common/my_version.h"
#include <stdio.h>


//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------


#define OBJECT_ATTRIBUTES_ATTRIBUTES                            \
    (ObjectAttributes                                           \
        ? ObjectAttributes->Attributes | OBJ_CASE_INSENSITIVE   \
        : 0)


//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------


static NTSTATUS SysInfo_NtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    void *Buffer,
    ULONG BufferLength,
    ULONG *ReturnLength);

static void SysInfo_DiscardProcesses(SYSTEM_PROCESS_INFORMATION *buf);

static BOOL SysInfo_SetLocaleInfoW(
    LCID Locale, LCTYPE LCType, void *lpLCData);

static BOOL SysInfo_SetLocaleInfoA(
    LCID Locale, LCTYPE LCType, void *lpLCData);

static NTSTATUS SysInfo_NtTraceEvent(
    HANDLE TraceHandle, ULONG Flags, ULONG FieldSize, PVOID Fields);
 
static NTSTATUS SysInfo_NtDeleteWnfStateData(
    ULONG_PTR UnknownParameter1, ULONG_PTR UnknownParameter2);

static NTSTATUS SysInfo_NtCreateJobObject(
    HANDLE *JobHandle, ACCESS_MASK DesiredAccess,
    OBJECT_ATTRIBUTES *ObjectAttributes);

static NTSTATUS SysInfo_NtAssignProcessToJobObject(
    HANDLE JobHandle, HANDLE ProcessHandle);

static NTSTATUS SysInfo_NtSetInformationJobObject(
    HANDLE JobHandle, JOBOBJECTINFOCLASS JobObjectInformationClass,
    void *JobObjectInformtion, ULONG JobObjectInformtionLength);

static void SysInfo_JobCallbackData_Set(
    HANDLE ProcessHandle, void *JobPortInformation);

static ULONG SysInfo_JobCallbackData_Thread(void *xHandles);


//---------------------------------------------------------------------------


typedef BOOL (*P_SetLocaleInfo)(LCID Locale, LCTYPE LCType, void *lpLCData);


//---------------------------------------------------------------------------


static P_NtQuerySystemInformation   __sys_NtQuerySystemInformation  = NULL;

static P_SetLocaleInfo              __sys_SetLocaleInfoW            = NULL;
static P_SetLocaleInfo              __sys_SetLocaleInfoA            = NULL;

static P_NtCreateJobObject          __sys_NtCreateJobObject         = NULL;
static P_NtAssignProcessToJobObject __sys_NtAssignProcessToJobObject = NULL;
static P_NtSetInformationJobObject  __sys_NtSetInformationJobObject = NULL;

static P_NtTraceEvent  __sys_NtTraceEvent = NULL;


//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------


static ULONG_PTR *SysInfo_JobCallbackData = NULL;


//---------------------------------------------------------------------------
// SysInfo_Init
//---------------------------------------------------------------------------


_FX BOOLEAN SysInfo_Init(void)
{
    void *NtTraceEvent;

    if (! Dll_SkipHook(L"ntqsi")) {

        SBIEDLL_HOOK(SysInfo_,NtQuerySystemInformation);
    }

    SBIEDLL_HOOK(SysInfo_,NtCreateJobObject);
    SBIEDLL_HOOK(SysInfo_,NtAssignProcessToJobObject);
    SBIEDLL_HOOK(SysInfo_,NtSetInformationJobObject);

    SBIEDLL_HOOK(SysInfo_,SetLocaleInfoW);
    SBIEDLL_HOOK(SysInfo_,SetLocaleInfoA);

    //
    // we don't want to hook NtTraceEvent in kernel mode
    // so we hook it in user mode
    //

    NtTraceEvent = GetProcAddress(Dll_Ntdll, "NtTraceEvent");
    if (NtTraceEvent) {

        SBIEDLL_HOOK(SysInfo_, NtTraceEvent);
    }

    if (Dll_OsBuild >= 8400) {

        //
        // on Windows 8, a new API returns STATUS_ACCESS_DENIED
        //

        void *NtDeleteWnfStateData =
                        GetProcAddress(Dll_Ntdll, "NtDeleteWnfStateData");
        if (NtDeleteWnfStateData) {

            P_NtDeleteWnfStateData __sys_NtDeleteWnfStateData = NULL;
            SBIEDLL_HOOK(SysInfo_,NtDeleteWnfStateData);
        }
    }

    return TRUE;
}


//---------------------------------------------------------------------------
// SysInfo_NtQuerySystemInformation
//---------------------------------------------------------------------------


_FX NTSTATUS SysInfo_NtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    void *Buffer,
    ULONG BufferLength,
    ULONG *ReturnLength)
{
    NTSTATUS status;

    status = __sys_NtQuerySystemInformation(
        SystemInformationClass, Buffer, BufferLength, ReturnLength);

    if (NT_SUCCESS(status) &&
            SystemInformationClass == SystemProcessInformation) {

        SysInfo_DiscardProcesses(Buffer);
    }

    return status;
}


//---------------------------------------------------------------------------
// SysInfo_DiscardProcesses
//---------------------------------------------------------------------------


_FX void SysInfo_DiscardProcesses(SYSTEM_PROCESS_INFORMATION *buf)
{
    SYSTEM_PROCESS_INFORMATION *curr = buf;
    SYSTEM_PROCESS_INFORMATION *next;
    WCHAR boxname[48];

    //
    // we assume the first record is always going to be the idle process or
    // a system process -- in any case, one we're not going to have to skip
    //

    while (1) {

        next = (SYSTEM_PROCESS_INFORMATION *)
                    (((UCHAR *)curr) + curr->NextEntryOffset);
        if (next == curr)
            return;

        SbieApi_QueryProcess(
            next->UniqueProcessId, boxname, NULL, NULL, NULL);

        if ((! boxname[0]) || _wcsicmp(boxname, Dll_BoxName) == 0)
            curr = next;
        else if (next->NextEntryOffset)
            curr->NextEntryOffset += next->NextEntryOffset;
        else
            curr->NextEntryOffset = 0;
    }
}


//---------------------------------------------------------------------------
// SysInfo_SetLocaleInfoW
//---------------------------------------------------------------------------


_FX BOOL SysInfo_SetLocaleInfoW(LCID Locale, LCTYPE LCType, void *lpLCData)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}


//---------------------------------------------------------------------------
// SysInfo_SetLocaleInfoA
//---------------------------------------------------------------------------


_FX BOOL SysInfo_SetLocaleInfoA(LCID Locale, LCTYPE LCType, void *lpLCData)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}


//---------------------------------------------------------------------------
// SysInfo_NtTraceEvent
//---------------------------------------------------------------------------


_FX NTSTATUS SysInfo_NtTraceEvent(
    HANDLE TraceHandle, ULONG Flags, ULONG FieldSize, PVOID Fields)
{
    // this prevents the CAPI2 application event 'The Cryptographic Services service failed to initialize the VSS backup "System Writer" object.'
    if (Dll_ImageType == DLL_IMAGE_SANDBOXIE_CRYPTO) {
        return STATUS_ACCESS_DENIED;
    }
    return __sys_NtTraceEvent(TraceHandle, Flags, FieldSize, Fields);
}


//---------------------------------------------------------------------------
// SysInfo_NtDeleteWnfStateData
//---------------------------------------------------------------------------


static NTSTATUS SysInfo_NtDeleteWnfStateData(
    ULONG_PTR UnknownParameter1, ULONG_PTR UnknownParameter2)
{
    if (Dll_ImageType != DLL_IMAGE_SANDBOXIE_RPCSS)
        SbieApi_Log(2205, L"NtDeleteWnfStateData (%S)", Dll_ImageName);
    return STATUS_SUCCESS;
}


//---------------------------------------------------------------------------
// SysInfo_QueryProcesses
//---------------------------------------------------------------------------


_FX void *SysInfo_QueryProcesses(ULONG *out_len)
{
    SYSTEM_PROCESS_INFORMATION *info;
    ULONG len, i;
    NTSTATUS status;

    info = NULL;
    len  = 8192;

    for (i = 0; i < 5; ++i) {

        info = Dll_AllocTemp(len);

        status = NtQuerySystemInformation(
                            SystemProcessInformation, info, len, &len);
        if (NT_SUCCESS(status))
            break;

        Dll_Free(info);
        info = NULL;

        if (status == STATUS_BUFFER_OVERFLOW ||
            status == STATUS_INFO_LENGTH_MISMATCH ||
            status == STATUS_BUFFER_TOO_SMALL) {

            len += 64;
            continue;
        }

        break;
    }

    if (out_len)
        *out_len = len;
    return info;
}


//---------------------------------------------------------------------------
// SysInfo_NtCreateJobObject
//---------------------------------------------------------------------------


_FX NTSTATUS SysInfo_NtCreateJobObject(
    HANDLE *JobHandle, ACCESS_MASK DesiredAccess,
    OBJECT_ATTRIBUTES *ObjectAttributes)
{
    static volatile ULONG _JobCounter = 0;
    OBJECT_ATTRIBUTES objattrs;
    UNICODE_STRING objname;
    WCHAR *jobname;
    ULONG jobname_len;
    NTSTATUS status;

    //
    // the driver requires us to specify a sandboxed path name to a
    // job object, and to not request some specific rights
    //

    DesiredAccess &= ~(JOB_OBJECT_ASSIGN_PROCESS | JOB_OBJECT_TERMINATE);

    jobname_len = Dll_BoxIpcPathLen + wcslen(Dll_ImageName) + 64;
    jobname = Dll_AllocTemp(jobname_len * sizeof(WCHAR));

    while (1) {

        InterlockedIncrement(&_JobCounter);
        Sbie_swprintf(jobname, L"%s\\%s_DummyJob_%s_%d",
                        Dll_BoxIpcPath, SBIE, Dll_ImageName, _JobCounter);
        RtlInitUnicodeString(&objname, jobname);

        InitializeObjectAttributes(&objattrs,
            &objname, OBJECT_ATTRIBUTES_ATTRIBUTES, NULL, Secure_EveryoneSD);

        status = __sys_NtCreateJobObject(
                                JobHandle, DesiredAccess, &objattrs);

        //
        // we always start each process with _JobCounter = 0 so we have to
        // account for the case where a dummy job object was already created
        // by another process, and not fail the request
        //

        if (status != STATUS_OBJECT_NAME_COLLISION)
            break;
    }

    Dll_Free(jobname);
    return status;
}


//---------------------------------------------------------------------------
// SysInfo_NtAssignProcessToJobObject
//---------------------------------------------------------------------------


_FX NTSTATUS SysInfo_NtAssignProcessToJobObject(
    HANDLE JobHandle, HANDLE ProcessHandle)
{
    HANDLE DuplicatedProcessHandle;

    if (0 == NtDuplicateObject(
                NtCurrentProcess(), ProcessHandle,
                NtCurrentProcess(), &DuplicatedProcessHandle,
                PROCESS_QUERY_INFORMATION | SYNCHRONIZE, 0, 0)) {

        SysInfo_JobCallbackData_Set(DuplicatedProcessHandle, NULL);
    }

    return STATUS_SUCCESS;
}


//---------------------------------------------------------------------------
// SysInfo_NtSetInformationJobObject
//---------------------------------------------------------------------------


_FX NTSTATUS SysInfo_NtSetInformationJobObject(
    HANDLE JobHandle, JOBOBJECTINFOCLASS JobObjectInformationClass,
    void *JobObjectInformtion, ULONG JobObjectInformtionLength)
{
    NTSTATUS status = __sys_NtSetInformationJobObject(
        JobHandle, JobObjectInformationClass,
        JobObjectInformtion, JobObjectInformtionLength);

    if (NT_SUCCESS(status) &&
            JobObjectInformationClass ==
                    JobObjectAssociateCompletionPortInformation) {

        SysInfo_JobCallbackData_Set(NULL, JobObjectInformtion);
    }

    return status;
}


//---------------------------------------------------------------------------
// SysInfo_JobCallbackData_Set
//---------------------------------------------------------------------------


_FX void SysInfo_JobCallbackData_Set(
    HANDLE ProcessHandle, void *JobPortInformation)
{
    ULONG_PTR *Handles = SysInfo_JobCallbackData;
    if (! Handles) {
        Handles = Dll_Alloc(sizeof(ULONG_PTR) * 4);
        Handles[3] = 0;
    }

    if (JobPortInformation) {
        // copy CompletionKey and CompletionPort from
        // JOBOBJECT_ASSOCIATE_COMPLETION_PORT structure
        memcpy(Handles, JobPortInformation, sizeof(ULONG_PTR) * 2);
        Handles[3] |= 1;
    }

    if (ProcessHandle) {
        Handles[2] = (ULONG_PTR)ProcessHandle;
        Handles[3] |= 2;
    }

    if ((Handles[3] & 3) == 3) {

        HANDLE ThreadHandle = CreateThread(
            NULL, 0, SysInfo_JobCallbackData_Thread, Handles, 0, NULL);

        if (ThreadHandle)
            CloseHandle(ThreadHandle);

        Handles = NULL;
    }

    SysInfo_JobCallbackData = Handles;
}


//---------------------------------------------------------------------------
// SysInfo_JobCallbackData_Thread
//---------------------------------------------------------------------------


_FX ULONG SysInfo_JobCallbackData_Thread(void *xHandles)
{
    ULONG_PTR *Handles      = (ULONG_PTR *)xHandles;
    ULONG_PTR CompletionKey = Handles[0];
    HANDLE CompletionPort   = (HANDLE)Handles[1];
    HANDLE ProcessHandle    = (HANDLE)Handles[2];

    PROCESS_BASIC_INFORMATION info;
    ULONG len;

    Dll_Free(Handles);

    //
    // when a completion port is set, the job completion port receives
    // JOB_OBJECT_MSG_NEW_PROCESS notifications for processes in the job
    //

    if (0 != NtQueryInformationProcess(
                ProcessHandle, ProcessBasicInformation,
                &info, sizeof(PROCESS_BASIC_INFORMATION), &len))
        info.UniqueProcessId = 0;

    PostQueuedCompletionStatus(
        CompletionPort, JOB_OBJECT_MSG_NEW_PROCESS, CompletionKey,
        (LPOVERLAPPED)info.UniqueProcessId);

    //
    // send JOB_OBJECT_MSG_EXIT_PROCESS when the process ends
    //

    if (WaitForSingleObject(ProcessHandle, INFINITE) == 0) {

        CloseHandle(ProcessHandle);
        PostQueuedCompletionStatus(
            CompletionPort, JOB_OBJECT_MSG_EXIT_PROCESS, CompletionKey,
            (LPOVERLAPPED)info.UniqueProcessId);
    }

    return 0;
}
