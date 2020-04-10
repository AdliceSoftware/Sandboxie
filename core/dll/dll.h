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
// Sandboxie DLL -- private include
//---------------------------------------------------------------------------


#ifndef _MY_DLL_H
#define _MY_DLL_H

#include <ntstatus.h>
#define WIN32_NO_STATUS
typedef long NTSTATUS;

#include <windows.h>
#include "common/win32_ntddk.h"


#include "sbiedll.h"
#include "common/defines.h"
#include "common/list.h"

extern __declspec(dllexport) int __CRTDECL Sbie_swprintf(wchar_t *_Buffer, const wchar_t * const _Format, ...);
extern __declspec(dllexport) int __CRTDECL Sbie_sprintf(char *_Buffer, const char * const _Format, ...);


//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------


#define TRUE_NAME_BUFFER        0
#define COPY_NAME_BUFFER        1
#define NAME_BUFFER_DEPTH       12


#ifdef _WIN64
#define Dll_IsWin64 1
#else
#define Dll_IsWin64 Dll_IsWow64
#endif _WIN64

#ifdef _WIN64

// Pointer to 64-bit PEB_LDR_DATA is at offset 0x0018 of 64-bit PEB
#define GET_ADDR_OF_PEB __readgsqword(0x60)
#define GET_PEB_LDR_DATA (*(PEB_LDR_DATA **)(GET_ADDR_OF_PEB + 0x18))
#define GET_PEB_IMAGE_BASE (*(ULONG_PTR *)(GET_ADDR_OF_PEB + 0x10))
#define GET_PEB_MAJOR_VERSION (*(USHORT *)(GET_ADDR_OF_PEB + 0x118))
#define GET_PEB_MINOR_VERSION (*(USHORT *)(GET_ADDR_OF_PEB + 0x11c))
#define GET_PEB_IMAGE_BUILD (*(USHORT *)(GET_ADDR_OF_PEB + 0x120))

#else

// Pointer to 32-bit PEB_LDR_DATA is at offset 0x000C of 32-bit PEB
#define GET_ADDR_OF_PEB __readfsdword(0x30)
#define GET_PEB_LDR_DATA (*(PEB_LDR_DATA **)(GET_ADDR_OF_PEB + 0x0C))
#define GET_PEB_IMAGE_BASE (*(ULONG_PTR *)(GET_ADDR_OF_PEB + 0x08))
#define GET_PEB_MAJOR_VERSION (*(USHORT *)(GET_ADDR_OF_PEB + 0xa4))
#define GET_PEB_MINOR_VERSION (*(USHORT *)(GET_ADDR_OF_PEB + 0xa8))
#define GET_PEB_IMAGE_BUILD (*(USHORT *)(GET_ADDR_OF_PEB + 0xac))

#endif  _WIN64

enum {
    DLL_IMAGE_UNSPECIFIED = 0,
    DLL_IMAGE_SANDBOXIE_RPCSS,
    DLL_IMAGE_SANDBOXIE_DCOMLAUNCH,
    DLL_IMAGE_SANDBOXIE_CRYPTO,
    DLL_IMAGE_SANDBOXIE_WUAU,
    DLL_IMAGE_SANDBOXIE_BITS,
    DLL_IMAGE_SANDBOXIE_SBIESVC,
    DLL_IMAGE_TRUSTED_INSTALLER,
    DLL_IMAGE_WUAUCLT,
    DLL_IMAGE_SHELL_EXPLORER,
    DLL_IMAGE_INTERNET_EXPLORER,
    DLL_IMAGE_MOZILLA_FIREFOX,
    DLL_IMAGE_WINDOWS_MEDIA_PLAYER,
    DLL_IMAGE_NULLSOFT_WINAMP,
    DLL_IMAGE_PANDORA_KMPLAYER,
    DLL_IMAGE_WINDOWS_LIVE_MAIL,
    DLL_IMAGE_SERVICE_MODEL_REG,
    DLL_IMAGE_RUNDLL32,
    DLL_IMAGE_DLLHOST,
    DLL_IMAGE_DLLHOST_WININET_CACHE,
    DLL_IMAGE_WISPTIS,
    DLL_IMAGE_GOOGLE_CHROME,
    DLL_IMAGE_GOOGLE_UPDATE,
    DLL_IMAGE_ACROBAT_READER,
    DLL_IMAGE_OFFICE_OUTLOOK,
    DLL_IMAGE_OFFICE_EXCEL,
    DLL_IMAGE_FLASH_PLAYER_SANDBOX,
    DLL_IMAGE_PLUGIN_CONTAINER,
    DLL_IMAGE_LAST
};


