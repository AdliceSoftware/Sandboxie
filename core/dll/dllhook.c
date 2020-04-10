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
// SbieDLL Hook Management
//---------------------------------------------------------------------------


#define NOGDI
#include "dll.h"
#include "common/pool.h"
#include "common/pattern.h"

//#include <stdio.h>


//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------


static void *SbieDll_Hook_CheckChromeHook(void *SourceFunc);

static WCHAR *Dll_GetSettingsForImageName(
    const WCHAR *setting, const WCHAR *deftext);

ULONG_PTR  DLL_FindWow64Target(ULONG_PTR address);

//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------


#ifdef _WIN64

typedef struct _VECTOR_TABLE {
  void * offset;
  int index;
  int maxEntries;
} VECTOR_TABLE;

BOOL bVTableEable = TRUE;
#define NUM_VTABLES 0x10 
#define VTABLE_SIZE 0x4000 //16k enough for 2048 8 byte entrys

VECTOR_TABLE SbieDllVectorTable[NUM_VTABLES] = {
    {0,0,0},{0,0,0},{0,0,0},{0,0,0},
    {0,0,0},{0,0,0},{0,0,0},{0,0,0},
    {0,0,0},{0,0,0},{0,0,0},{0,0,0},
    {0,0,0},{0,0,0},{0,0,0},{0,0,0}
};

extern CRITICAL_SECTION VT_CriticalSection;
#endif _WIN64
extern ULONG Dll_Windows;


//---------------------------------------------------------------------------
// SbieDll_Hook
//---------------------------------------------------------------------------


