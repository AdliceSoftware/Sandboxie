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
// Sandboxie Ini Server -- using PipeServer
//---------------------------------------------------------------------------


#ifndef _MY_SBIEINISERVER_H
#define _MY_SBIEINISERVER_H


#include "PipeServer.h"


class SbieIniServer
{

public:

    SbieIniServer(PipeServer *pipeServer);

    ~SbieIniServer();

protected:

    static MSG_HEADER *Handler(void *_this, MSG_HEADER *msg);

    MSG_HEADER *Handler2(MSG_HEADER *msg);

    MSG_HEADER *GetVersion(MSG_HEADER *msg);

    MSG_HEADER *GetWaitHandle(HANDLE idProcess);

    MSG_HEADER *GetUser(MSG_HEADER *msg);

    MSG_HEADER *GetPath(MSG_HEADER *msg);

    ULONG CheckRequest(MSG_HEADER *msg);

    bool SetUserSettingsSectionName(HANDLE hToken);

    bool UserCanEdit(HANDLE hToken);

    ULONG SetSetting(MSG_HEADER *msg);

    ULONG AddSetting(MSG_HEADER *msg, bool insert);

    ULONG DelSetting(MSG_HEADER *msg);

    ULONG SetTemplate(MSG_HEADER *msg);

    ULONG SetOrTestPassword(MSG_HEADER *msg);

    ULONG CallSetSetting(WCHAR *text, MSG_HEADER *msg);

    bool AddText(const WCHAR *line);

    bool AddCallerText(WCHAR *setting, WCHAR *value);

    ULONG RefreshConf();

    bool GetIniPath(WCHAR **IniPath, WCHAR **TmpPath,
                    BOOLEAN *IsHomePath = NULL);

    bool TokenIsAdmin(HANDLE hToken);

    ULONG IsCallerAuthorized(HANDLE hToken, const WCHAR *Password);

    void LockConf(WCHAR *IniPath);

    void UnlockConf();

    bool HashPassword(const WCHAR *Password, WCHAR *Hash41);

    MSG_HEADER *RunSbieCtrl(HANDLE idProcess, bool isSandboxed);

protected:

    CRITICAL_SECTION m_critsec;
    WCHAR m_username[256];
    WCHAR m_sectionname[128];
    WCHAR *m_text, *m_text_base;
    ULONG m_text_max_len;
    WCHAR m_line[1500];
    BOOLEAN m_insertbom;
    BOOLEAN m_admin;
    HANDLE m_hLockFile;
    ULONG m_session_id;

};


#endif /* _MY_SBIEINISERVER_H */