//---------------------------------------------------------------------------
// Structures and Types
//---------------------------------------------------------------------------
/*
#ifdef _WIN64
typedef struct _MY_LDR_WORKER_QUEUE_STUFF {
    DWORD WorkInProgress;
    DWORD unused[3];
    ULONG_PTR WorkQueueLock;
    DWORD addr1;
    DWORD addr2;
    ULONG_PTR qAddr1;
    ULONG_PTR qAddr2;
    ULONG_PTR qUnused1;
    ULONG_PTR qUnused2;
    ULONG_PTR LdrpWorkQueue;
    ULONG_PTR qAddr3;
    ULONG_PTR LdrpThreadPool;
    ULONG_PTR LdrpLoadCompleteEvent;
    ULONG_PTR LdrpWorkCompleteEvent;
    ULONG_PTR LdrpDetourExist;
}MY_LDR_WORKER_QUEUE_STUFF;
#else
typedef struct _MY_LDR_WORKER_QUEUE_STUFF {
    DWORD WorkInProgress;
    ULONG_PTR WorkQueueLock;
    ULONG_PTR unused[5];
    ULONG_PTR LdrpWorkQueue;
    ULONG_PTR dwAdd1;
    ULONG_PTR LdrpThreadPool;
    ULONG_PTR LdrpLoadCompleteEvent;
    ULONG_PTR LdrpWorkCompleteEvent;
}MY_LDR_WORKER_QUEUE_STUFF;
#endif
*/
typedef struct _THREAD_DATA {

    //
    // name buffers:  first index is for true name, second for copy name
    //

    WCHAR *name_buffer[2][NAME_BUFFER_DEPTH];
    ULONG name_buffer_len[2][NAME_BUFFER_DEPTH];
    int depth;

    //
    // locks
    //

    BOOLEAN key_NtCreateKey_lock;

    BOOLEAN file_NtCreateFile_lock;
    BOOLEAN file_NtClose_lock;
    BOOLEAN file_GetCurDir_lock;

    BOOLEAN ipc_KnownDlls_lock;

    BOOLEAN obj_NtQueryObject_lock;

    //
    // file module
    //

    ULONG file_dont_strip_write_access;

    //
    // proc module:  image path for a child process being started
    //

    ULONG           proc_create_process;
    BOOLEAN         proc_create_process_capture_image;
    BOOLEAN         proc_create_process_force_elevate;
    BOOLEAN         proc_create_process_as_invoker;
    BOOLEAN         proc_image_is_copy;
    WCHAR          *proc_image_path;
    WCHAR          *proc_command_line;

    ULONG           sh32_shell_execute;

    //
    // gui module
    //

    ULONG_PTR       gui_himc;

    HWND            gui_dde_client_hwnd;
    HWND            gui_dde_proxy_hwnd;
    WPARAM          gui_dde_post_wparam;
    LPARAM          gui_dde_post_lparam;

    ULONG           gui_create_window;

    BOOL            gui_should_suppress_msgbox;

    //
    // sbieapi:  SbieSvc port handle
    //

    HANDLE          PortHandle;
    ULONG           MaxDataLen;
    ULONG           SizeofPortMsg;
    BOOLEAN         bOperaFileDlgThread;


} THREAD_DATA;


//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------


extern const ULONG tzuk;

extern HINSTANCE Dll_Instance;
extern HMODULE Dll_Ntdll;
extern HMODULE Dll_Kernel32;
extern HMODULE Dll_KernelBase;
extern HMODULE Dll_DigitalGuardian;

extern const WCHAR *Dll_BoxName;
extern const WCHAR *Dll_ImageName;
extern const WCHAR *Dll_SidString;

extern const WCHAR *Dll_BoxFilePath;
extern const WCHAR *Dll_BoxKeyPath;
extern const WCHAR *Dll_BoxIpcPath;