_FX void *SbieDll_Hook(
    const char *SourceFuncName, void *SourceFunc, void *DetourFunc)
{
    static const WCHAR *_fmt1 = L"%s (%d)";
    static const WCHAR *_fmt2 = L"%s (%d, %d)";
    UCHAR *tramp, *func;
    ULONG prot, dummy_prot;
    ULONG_PTR diff;
    ULONG_PTR target;
#ifdef _WIN64
    long long delta;
    BOOLEAN CallInstruction64 = FALSE;
#endif _WIN64

    //
    // validate parameters
    //

    if (! SourceFunc) {
        SbieApi_Log(2303, _fmt1, SourceFuncName, 1);
        return NULL;
    }

    //
    // Chrome sandbox support
    //

    SourceFunc = SbieDll_Hook_CheckChromeHook(SourceFunc);

    //
    // if the source function begins with relative jump EB xx, it means
    // someone else has already hooked it, so we try to hook the hook
    // (this helps to co-exist with Cisco Security Agent)
    //

    if (*(UCHAR *)SourceFunc == 0xEB) {
        signed char offset = *((signed char *)SourceFunc + 1);
        SourceFunc = (UCHAR *)SourceFunc + offset + 2;
    }

    //
    // if the source function begins with a near jump E9 xx xx xx xx
    // then follow the jump to its destination.  if the destination
    // turns out to be the same as DetourFunc, then abort,
    // otherwise (for 32-bit code) just replace the jump target
    //

    while (*(UCHAR *)SourceFunc == 0xE9) {

        diff = *(LONG *)((ULONG_PTR)SourceFunc + 1);
        target = (ULONG_PTR)SourceFunc + diff + 5;
        if (target == (ULONG_PTR)DetourFunc) {
            SbieApi_Log(2303, _fmt1, SourceFuncName, 4);
            return NULL;
        }

#ifdef _WIN64

        SourceFunc = (void *)target;

#else ! WIN_64

        func = SbieDll_Hook_CheckChromeHook((void *)target);
        if (func != (void *)target) {
            SourceFunc = func;
            goto skip_e9_rewrite;
        }

        func = (UCHAR *)SourceFunc;
        diff = (UCHAR *)DetourFunc - (func + 5);
        ++func;
        if (! VirtualProtect(func, 4, PAGE_EXECUTE_READWRITE, &prot)) {
            ULONG err = GetLastError();
            SbieApi_Log(2303, _fmt2, SourceFuncName, 31, err);
            return NULL;
        }
        *(ULONG *)func = (ULONG)diff;
        VirtualProtect(func, 4, prot, &dummy_prot);

        return (void *)target;

skip_e9_rewrite: ;

#endif _WIN64

    }

#ifdef _WIN64

    //
    // 64-bit only:  avast snxhk64.dll compatibility:  the function
    // begins with nop+jmp (90,E9), and the jump target is a second
    // jump instruction 'jmp qword ptr [x]', then advance the pointer
    // to the second jump instruction so the next block of code
    // can process it
    //

    if (*(USHORT *)SourceFunc == 0xE990) {
        diff = *(LONG *)((ULONG_PTR)SourceFunc + 2);
        target = (ULONG_PTR)SourceFunc + diff + 6;
        if (*(USHORT *)target == 0x25FF)
            SourceFunc = (void *)target;
    }

    //
    // 64-bit only:  if the function begins with 'jmp qword ptr [x]'
    // (6 bytes) then replace the value at x, rather than overwrite
    // 12 bytes.
    //

    if (*(UCHAR *)SourceFunc == 0x48 &&
            *(USHORT *)((UCHAR *)SourceFunc + 1) == 0x25FF) {
        // 4825FF is same as 25FF
        SourceFunc = (UCHAR *)SourceFunc + 1;
    }

    if (*(USHORT *)SourceFunc == 0x25FF) {

        void *orig_addr;
        /*
        sprintf(buffer,"0x25FF Hook: %s\n",SourceFuncName);
        OutputDebugStringA(buffer);
        */
        diff = *(LONG *)((ULONG_PTR)SourceFunc + 2);
        target = (ULONG_PTR)SourceFunc + 6 + diff;
        orig_addr = (void *)*(ULONG_PTR *)target;
        if (orig_addr == DetourFunc) {
            SbieApi_Log(2303, _fmt1, SourceFuncName, 4);
            return NULL;
        }

        func = (UCHAR *)target;
        if (! VirtualProtect(func, 8, PAGE_EXECUTE_READWRITE, &prot)) {
            ULONG err = GetLastError();
            SbieApi_Log(2303, _fmt2, SourceFuncName, 32, err);
            return NULL;
        }
        *(ULONG_PTR *)target = (ULONG_PTR)DetourFunc;
        VirtualProtect(func, 8, prot, &dummy_prot);

        return orig_addr;
    }

#endif _WIN64

    //
    // 64-bit only:  if the function begins with 'call qword ptr [x]'
    // (6 bytes) then overwrite at the call target address.
    //

#ifdef _WIN64

    if (*(USHORT *)SourceFunc == 0x15FF) {

        //
        // the call instruction pushes a qword into the stack, we need
        // to remove this qword before calling our detour function
        //

        UCHAR *NewDetour = Dll_AllocCode128();

        NewDetour[0] = 0x58;        // pop rax
        NewDetour[1] = 0x48;        // mov rax, DetourFunc
        NewDetour[2] = 0xB8;
        *(ULONG_PTR *)(&NewDetour[3]) = (ULONG_PTR)DetourFunc;
        NewDetour[11] = 0xFF;       // jmp rax
        NewDetour[12] = 0xE0;

        DetourFunc = NewDetour;

        //
        // when our detour function calls the trampoline to invoke the
        // original code, we have to push the qword back into the stack,
        // because this is what the original code expects
        //

        NewDetour[16] = 0x48;       // mov rax, SourceFunc+6
        NewDetour[17] = 0xB8;
        *(ULONG_PTR *)(&NewDetour[18]) = (ULONG_PTR)SourceFunc + 6;
        NewDetour[26] = 0x50;       // push rax
        NewDetour[27] = 0x48;       // mov rax, trampoline code
        NewDetour[28] = 0xB8;
        *(ULONG_PTR *)(&NewDetour[29]) = 0;
        NewDetour[37] = 0xFF;       // jmp rax
        NewDetour[38] = 0xE0;

        CallInstruction64 = TRUE;

        //
        // overwrite the code at the target of the call instruction
        //

        diff = *(LONG *)((ULONG_PTR)SourceFunc + 2);
        target = (ULONG_PTR)SourceFunc + 6 + diff;
        SourceFunc = (void *)*(ULONG_PTR *)target;
    }

#endif _WIN64

    //
    // invoke the driver to create a trampoline
    //

    tramp = Dll_AllocCode128();

    if (SbieApi_HookTramp(SourceFunc, tramp) != 0) {
        SbieApi_Log(2303, _fmt1, SourceFuncName, 2);
        return NULL;
    }

    //
    // create the detour
    //

    func = (UCHAR *)SourceFunc;

    if (!VirtualProtect(&func[-8], 20, PAGE_EXECUTE_READWRITE, &prot)) {

        ULONG err = GetLastError();
        SbieApi_Log(2303, _fmt2, SourceFuncName, 33, err);
        return NULL;
    }

    //
    // hook the source function
    //

#ifdef _WIN64
    if (Dll_Windows >= 10) {
        target = (ULONG_PTR)&func[6];
    }
    else {
        target = (ULONG_PTR)&func[5];
    }

    diff = (ULONG_PTR)((ULONG_PTR)DetourFunc - target);
    delta = diff;
    delta < 0 ? delta *= -1 : delta;

    //is DetourFunc in 32bit jump range
    if (delta < 0x80000000) {
        /*
        sprintf(buffer,"32 bit Hook: %s\n",SourceFuncName);
        OutputDebugStringA(buffer);
        */
        if (Dll_Windows >= 10) {
            func[0] = 0x48;             // 32bit relative JMP DetourFunc
            func[1] = 0xE9;             // 32bit relative JMP DetourFunc
            *(ULONG *)(&func[2]) = (ULONG)diff;
        }
        else {
            func[0] = 0xE9;             // 32bit relative JMP DetourFunc
            *(ULONG *)(&func[1]) = (ULONG)diff;
        }
    }

    else {



        BOOLEAN hookset = FALSE;
        BOOLEAN defaultRange = FALSE;
        int i;
        EnterCriticalSection(&VT_CriticalSection);

        if (bVTableEable) {
            VECTOR_TABLE *ptrVTable = SbieDllVectorTable;
            //default step size 

            for (i = 0; i < NUM_VTABLES && !hookset; i++, ptrVTable++) {
                if (!ptrVTable->offset) {
                    ULONG_PTR tempAddr;
                    ULONG_PTR step = 0x20000;// + VTABLE_SIZE;
                    ULONG_PTR max_attempts = 0x4000000 / step;
                    // optimization for windows 7 and low memory DLL's

                    if ((ULONG_PTR)func < 0x80000000 && ((ULONG_PTR)func > 0x4000000)) {
                        step = 0x200000;
                    }
                    // optimization for windows 8.1
                    else if ((ULONG_PTR)func < 0x4000000) {
                        step *= -1;
                    }
                    else if ((ULONG_PTR)func < 0x10000000000) {
                        step *= -1;
                    }
                    else {
                        defaultRange = TRUE;
                    }

                    // sprintf(buffer,"VTable Alloc: func = %p, step = %p, default = %d\n",func,step,defaultRange);
                    // OutputDebugStringA(buffer);
                    tempAddr = ((ULONG_PTR)func & 0xfffffffffffe0000) - (step << 2);

                    if (defaultRange) {
                        tempAddr -= 0x20000000;
                    }

                    for (; !ptrVTable->offset && max_attempts; tempAddr -= step, max_attempts--) {
                        ptrVTable->offset = VirtualAlloc((void *)tempAddr, VTABLE_SIZE, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
                        //  sprintf(buffer,"VTable Offset: func = %p, offset = %p, tryAddress = %p, attempt = 0x%x\n",func,ptrVTable->offset,tempAddr,max_attempts);
                        //  OutputDebugStringA(buffer);
                    }

                    ptrVTable->index = 0;
                    ptrVTable->maxEntries = VTABLE_SIZE / sizeof(void *);
                }
                if (ptrVTable->offset) {
                    target = (ULONG_PTR)&func[6];
                    diff = (ULONG_PTR) &((ULONG_PTR *)ptrVTable->offset)[ptrVTable->index];
                    diff = diff - target;
                    delta = diff;
                    delta < 0 ? delta *= -1 : delta;

                    // is DetourFunc in 32bit jump range
                    if (delta < 0x80000000 && ptrVTable->index <= ptrVTable->maxEntries) {
                        ((ULONG_PTR *)ptrVTable->offset)[ptrVTable->index] = (ULONG_PTR)DetourFunc;
                        *(USHORT *)&func[0] = 0x25ff;
                        *(ULONG *)&func[2] = (ULONG)diff;
                        ptrVTable->index++;
                        hookset = TRUE;
                    }
                }
                else {
                    bVTableEable = FALSE;
                    SbieApi_Log(2303, _fmt1, SourceFuncName, 888);
                    LeaveCriticalSection(&VT_CriticalSection);
                    return NULL;
                }
            }
        }

        LeaveCriticalSection(&VT_CriticalSection);
        if (!hookset) {
            // OutputDebugStringA("Memory alloc failed: 12 Byte Patch Disabled\n");
            SbieApi_Log(2303, _fmt1, SourceFuncName, 999);
            return NULL;
        }
    }

#else
    diff = (UCHAR *)DetourFunc - (func + 5);
    func[0] = 0xE9;             // JMP DetourFunc
    *(ULONG *)(&func[1]) = (ULONG)diff;
#endif

    VirtualProtect(&func[-8], 20, prot, &dummy_prot);

    // the trampoline code begins at trampoline + 16 bytes
    func = (UCHAR *)(ULONG_PTR)(tramp + 16);

    //
    // 64-bit only:  if we are hooking a function that started with a
    // call instruction, then we have to return a secondary trampoline
    //

#ifdef _WIN64

    if (CallInstruction64) {

        UCHAR *NewDetour = (UCHAR *)DetourFunc;
        *(ULONG_PTR *)(&NewDetour[29]) = (ULONG_PTR)func;
        func = NewDetour + 16;
    }

#endif _WIN64

    return func;
}


//---------------------------------------------------------------------------
// SbieDll_Hook_CheckChromeHook
//---------------------------------------------------------------------------
#ifdef _WIN64
ULONGLONG * SbieDll_findChromeTarget(unsigned char* addr);
#define MAX_FUNC_SIZE 0x76
//Note any change to this function requires the same modification to the function in LowLevel: see init.c (findChromeTarget)
ULONGLONG * SbieDll_findChromeTarget(unsigned char* addr)
{
    int i = 0;
    ULONGLONG target;
    ULONGLONG * ChromeTarget = NULL;
    if (!addr) return NULL;
    //Look for mov rcx,[target 4 byte offset] or in some cases mov rax,[target 4 byte offset]
    //So far the offset has been positive between 0xa00000 and 0xb00000 bytes;
    //This may change in a future version of chrome
    for (i = 0; i < MAX_FUNC_SIZE; i++) {
        if ((*(USHORT *)&addr[i] == 0x8b48)) {
            if ((addr[i + 2] == 0x0d || addr[i + 2] == 0x05)) {
                LONG delta;
                target = (ULONG_PTR)(addr + i + 7);
                delta = *(LONG *)&addr[i + 3];
                //check if offset is close to the expected value (is positive and less than 0x100000 as of chrome 64) 
        //      if (delta > 0 && delta < 0x100000 )  { //may need to check delta in a future version of chrome
                target += delta;
                ChromeTarget = *(ULONGLONG **)target;
                //}
                break;
            }
        }
    }

    return ChromeTarget;
}
#endif

_FX void *SbieDll_Hook_CheckChromeHook(void *SourceFunc)
{
#ifndef _WIN64

    UCHAR *func = (UCHAR *)SourceFunc;
    if (func[0] == 0xB8 &&                  // mov eax,?
        func[5] == 0xBA &&                  // mov edx,?
        *(USHORT *)&func[10] == 0xE2FF)     // jmp edx
    {
        ULONG i = 0;
        ULONG *longs = *(ULONG **)&func[6];

        for (i = 0; i < 20; i++, longs++)
        {
            if (longs[0] == 0x5208EC83 && longs[1] == 0x0C24548B &&
                longs[2] == 0x08245489 && longs[3] == 0x0C2444C7 &&
                longs[5] == 0x042444C7)
            {
                SourceFunc = (void *)longs[4];
                break;
            }
        }
    }
#else if  
    UCHAR *func = (UCHAR *)SourceFunc;
    ULONGLONG *chrome64Target = NULL;
    if (!SourceFunc)
        return NULL;

    if (func[0] == 0x50 && func[1] == 0x48 && func[2] == 0xb8) {
        ULONGLONG *longlongs = *(ULONGLONG **)&func[3];
        chrome64Target = SbieDll_findChromeTarget((unsigned char *)longlongs);
    }
    // Chrome 49+ 64bit hook
    // mov rax, <target> 
    // jmp rax 
    else if (func[0] == 0x48 && func[1] == 0xb8 && *(USHORT *)&func[10] == 0xe0ff) {
        ULONGLONG *longlongs = *(ULONGLONG **)&func[2];
        chrome64Target = SbieDll_findChromeTarget((unsigned char *)longlongs);
    }
    if (chrome64Target) {
        SourceFunc = chrome64Target;
    }
    /*sboxie 64bit jtable hook signature */
        /* // use this to hook jtable location (useful for debugging)
        else if(func[0] == 0x51 && func[1] == 0x48 && func[2] == 0xb8 ) {
            long long addr;
            addr = (ULONG_PTR) *(ULONGLONG **)&func[3] ;
            SourceFunc = (void *) addr;
        }
        */
#endif ! _WIN64
    return SourceFunc;
}


//---------------------------------------------------------------------------
// Dll_GetSettingsForImageName
//---------------------------------------------------------------------------


_FX WCHAR *Dll_GetSettingsForImageName(
    const WCHAR *setting, const WCHAR *deftext)
{
    POOL *pool;
    WCHAR *text, *image_lwr, *buf;
    ULONG text_len, image_len;
    ULONG index;

    //
    //
    //

    pool = Pool_Create();
    if (! pool)
        goto outofmem;

    //
    //
    //

    if (deftext)
        text_len = wcslen(deftext);
    else
        text_len = 0;
    text = Pool_Alloc(pool, (text_len + 1) * sizeof(WCHAR));
    if (! text)
        goto outofmem;
    wmemcpy(text, deftext, text_len);
    text[text_len] = L'\0';

    //
    //
    //

    image_len = (wcslen(Dll_ImageName) + 1) * sizeof(WCHAR);
    image_lwr = Pool_Alloc(pool, image_len);
    if (! image_lwr)
        goto outofmem;
    memcpy(image_lwr, Dll_ImageName, image_len);
    _wcslwr(image_lwr);
    image_len = wcslen(image_lwr);

    //
    //
    //

    buf = Pool_Alloc(pool, 1024 * sizeof(WCHAR));
    if (! buf)
        goto outofmem;

    index = 0;
    while (1) {

        WCHAR *ptr, *buf_ptr;
        PATTERN *image_pat;

        NTSTATUS status = SbieApi_QueryConfAsIs(
                    NULL, setting, index, buf, 1020 * sizeof(WCHAR));
        if (! NT_SUCCESS(status))
            break;
        ++index;

        ptr = wcschr(buf, L',');
        if (! ptr)
            continue;
        *ptr = L'\0';

        if (buf[0] == L'/' && buf[1] == L'/' &&
                (Dll_ProcessFlags & SBIE_FLAG_IMAGE_FROM_SBIE_DIR)) {
            // a // prefix in the image name (such as //start.exe) matches
            // only if the image resides in our installation directory
            buf_ptr = buf + 2;
        } else
            buf_ptr = buf;

        image_pat = Pattern_Create(pool, buf_ptr, TRUE);
        if (Pattern_Match(image_pat, image_lwr, image_len)) {

            ULONG ptr_len;
            WCHAR *new_text;
            if (text_len)
                *ptr = L',';    // restore comma if text is not empty
            else
                ++ptr;          // or skip comma if text is empty
            ptr_len = wcslen(ptr);
            new_text = Pool_Alloc(pool,
                            (text_len + ptr_len + 1) * sizeof(WCHAR));
            if (! new_text)
                goto outofmem;
            wmemcpy(new_text, text, text_len);
            wmemcpy(new_text + text_len, ptr, ptr_len + 1);
            text = new_text;
            text_len = text_len + ptr_len;
        }

        Pattern_Free(image_pat);
    }

    //
    // finish
    //

    buf = Dll_Alloc((text_len + 1) * sizeof(WCHAR));
    wmemcpy(buf, text, text_len + 1);

    Pool_Delete(pool);

    return buf;

outofmem:

    SbieApi_Log(2305, NULL);
    ExitProcess(-1);
    return NULL;
}


//---------------------------------------------------------------------------
// Dll_SkipHook
//---------------------------------------------------------------------------


_FX BOOLEAN Dll_SkipHook(const WCHAR *HookName)
{
    static WCHAR *HookText = NULL;
    BOOLEAN found = FALSE;

    //
    // initialize hook text based on image name
    //

    if (! HookName) {

        const WCHAR *deftext = NULL;

        if (_wcsicmp(Dll_ImageName, L"DragonSaga.exe") == 0)
            deftext = L"ntqsi,enumwin,findwin";

        if (_wcsicmp(Dll_ImageName, L"BatmanAC.exe") == 0)
            deftext = L"enumwin,findwin";

        if (_wcsicmp(Dll_ImageName, L"PotPlayer64.exe") == 0 ||
            _wcsicmp(Dll_ImageName, L"PotPlayerMini64.exe") == 0 ||
            _wcsicmp(Dll_ImageName, L"mpc-hc64.exe") == 0) {

            deftext = L"cocreate";
        }

        HookText = Dll_GetSettingsForImageName(L"SkipHook", deftext);

    //
    // query for a specific hook
    //

    } else if (HookText) {

        ULONG len = wcslen(HookName);
        WCHAR *ptr = HookText;
        while (ptr) {
            while (*ptr == L',')
                ++ptr;
            if (_wcsnicmp(ptr, HookName, len) == 0) {
                found = TRUE;
                break;
            }
            ptr = wcschr(ptr, L',');
        }
    }

    return found;
}


//---------------------------------------------------------------------------
// Dll_JumpStub
//---------------------------------------------------------------------------


_FX void *Dll_JumpStub(void *OldCode, void *NewCode, ULONG_PTR Arg)
{
    UCHAR *code, *ptr;

    code = Dll_AllocCode128();
    ptr = code;

    //
    // build stub which loads eax with (code + 32) then jumps to NewCode
    //

#ifdef _WIN64
    *(USHORT *)ptr = 0xB848;    // mov rax
    ptr += sizeof(USHORT);
#else ! _WIN64
    *ptr = 0xB8;                // mov eax
    ++ptr;
#endif _WIN64
    *(ULONG_PTR *)ptr = (ULONG_PTR)(code + 32);
    ptr += sizeof(ULONG_PTR);

    *(USHORT *)ptr = 0x25FF;    // jmp dword/qword ptr [rip+6]
    ptr += 2;
#ifdef _WIN64
    *(ULONG *)ptr = 0;
#else ! _WIN64
    *(ULONG *)ptr = (ULONG)(ptr + 4);
#endif _WIN64
    ptr += sizeof(ULONG);
    *(ULONG_PTR *)ptr = (ULONG_PTR)NewCode;

    //
    // write data at (code + 32)
    //

    ptr = code + 32;
    *(ULONG_PTR *)ptr = (ULONG_PTR)code;
    ptr += sizeof(ULONG_PTR);
    *(ULONG_PTR *)ptr = (ULONG_PTR)OldCode;
    ptr += sizeof(ULONG_PTR);
    *(ULONG_PTR *)ptr = (ULONG_PTR)Arg;

    //
    // write eyecatcher at (code + 64)
    //

    *(ULONG *)(code + 64) = tzuk;

    return code;
}


//---------------------------------------------------------------------------
// Dll_JumpStubData
//---------------------------------------------------------------------------


#pragma warning(push)
#pragma warning(disable : 4716) // function must return a value
_FX ULONG_PTR *Dll_JumpStubData(void)
{
    //
    // returns pointer to StubData which is stored in eax at the time
    // when the replacement code is entered.  use as first statement
    // to make sure the value in eax is not modified
    //
    // StubData[0] = detour stub.  use with Dll_FreeCode128:
    //               Dll_FreeCode128((void *)StubData[0])
    //
    // StubData[1] = OldCode value passed to Dll_JumpStub()
    //
    // StubData[2] = Arg value passed to Dll_JumpStub()
    //
    //
}
#pragma warning(pop)


//---------------------------------------------------------------------------
// Dll_JumpStubDataForCode
//---------------------------------------------------------------------------


_FX ULONG_PTR *Dll_JumpStubDataForCode(void *StubCode)
{
    //
    // if StubCode identifies a function created by Dll_JumpStub then
    // return the StubData for that stub.  otherwise return NULL
    //

    ULONG_PTR rv = 0;
    __try {
        if (*(ULONG *)((UCHAR *)StubCode + 64) == tzuk)
            rv = (ULONG_PTR)StubCode + 32;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return (ULONG_PTR *)rv;
}

#define WOW_SIZE 0x53
#define WOW_PATCH_SIZE 7

//---------------------------------------------------------------------------
// Dll_FixWow64Syscall
//---------------------------------------------------------------------------

extern ULONG Dll_Windows;
#ifndef _WIN64
#define GET_ADDR_OF_PEB __readfsdword(0x30)
#define GET_PEB_IMAGE_BUILD (*(USHORT *)(GET_ADDR_OF_PEB + 0xac))

_FX void Dll_FixWow64Syscall(void)
{
    static UCHAR *_code = NULL;

    //
    // the Wow64 thunking layer for syscalls in ntdll32 has several thunks:
    // thunk 0 calls the corresponding NtXxx export in the 64-bit ntdll.
    // other thunks issue the syscall instruction directly and are probably
    // intended as an optimization.  we want all 32-bit syscalls to go
    // through our SbieLow syscall interface, so we need to always force
    // use of thunk 0 rather than the optimization thunks.
    //
    // fs:[0xC0] stores the address of wow64cpu!X86SwitchTo64BitMode which
    // switches into 64-bit mode, we want to replace this with a small stub
    // which zeroes ecx (the thunk selector) and calls to the original code.
    //
    // note that fs:[0xC0] is thread specific so we have to fix this address
    // in every thread, so we are called from DllMain on DLL_THREAD_ATTACH
    //
    // note that on Windows 8, the thunk is selected by the high 16-bits of
    // eax, rather than by ecx, so we also adjust the value of eax
    //
    // Windows 10:
    // The wow64cpu!X86SwitchTo64BitMode has been removed.  Instead the switch
    // to 64 bit is done in 32 bit dll's in a function named Wow64SystemServiceCall
    // (ntdll.dll, user32.dll, gdi.dll,... etc).
    // The function uses a special jmp, e.g., (jmp far ptr 33h:$06).  The segment jmp using 
    // segment 0x33 will switch the cpu to 64bit (long mode).

    //; ---------------------------------------------------------------------------
    //_Wow64SystemServiceCall@0 proc near   ; CODE XREF: NtAccessCheck(x,x,x,x,x,x,x,x)
    //            mov     edx, large fs:30h
    //            mov     edx, [edx+254h]
    //            test    edx, 2 //if wow64
    //            jz      short IS_WOW64 
    //            int     2Eh               ; 32 bit syscall DOS 2+ internal - EXECUTE COMMAND
    //                                      ; DS:SI -> counted CR-terminated command string
    //            retn
    //; ---------------------------------------------------------------------------
    //            IS_WOW64:                 ; CODE XREF: Wow64SystemServiceCall()+13
    //            jmp     far ptr 33h:CPU_64 //cpu is switched to 64 bit mode @CPU_64
    //            CPU_64:
    //            jmp     dword ptr [r15+0F8h] //jmp to wow64cpu dispatch function
    //_Wow64SystemServiceCall@0 endp
    //; ---------------------------------------------------------------------------
    //In order to dispatch Wow64 in windows 10 correctly it is necessary to patch
    //wow64cpu.dll dispatch function directly.  This is done using fs:[0xc0] as a reference
    //,since it still points to a function in wow64cpu, to find this dispatch function.  
    //The following function finds and hooks this function for windows 10.


    if (Dll_IsWow64) {

        Dll_OsBuild = GET_PEB_IMAGE_BUILD;

        if (Dll_Windows >= 10) {
            if (!_code) {
                UCHAR *myAddress;
                ULONG_PTR wow64cpu = 0;
                void *RegionBase;
                SIZE_T RegionSize;
                ULONG OldProtect;
                ULONG dummy_prot;
                int i;

                //logic to find win64cpu dispatcher function by referencing the location
                //in fs:[0xc0] and looking for the function start signature (0x49 0x87 0xe6 : xch rsp,14)

                //end of previous function
                //48 CF     iretq
                //start of wow64 dispatch function
                //49 87 E6    xchg    rsp, r14
                //45 8B 06    mov     r8d, [r14]

                wow64cpu = (__readfsdword(0xC0) & 0xfffff0000);
                wow64cpu = DLL_FindWow64Target(wow64cpu);
                if (!wow64cpu) {
                    return;
                }
                wow64cpu -= 0x50;
                _try{
                    for (i = 0; i < 0x50 && *(ULONG *)wow64cpu != 0x45e68749; i++, wow64cpu++);
                // Look for end of previous function 
                if (*(USHORT *)(wow64cpu - 2) != 0xcf48) {
                    wow64cpu = 0;
                }
                }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                    wow64cpu = 0;
                }

                if (!wow64cpu) {
                    wow64cpu = 0;
                    return;
                }
                myAddress = (UCHAR *)VirtualAlloc(NULL, 0x100, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                if (!myAddress) {
                    return;
                }

                //wow64cpu hook for windows 10
                memcpy(myAddress + WOW_PATCH_SIZE, (void *)wow64cpu, WOW_SIZE);
                *(ULONG*)(myAddress + 0) = 0xFF25c933;
                myAddress[4] = 0xff;
                myAddress[5] = 0;
                myAddress[6] = 0;
                RegionBase = (void *)wow64cpu;
                RegionSize = 0x10;
                _code = (UCHAR *)wow64cpu;

                NtProtectVirtualMemory(NtCurrentProcess(), &RegionBase, &RegionSize, PAGE_EXECUTE_READWRITE, &OldProtect);
                _code[0] = 0x51;        //push rcx
                // mov rcx,<8 byte address to myAddress>
                _code[1] = 0x48;
                _code[2] = 0xb9;
                *(ULONGLONG *)&_code[3] = (ULONGLONG)myAddress;
                *(ULONG *)&_code[11] = 0x240C8948;  // mov [rsp],rcx
                _code[15] = 0xc3;       // ret
                NtProtectVirtualMemory(NtCurrentProcess(), &RegionBase, &RegionSize, OldProtect, &dummy_prot);
                return;
            }
        }
        else {
            if (!_code) {
                ULONG X86SwitchTo64BitMode = __readfsdword(0xC0);
                _code = (UCHAR *)Dll_AllocCode128();
                // and eax, 0xFFFF ; xor ecx, ecx ; jmp xxx ( push xxx; ret; )
                *(ULONG *)(_code + 0) = 0xFF25C933;
                *(ULONG *)(_code + 4) = 0x680000FF;
                *(ULONG *)(_code + 8) = X86SwitchTo64BitMode;
                *(UCHAR *)(_code + 12) = 0xc3;
            }
            __writefsdword(0xC0, (ULONG)_code);
            return;
        }
    }
    return;
}


_FX ULONG_PTR  DLL_FindWow64Target(ULONG_PTR address)
{
    IMAGE_DOS_HEADER *dos_hdr = 0;
    IMAGE_NT_HEADERS *nt_hdrs = 0;
    IMAGE_SECTION_HEADER *section = 0;
    IMAGE_DATA_DIRECTORY *data_dirs = 0;
    ULONG_PTR ExportDirectoryVA;
    IMAGE_EXPORT_DIRECTORY *ExportDirectory = NULL;
    ULONG_PTR* names;
    ULONG_PTR* functions;
    ULONG_PTR imageBase = 0;
    DWORD numNames;

    dos_hdr = (IMAGE_DOS_HEADER *)address;

    if (dos_hdr->e_magic == 'MZ' || dos_hdr->e_magic == 'ZM')
    {
        nt_hdrs = (IMAGE_NT_HEADERS *)((UCHAR *)dos_hdr + dos_hdr->e_lfanew);

        if (nt_hdrs->Signature == IMAGE_NT_SIGNATURE)       // 'PE\0\0'
        {
            if (nt_hdrs->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                IMAGE_NT_HEADERS64 *nt_hdrs_64 = (IMAGE_NT_HEADERS64 *)nt_hdrs;
                IMAGE_OPTIONAL_HEADER64 *opt_hdr_64 = &nt_hdrs_64->OptionalHeader;
                data_dirs = &opt_hdr_64->DataDirectory[0];
                imageBase = (ULONG_PTR)opt_hdr_64->ImageBase;
            }
        }
    }

    if (!data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].Size)
        return 0;

    ExportDirectoryVA = data_dirs[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    if (!ExportDirectoryVA)
        return 0;

    ExportDirectory = (IMAGE_EXPORT_DIRECTORY *)(ExportDirectoryVA + address);
    names = (ULONG_PTR *)(ExportDirectory->AddressOfNames + address);
    functions = (ULONG_PTR *)(ExportDirectory->AddressOfFunctions + address);

    for (numNames = ExportDirectory->NumberOfNames; numNames; numNames--)
    {
        if (!_stricmp((const char *)(names[numNames - 1] + address), "TurboDispatchJumpAddressStart"))
            return functions[numNames - 1] + address;
    }

    return 0;
}

#endif ! _WIN64
