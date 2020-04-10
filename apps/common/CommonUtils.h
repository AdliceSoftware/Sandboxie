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
// Common Utility Functions
//---------------------------------------------------------------------------


#ifndef _MY_COMMONUTILS_H
#define _MY_COMMONUTILS_H


//---------------------------------------------------------------------------
// Functions (c++)
//---------------------------------------------------------------------------


#ifdef __cplusplus


#include <afxcmn.h>


void Common_RunStartExe(const CString &cmd, const CString &box,
                        BOOL wait = FALSE, BOOL inherit = FALSE);


#endif __cplusplus


//---------------------------------------------------------------------------
// Functions (c)
//---------------------------------------------------------------------------


#ifdef __cplusplus
extern "C" {
#endif


void *Common_DlgTmplRtl(HINSTANCE hInst, const WCHAR *TmplName);

int Common_SHFileOperation(void *lpSHFileOpStruct, BOOL *pYesToAll,
                           const WCHAR *ReplaceButtonText);


#ifdef __cplusplus
}
#endif


//---------------------------------------------------------------------------

#endif _MY_COMMONUTILS_H