extern ULONG Dll_BoxFilePathLen;
extern ULONG Dll_BoxKeyPathLen;
extern ULONG Dll_BoxIpcPathLen;
extern ULONG Dll_SidStringLen;

extern ULONG Dll_ProcessId;
extern ULONG Dll_SessionId;

extern ULONG64 Dll_ProcessFlags;

extern BOOLEAN Dll_IsWow64;
extern BOOLEAN Dll_IsSystemSid;
extern BOOLEAN Dll_InitComplete;
extern BOOLEAN Dll_RestrictedToken;
extern BOOLEAN Dll_ChromeSandbox;
extern BOOLEAN Dll_FirstProcessInBox;

extern ULONG Dll_ImageType;

extern ULONG Dll_OsBuild;
extern ULONG Dll_Windows;

extern PSECURITY_DESCRIPTOR Secure_NormalSD;
extern PSECURITY_DESCRIPTOR Secure_EveryoneSD;

extern BOOLEAN Ldr_BoxedImage;

extern WCHAR *Ldr_ImageTruePath;

extern BOOLEAN Ipc_OpenCOM;

extern const WCHAR *Scm_CryptSvc;


//---------------------------------------------------------------------------
// DLL Name Variables
//---------------------------------------------------------------------------


extern const WCHAR *DllName_advapi32;
extern const WCHAR *DllName_combase;
extern const WCHAR *DllName_kernel32;
extern const WCHAR *DllName_kernelbase;
extern const WCHAR *DllName_ole32;
extern const WCHAR *DllName_oleaut32;
extern const WCHAR *DllName_user32;
extern const WCHAR *DllName_rpcrt4;
extern const WCHAR *DllName_winnsi;
extern const WCHAR *DllName_shell32;
extern const WCHAR *DllName_sechost;
extern const WCHAR *DllName_gdi32;
extern const WCHAR *DllName_secur32;
extern const WCHAR *DllName_sspicli;
extern const WCHAR *DllName_mscoree;
extern const WCHAR *DllName_ntmarta;


#define DllName_ole32_or_combase \
    ((Dll_OsBuild >= 8400) ? DllName_combase : DllName_ole32)


//---------------------------------------------------------------------------
// Functions (dllmem)
//---------------------------------------------------------------------------

//void *Ldr_GetProcAddr_4(ULONG_PTR DllBase, char *ProcName, int *flag);
_FX void * myGetProcAddress(HMODULE DllBase, char *ProcName);
BOOLEAN Dll_InitMem(void);

void *Dll_Alloc(ULONG size);
void *Dll_AllocTemp(ULONG size);
void Dll_Free(void *ptr);

void *Dll_AllocCode128(void);
void Dll_FreeCode128(void *ptr);

THREAD_DATA *Dll_GetTlsData(ULONG *pLastError);
void Dll_FreeTlsData(void);

WCHAR *Dll_GetTlsNameBuffer(THREAD_DATA *data, ULONG which, ULONG size);
void Dll_PushTlsNameBuffer(THREAD_DATA *data);
void Dll_PopTlsNameBuffer(THREAD_DATA *data);


//---------------------------------------------------------------------------
// Functions (dllpath)
//---------------------------------------------------------------------------


BOOLEAN Dll_InitPathList(void);

void Dll_RefreshPathList(void);

ULONG SbieDll_MatchPath(WCHAR path_code, const WCHAR *path);

ULONG SbieDll_MatchPath2(WCHAR path_code, const WCHAR *path, BOOLEAN bCheckObjectExists, BOOLEAN bMonitorLog);

#define PATH_OPEN_FLAG      0x10
#define PATH_CLOSED_FLAG    0x20
#define PATH_WRITE_FLAG     0x40

#define PATH_IS_OPEN(f)     (((f) & PATH_OPEN_FLAG) != 0)
#define PATH_NOT_OPEN(f)    (((f) & PATH_OPEN_FLAG) == 0)

#define PATH_IS_CLOSED(f)   (((f) & PATH_CLOSED_FLAG) != 0)
#define PATH_NOT_CLOSED(f)  (((f) & PATH_CLOSED_FLAG) == 0)

