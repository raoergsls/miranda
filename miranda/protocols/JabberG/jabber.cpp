/*

Jabber Protocol Plugin for Miranda IM
Copyright ( C ) 2002-04  Santithorn Bunchua
Copyright ( C ) 2005-08  George Hazan
Copyright ( C ) 2007     Maxim Mluhov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or ( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

File name      : $URL$
Revision       : $Revision$
Last change on : $Date$
Last change by : $Author$

*/

#include "jabber.h"
#include "jabber_ssl.h"
#include "jabber_iq.h"
#include "jabber_caps.h"
#include "jabber_rc.h"
#include "resource.h"
#include "version.h"

#include "sdk/m_icolib.h"
#include "sdk/m_folders.h"
#include "sdk/m_wizard.h"

HINSTANCE hInst;
PLUGINLINK *pluginLink;

PLUGININFOEX pluginInfo = {
	sizeof( PLUGININFOEX ),
	"Jabber Protocol",
	__VERSION_DWORD,
	"Jabber protocol plugin for Miranda IM ( "__DATE__" )",
	"George Hazan, Maxim Mluhov, Victor Pavlychko, Artem Shpynov, Michael Stepura",
	"ghazan@miranda-im.org",
	"(c) 2005-07 George Hazan, Maxim Mluhov, Victor Pavlychko, Artem Shpynov, Michael Stepura",
	"http://miranda-im.org",
	UNICODE_AWARE,
	0,
    #if defined( _UNICODE )
    {0x1ee5af12, 0x26b0, 0x4290, { 0x8f, 0x97, 0x16, 0x77, 0xcb, 0xe, 0xfd, 0x2b }} //{1EE5AF12-26B0-4290-8F97-1677CB0EFD2B}
    #else
    {0xf7f5861d, 0x988d, 0x479d, { 0xa5, 0xbb, 0x80, 0xc7, 0xfa, 0x8a, 0xd0, 0xef }} //{F7F5861D-988D-479d-A5BB-80C7FA8AD0EF}
    #endif
};

MM_INTERFACE    mmi;
LIST_INTERFACE  li;
UTF8_INTERFACE  utfi;
MD5_INTERFACE   md5i;
SHA1_INTERFACE  sha1i;

/////////////////////////////////////////////////////////////////////////////
// Theme API
BOOL (WINAPI *JabberAlphaBlend)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION) = NULL;
BOOL (WINAPI *JabberIsThemeActive)() = NULL;
HRESULT (WINAPI *JabberDrawThemeParentBackground)(HWND, HDC, RECT *) = NULL;
/////////////////////////////////////////////////////////////////////////////

HANDLE hMainThread = NULL;
DWORD  jabberMainThreadId;
BOOL   jabberChatDllPresent = FALSE;

// SSL-related global variable
HMODULE hLibSSL = NULL;

const char xmlnsAdmin[] = "http://jabber.org/protocol/muc#admin";
const char xmlnsOwner[] = "http://jabber.org/protocol/muc#owner";

void JabberUserInfoInit(void);

int bSecureIM;

extern "C" BOOL WINAPI DllMain( HINSTANCE hModule, DWORD dwReason, LPVOID lpvReserved )
{
	#ifdef _DEBUG
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif
	hInst = hModule;
	return TRUE;
}

extern "C" __declspec( dllexport ) PLUGININFOEX *MirandaPluginInfoEx( DWORD mirandaVersion )
{
	if ( mirandaVersion < PLUGIN_MAKE_VERSION( 0,8,0,8 )) {
		MessageBoxA( NULL, "The Jabber protocol plugin cannot be loaded. It requires Miranda IM 0.8.0.8 or later.", "Jabber Protocol Plugin", MB_OK|MB_ICONWARNING|MB_SETFOREGROUND|MB_TOPMOST );
		return NULL;
	}

	return &pluginInfo;
}

static const MUUID interfaces[] = {MIID_PROTOCOL, MIID_LAST};
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

///////////////////////////////////////////////////////////////////////////////
// OnPreShutdown - prepares Miranda to be shut down

