/*
Miranda Database Tool
Copyright (C) 2001-3  Richard Hughes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include <windows.h>
#include <commctrl.h>
#include "../../SDK/headers_c/newpluginapi.h" // Only needed to keep m_database.h happy
#include "../../SDK/headers_c/m_database.h"
#include "database.h" // Note: This is a copy of database.h from the Miranda IM v0.3 tree.
                      //       Remember to update this when releasing new dbtool versions.
#include "resource.h"

#define WZM_GOTOPAGE   (WM_USER+1)
#define WZN_PAGECHANGING  (WM_USER+1221)
#define WZN_CANCELCLICKED (WM_USER+1222)

struct DbToolOptions {
	char filename[MAX_PATH];
	char workingFilename[MAX_PATH];
	char outputFilename[MAX_PATH];
	char backupFilename[MAX_PATH];
	HANDLE hFile;
	HANDLE hOutFile;
	DWORD error;
	int bCheckOnly,bBackup,bAggressive;
	int bEraseHistory,bMarkRead;
};

extern HINSTANCE hInst;
extern DbToolOptions opts;
extern DBHeader dbhdr;

int DoMyControlProcessing(HWND hdlg,UINT message,WPARAM wParam,LPARAM lParam,BOOL *bReturn);

#define STATUS_MESSAGE    0
#define STATUS_WARNING    1
#define STATUS_ERROR      2
#define STATUS_FATAL      3
#define STATUS_SUCCESS    4
#define STATUS_CLASSMASK  0x0f
int AddToStatus(DWORD flags,char *fmt,...);
void SetProgressBar(int perThou);

int PeekSegment(DWORD ofs,PVOID buf,int cbBytes);
int ReadSegment(DWORD ofs,PVOID buf,int cbBytes);
#define WSOFS_END   0xFFFFFFFF
#define WS_ERROR    0xFFFFFFFF
DWORD WriteSegment(DWORD ofs,PVOID buf,int cbBytes);
int SignatureValid(DWORD ofs,DWORD signature);
DWORD ConvertModuleNameOfs(DWORD ofsOld);