#define PATH_IS_WRITE(f)    (((f) & PATH_WRITE_FLAG) != 0)
#define PATH_NOT_WRITE(f)   (((f) & PATH_WRITE_FLAG) == 0)


//---------------------------------------------------------------------------
// Functions (dllmain)
//---------------------------------------------------------------------------


void Dll_InitExeEntry(void);

int Dll_NlsStrCmp(const WCHAR *s1, const WCHAR *s2, ULONG len);

void *Dll_SidStringToSid(const WCHAR *SidString);

NTSTATUS Dll_GetCurrentSidString(UNICODE_STRING *SidString);


//---------------------------------------------------------------------------
// Functions (dllhook)
//---------------------------------------------------------------------------


BOOLEAN Dll_SkipHook(const WCHAR *HookName);

void *Dll_JumpStub(void *OldCode, void *NewCode, ULONG_PTR StubArg);

ULONG_PTR *Dll_JumpStubData(void);

ULONG_PTR *Dll_JumpStubDataForCode(void *StubCode);

#ifdef _WIN64

#define Dll_FixWow64Syscall()

#else ! _WIN64

void Dll_FixWow64Syscall(void);

#endif _WIN64


//---------------------------------------------------------------------------
// Functions (file)
//---------------------------------------------------------------------------


const WCHAR *Pipe_IsNamedPipe(const WCHAR *path);

NTSTATUS Pipe_NtCreateFile(
    HANDLE *FileHandle,
    ACCESS_MASK DesiredAccess,
    OBJECT_ATTRIBUTES *ObjectAttributes,
    IO_STATUS_BLOCK *IoStatusBlock,
    LARGE_INTEGER *AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    void *EaBuffer,
    ULONG EaLength);

void File_DuplicateRecover(HANDLE OldFileHandle, HANDLE NewFileHandle);

void File_DoAutoRecover(BOOLEAN force);

NTSTATUS File_CreateBoxedPath(const WCHAR *PathToCreate);

HANDLE File_GetTrueHandle(HANDLE FileHandle, BOOLEAN *pIsOpenPath);

void *File_AllocAndInitEnvironment(
    void *Environment, BOOLEAN IsUnicode, BOOLEAN AddDeviceMap,
    ULONG *OutLengthInBytes);

WCHAR *File_TranslateDosToNtPath(const WCHAR *DosPath);

WCHAR *File_GetTruePathForBoxedPath(const WCHAR *Path, BOOLEAN IsDosPath);

NTSTATUS File_MyQueryDirectoryFile(
    HANDLE FileHandle,
    void *FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    UNICODE_STRING *FileMask,
    BOOLEAN RestartScan);

NTSTATUS File_AddProxyPipe(HANDLE *OutHandle, ULONG InHandle);

ULONG File_GetProxyPipe(HANDLE FakeHandle, UCHAR *FileIndex);

BOOLEAN File_IsBlockedNetParam(const WCHAR *BoxName);

void File_GetSetDeviceMap(WCHAR *DeviceMap96);


//---------------------------------------------------------------------------
// Functions (key)
//---------------------------------------------------------------------------


NTSTATUS Key_NtDeleteKeyTree(HANDLE KeyHandle, BOOLEAN DeleteTree);
NTSTATUS Key_NtDeleteKeyTreeImpl(HANDLE KeyHandle, BOOLEAN DeleteTree);

NTSTATUS Key_MarkDeletedAndClose(HANDLE KeyHandle);

void Key_DiscardMergeByPath(const WCHAR *TruePath, BOOLEAN Recurse);

void Key_NtClose(HANDLE KeyHandle);

HANDLE Key_GetTrueHandle(HANDLE KeyHandle, BOOLEAN *pIsOpenPath);

NTSTATUS Key_OpenIfBoxed(
    HANDLE *out_handle, ACCESS_MASK access, OBJECT_ATTRIBUTES *objattrs);

NTSTATUS Key_OpenOrCreateIfBoxed(
    HANDLE *out_handle, ACCESS_MASK access, OBJECT_ATTRIBUTES *objattrs);

void Key_DeleteValueFromCLSID(
    const WCHAR *Xxxid, const WCHAR *Guid, const WCHAR *ValueName);