int __cdecl CJabberProto::OnPreShutdown( WPARAM wParam, LPARAM lParam )
{
	if ( m_hwndJabberAgents ) {
		::SendMessage( m_hwndJabberAgents, WM_CLOSE, 0, 0 );
		m_hwndJabberAgents = NULL;
	}
	if ( m_hwndJabberGroupchat ) {
		::SendMessage( m_hwndJabberGroupchat, WM_CLOSE, 0, 0 );
		m_hwndJabberGroupchat = NULL;
	}
	if ( m_hwndJabberJoinGroupchat ) {
		::SendMessage( m_hwndJabberJoinGroupchat, WM_CLOSE, 0, 0 );
		m_hwndJabberJoinGroupchat = NULL;
	}
	if ( m_hwndAgentReg ) {
		::SendMessage( m_hwndAgentReg, WM_CLOSE, 0, 0 );
		m_hwndAgentReg = NULL;
	}
	if ( m_hwndAgentRegInput ) {
		::SendMessage( m_hwndAgentRegInput, WM_CLOSE, 0, 0 );
		m_hwndAgentRegInput = NULL;
	}
	if ( m_hwndRegProgress ) {
		::SendMessage( m_hwndRegProgress, WM_CLOSE, 0, 0 );
		m_hwndRegProgress = NULL;
	}
	if ( m_hwndJabberVcard ) {
		::SendMessage( m_hwndJabberVcard, WM_CLOSE, 0, 0 );
		m_hwndJabberVcard = NULL;
	}
	if ( m_hwndMucVoiceList ) {
		::SendMessage( m_hwndMucVoiceList, WM_CLOSE, 0, 0 );
		m_hwndMucVoiceList = NULL;
	}
	if ( m_hwndMucMemberList ) {
		::SendMessage( m_hwndMucMemberList, WM_CLOSE, 0, 0 );
		m_hwndMucMemberList = NULL;
	}
	if ( m_hwndMucModeratorList ) {
		::SendMessage( m_hwndMucModeratorList, WM_CLOSE, 0, 0 );
		m_hwndMucModeratorList = NULL;
	}
	if ( m_hwndMucBanList ) {
		::SendMessage( m_hwndMucBanList, WM_CLOSE, 0, 0 );
		m_hwndMucBanList = NULL;
	}
	if ( m_hwndMucAdminList ) {
		::SendMessage( m_hwndMucAdminList, WM_CLOSE, 0, 0 );
		m_hwndMucAdminList = NULL;
	}
	if ( m_hwndMucOwnerList ) {
		::SendMessage( m_hwndMucOwnerList, WM_CLOSE, 0, 0 );
		m_hwndMucOwnerList = NULL;
	}
	if ( m_hwndJabberChangePassword ) {
		::SendMessage( m_hwndJabberChangePassword, WM_CLOSE, 0, 0 );
		m_hwndJabberChangePassword = NULL;
	}
	if ( m_hwndJabberBookmarks ) {
		::SendMessage( m_hwndJabberBookmarks, WM_CLOSE, 0, 0 );
		m_hwndJabberBookmarks = NULL;
	}
	if ( m_hwndJabberAddBookmark ) {
		::SendMessage( m_hwndJabberAddBookmark, WM_CLOSE, 0, 0 );
		m_hwndJabberAddBookmark = NULL;
	}
	if ( m_hwndPrivacyRule ) {
		::SendMessage( m_hwndPrivacyRule, WM_CLOSE, 0, 0 );
		m_hwndPrivacyRule = NULL;
	}
	if ( m_pDlgPrivacyLists ) {
		m_pDlgPrivacyLists->Close();
		m_pDlgPrivacyLists = NULL;
	}
	if ( m_hwndServiceDiscovery ) {
		::SendMessage( m_hwndServiceDiscovery, WM_CLOSE, 0, 0 );
		m_hwndServiceDiscovery = NULL;
	}
	m_hwndAgentManualReg = NULL;

	m_iqManager.ExpireAll();
	m_iqManager.Shutdown();
	ConsoleUninit();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// OnModulesLoaded - execute some code when all plugins are initialized

static int OnModulesLoaded( WPARAM wParam, LPARAM lParam )
{
	bSecureIM = (ServiceExists("SecureIM/IsContactSecured"));
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// OnLoad - initialize the plugin instance

static CJabberProto* jabberProtoInit( const char* pszProtoName, const TCHAR* tszUserName )
{
	CJabberProto* ppro = new CJabberProto( pszProtoName );
	if ( !ppro )
		return NULL;

	ppro->m_tszUserName = mir_tstrdup( tszUserName );

	ppro->JHookEvent( ME_SYSTEM_MODULESLOADED, &CJabberProto::OnModulesLoadedEx );

	char text[ MAX_PATH ];
	mir_snprintf( text, sizeof( text ), "%s/Status", ppro->m_szProtoName );
	JCallService( MS_DB_SETSETTINGRESIDENT, TRUE, ( LPARAM )text );
	return ppro;
}

static int jabberProtoUninit( CJabberProto* ppro )
{
	delete ppro;
	return 0;
}

extern "C" int __declspec( dllexport ) Load( PLUGINLINK *link )
{
	pluginLink = link;

	// set the memory, lists & utf8 managers
	mir_getMMI( &mmi );
	mir_getLI( &li );
	mir_getUTFI( &utfi );
	mir_getMD5I( &md5i );
	mir_getSHA1I( &sha1i );

	DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hMainThread, THREAD_SET_CONTEXT, FALSE, 0 );
	jabberMainThreadId = GetCurrentThreadId();

	// Register protocol module
	PROTOCOLDESCRIPTOR pd;
	ZeroMemory( &pd, sizeof( PROTOCOLDESCRIPTOR ));
	pd.cbSize = sizeof( PROTOCOLDESCRIPTOR );
	pd.szName = "JABBER";
	pd.fnInit = ( pfnInitProto )jabberProtoInit;
	pd.fnUninit = ( pfnUninitProto )jabberProtoUninit;
	pd.type = PROTOTYPE_PROTOCOL;
	CallService( MS_PROTO_REGISTERMODULE, 0, ( LPARAM )&pd );

	// Load some fuctions
	HINSTANCE hDll;
	if ( hDll = LoadLibraryA("msimg32.dll" ))
		JabberAlphaBlend = (BOOL (WINAPI *)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION)) GetProcAddress(hDll, "AlphaBlend");

	if ( IsWinVerXPPlus() ) {
		if ( hDll = GetModuleHandleA("uxtheme")) {
			JabberDrawThemeParentBackground = (HRESULT (WINAPI *)(HWND,HDC,RECT *))GetProcAddress(hDll, "DrawThemeParentBackground");
			JabberIsThemeActive = (BOOL (WINAPI *)())GetProcAddress(hDll, "IsThemeActive");
	}	}

	srand(( unsigned ) time( NULL ));
	JabberUserInfoInit();
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Unload - destroy the plugin instance

extern "C" int __declspec( dllexport ) Unload( void )
{
	if ( hMainThread )
		CloseHandle( hMainThread );

	if ( hLibSSL ) {
		FreeLibrary( hLibSSL );
		hLibSSL = NULL;
	}

	return 0;
}