//---------------------------------------------------------------------------
// Functions (sxs)
//---------------------------------------------------------------------------


void Sxs_ActivateDefaultManifest(void *ImageBase);

ULONG Sxs_CheckManifestForCreateProcess(const WCHAR *DosPath);

BOOLEAN Sxs_KeyCallback(const WCHAR *path, HANDLE *out_handle);

BOOLEAN Sxs_FileCallback(const WCHAR *path, HANDLE *out_handle);


//---------------------------------------------------------------------------
// Functions (scm)
//---------------------------------------------------------------------------


HANDLE Scm_OpenKeyForService(
    const WCHAR *ServiceName, BOOLEAN ForWrite);

BOOLEAN Scm_SecHostDll(HMODULE);

void Scm_SecHostDll_W8(void);


//---------------------------------------------------------------------------
// Functions (gui)
//---------------------------------------------------------------------------


BOOLEAN Gui_InitConsole1(void);

void Gui_InitConsole2(void);

BOOLEAN Gui_ConnectToWindowStationAndDesktop(HMODULE User32);

BOOL Gui_EnumWindows(WNDENUMPROC lpEnumFunc, LPARAM lParam);

void Gui_AllowSetForegroundWindow(void);

void Gdi_SplWow64(BOOLEAN Register);

BOOLEAN Gdi_InitZero(void);

void Gui_ResetClipCursor(void);


//---------------------------------------------------------------------------
// Functions (other)
//---------------------------------------------------------------------------


BOOLEAN AdvApi_EnableDisableSRP(BOOLEAN Enable);

void Com_Trace_Guid(WCHAR *text, REFGUID guid, WCHAR *subkey);

ULONG_PTR ProtectCall2(void *CallAddress, ULONG_PTR Arg1, ULONG_PTR Arg2);

ULONG_PTR ProtectCall3(
    void *CallAddress, ULONG_PTR Arg1, ULONG_PTR Arg2, ULONG_PTR Arg3);

ULONG_PTR ProtectCall4(
    void *CallAddress,
    ULONG_PTR Arg1, ULONG_PTR Arg2, ULONG_PTR Arg3, ULONG_PTR Arg4);

BOOL SH32_DoRunAs(
    const WCHAR *CmdLine, const WCHAR *WorkDir,
    PROCESS_INFORMATION *pi, BOOL *cancelled);

ULONG SH_GetInternetExplorerVersion(void);

void *SysInfo_QueryProcesses(ULONG *out_len);

HANDLE Ipc_GetServerEvent(const WCHAR *service, BOOLEAN *create_flag);

ULONG Proc_WaitForParentExit(void *DoExitProcess);

RTL_USER_PROCESS_PARAMETERS *Proc_GetRtlUserProcessParameters(void);

NTSTATUS Proc_SectionCallback(HANDLE FileHandle);

void Secure_InitSecurityDescriptors(void);

BOOLEAN Secure_IsRestrictedToken(BOOLEAN CheckThreadToken);

BOOLEAN Secure_IsLocalSystemToken(BOOLEAN CheckThreadToken);

BOOL Proc_ImpersonateSelf(BOOLEAN Enable);

BOOLEAN Taskbar_SHCore_Init(HMODULE hmodule);


//---------------------------------------------------------------------------
// Functions (init for DllMain)
//---------------------------------------------------------------------------


BOOLEAN Key_Init(void);

BOOLEAN File_Init(void);

BOOLEAN Ipc_Init(void);

BOOLEAN Secure_Init(void);

BOOLEAN SysInfo_Init(void);

BOOLEAN Proc_Init(void);

BOOLEAN Sxs_InitKernel32(void);

BOOLEAN CustomizeSandbox(void);

void Gui_InitWindows7(void);


//---------------------------------------------------------------------------
// Functions (Ldr)
//---------------------------------------------------------------------------


BOOLEAN Ldr_Init(void);

void Ldr_CallDllCallbacks_WithLock(void);

BOOLEAN Ldr_MakeStaticDll(ULONG_PTR BaseAddress);

void *Ldr_GetProcAddrNew(const WCHAR *DllName, const WCHAR *ProcNameW,char *ProcNameA);
void *Ldr_GetProcAddrOld(const WCHAR *DllName, const WCHAR *ProcNameW);

//---------------------------------------------------------------------------
// Functions (called from Ldr Dll Callback)
//---------------------------------------------------------------------------


BOOLEAN AdvApi_Init(HMODULE);

BOOLEAN Scm_Init_AdvApi(HMODULE);

BOOLEAN Proc_Init_AdvApi(HMODULE);

BOOLEAN Cred_Init_AdvApi(HMODULE);

//BOOLEAN Lsa_Init_AdvApi(HMODULE module);

BOOLEAN NcryptProv_Init(HMODULE module);

BOOLEAN Crypt_Init(HMODULE);

BOOLEAN Trust_Init(HMODULE);

BOOLEAN HNet_Init(HMODULE);

BOOLEAN WSA_Init(HMODULE);

BOOLEAN NSI_Init(HMODULE);

BOOLEAN IpHlp_Init(HMODULE);

BOOLEAN NetApi_Init(HMODULE);

BOOLEAN NetApi_Init_WksCli(HMODULE);

BOOLEAN Ole_Init(HMODULE);

BOOLEAN Pst_Init(HMODULE);

BOOLEAN Lsa_Init_Secur32(HMODULE);
BOOLEAN Lsa_Init_SspiCli(HMODULE);

BOOLEAN Setup_Init_SetupApi(HMODULE);

BOOLEAN Setup_Init_CfgMgr32(HMODULE);

BOOLEAN SH32_Init(HMODULE);

BOOLEAN SH32_Init_ZipFldr(HMODULE);

BOOLEAN SH32_Init_UxTheme(HMODULE);

BOOLEAN Gui_Init(HMODULE);

BOOLEAN Gui_Init_IMM32(HMODULE);

BOOLEAN Gui_Init_D3D9(HMODULE);

BOOLEAN Gui_Init_D3D11(HMODULE module); //placeholder for d3d11

BOOLEAN Sfc_Init(HMODULE);

BOOLEAN Terminal_Init_WtsApi(HMODULE);

BOOLEAN Terminal_Init_WinSta(HMODULE);

BOOLEAN EvtApi_Init(HMODULE);

BOOLEAN Sxs_Init(HMODULE);

BOOLEAN Gdi_Full_Init(HMODULE);

BOOLEAN Gdi_Init(HMODULE);

BOOLEAN Gdi_Init_Spool(HMODULE);

BOOLEAN Com_Init_ComBase(HMODULE);

BOOLEAN Com_Init(HMODULE);

BOOLEAN Com_Init_Ole32(HMODULE);

BOOLEAN RpcRt_Init(HMODULE);

BOOLEAN Secure_Init_Elevation(HMODULE);

BOOLEAN UserEnv_Init(HMODULE);

BOOLEAN Scm_OsppcDll(HMODULE);

BOOLEAN Scm_DWriteDll(HMODULE);

BOOLEAN Scm_MsiDll(HMODULE);

BOOLEAN File_MsoDll(HMODULE);

BOOLEAN Proc_Init_AdvPack(HMODULE);

BOOLEAN Custom_SilverlightAgCore(HMODULE);

BOOLEAN Custom_MsgPlusLive(HMODULE);

BOOLEAN Custom_OsppcDll(HMODULE);

BOOLEAN Custom_InternetDownloadManager(HMODULE);

BOOLEAN Custom_Avast_SnxHk(HMODULE);

BOOLEAN Custom_EMET_DLL(HMODULE hmodule);

BOOLEAN Custom_SYSFER_DLL(HMODULE hmodule);

BOOLEAN MsCorEE_Init(HMODULE hmodule);

void Custom_ComServer(void);

void Custom_Load_UxTheme(void);

NTSTATUS StopTailCallOptimization(NTSTATUS status);

BOOLEAN Pdh_Init(HMODULE hmodule);

BOOLEAN NsiRpc_Init(HMODULE);

BOOLEAN Ntmarta_Init(HMODULE);

BOOLEAN Acscmonitor_Init(HMODULE);

BOOLEAN DigitalGuardian_Init(HMODULE);

BOOLEAN ComDlg32_Init(HMODULE);

//---------------------------------------------------------------------------


#endif /* _MY_DLL_H */
