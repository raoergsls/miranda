////////////////////////////////////////////////////////////////////////////////
// Gadu-Gadu Plugin for Miranda IM
//
// Copyright (c) 2003 Adam Strzelecki <ono+miranda@java.pl>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
////////////////////////////////////////////////////////////////////////////////

#include "gg.h"
#include <errno.h>

// Plugin info
PLUGININFO pluginInfo = {
    sizeof(PLUGININFO),
    "Gadu-Gadu Protocol",
    PLUGIN_MAKE_VERSION(0, 0, 2, 4),
    "Provides support for Gadu-Gadu Messenger",
    "Adam Strzelecki",
    "ono+miranda@java.pl",
    "Copyright � 2003 Adam Strzelecki",
    "http://www.miranda.kom.pl/",
    0,
    0
};

// Other variables
PLUGINLINK *pluginLink;
HINSTANCE hInstance;
int ggStatus = ID_STATUS_OFFLINE;           // gadu-gadu status
int ggDesiredStatus = ID_STATUS_OFFLINE;    // gadu-gadu desired status
HANDLE hNetlib;                             // used just for logz
HANDLE hLibSSL;								// SSL main library handle
HANDLE hLibEAY;								// SSL/EAY misc library handle
char *ggProto = NULL;						// proto id get from DLL name   (def GG from GG.dll or GGdebug.dll)
char *ggProtoName = NULL;					// proto name get from DLL name (def Gadu-Gadu from GG.dll or GGdebug.dll)
char *ggProtoError = NULL;					// proto error get from DLL name (def Gadu-Gadu from GG.dll or GGdebug.dll)

// status messages
struct gg_status_msgs ggModeMsg;

// Event Hooks
static HANDLE hHookOptsInit;
static HANDLE hHookUserInfoInit;
static HANDLE hHookModulesLoaded;
static HANDLE hHookSettingDeleted;
static HANDLE hHookSettingChanged;

static unsigned long crc_table[256];

//////////////////////////////////////////////////////////
// extra winsock function for error description
char *ws_strerror(int code)
{
    static char err_desc[160];

    // Not a windows error display WinSock
    if(code == 0)
    {
        char buff[128];
        int len;
        len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, WSAGetLastError(), 0, buff,
                  sizeof(buff), NULL);
        if(len == 0)
            sprintf(err_desc, "WinSock %u: Unknown error.", WSAGetLastError());
        else
            sprintf(err_desc, "WinSock %d: %s", WSAGetLastError(), buff);
        return err_desc;
    }

    // return normal error
    return strerror(code);
}

//////////////////////////////////////////////////////////
// build the crc table
void crc_gentable(void)
{
    unsigned long crc, poly;
    int	i, j;

    poly = 0xEDB88320L;
    for (i = 0; i < 256; i++)
    {
        crc = i;
        for (j = 8; j > 0; j--)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
        crc_table[i] = crc;
    }
}

//////////////////////////////////////////////////////////
// calculate the crc value
unsigned long crc_get(char *mem)
{
    register unsigned long crc = 0xFFFFFFFF;
    while(mem && *mem)
        crc = ((crc>>8) & 0x00FFFFFF) ^ crc_table[(crc ^ *(mem++)) & 0xFF];

    return (crc ^ 0xFFFFFFFF);
}

void gg_refreshblockedicon()
{
    // store blocked icon
    char strFmt1[MAX_PATH], strFmt2[MAX_PATH];
    GetModuleFileName(hInstance, strFmt1, sizeof(strFmt1));
    sprintf(strFmt2, "%s,-%d", strFmt1, IDI_STOP);
    sprintf(strFmt1, "%s%d", GG_PROTO, ID_STATUS_DND);
    DBWriteContactSettingString(NULL, "Icons", strFmt1, strFmt2);
}

//////////////////////////////////////////////////////////
// http_error_string()
//
// returns http error text
const char *http_error_string(int h)
{
	switch (h)
	{
		case 0:
			return Translate((errno == ENOMEM) ? "HTTP failed memory" : "HTTP failed connecting");
		case GG_ERROR_RESOLVING:
			return Translate("HTTP failed resolving");
		case GG_ERROR_CONNECTING:
			return Translate("HTTP failed connecting");
		case GG_ERROR_READING:
			return Translate("HTTP failed ceading");
		case GG_ERROR_WRITING:
			return Translate("HTTP failed writing");
	}

	return "Unknown HTTP error";
}

//////////////////////////////////////////////////////////
// gets plugin info
__declspec(dllexport) PLUGININFO *MirandaPluginInfo(DWORD mirandaVersion)
{
    return &pluginInfo;
}

//////////////////////////////////////////////////////////
// cleanups from last plugin
void gg_cleanuplastplugin(DWORD version)
{
    HANDLE hContact;
	char *szProto;

	// Remove bad e-mail and phones from
	if(version < PLUGIN_MAKE_VERSION(0, 0, 1, 4))
	{
		// Look for contact in DB
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while (hContact) {
			szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
			if (szProto != NULL && !strcmp(szProto, GG_PROTO))
			{
				// Do contact cleanup
				DBDeleteContactSetting(hContact, GG_PROTO, GG_KEY_EMAIL);
				DBDeleteContactSetting(hContact, GG_PROTO, "Phone");
			}
			hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
		}
	}

	// Store this plugin version
	DBWriteContactSettingDword(NULL, GG_PROTO, GG_PLUGINVERSION, pluginInfo.version);
}

//////////////////////////////////////////////////////////
// when miranda loaded its modules
int gg_modulesloaded(WPARAM wParam, LPARAM lParam)
{
    NETLIBUSER nlu = { 0 };
	char title[64];
	strcpy(title, GG_PROTONAME);
	strcat(title, " ");
	strcat(title, Translate("connection"));

    nlu.cbSize = sizeof(nlu);
    nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS;
    nlu.szSettingsModule = GG_PROTO;
    nlu.szDescriptiveName = title;
    hNetlib = (HANDLE) CallService(MS_NETLIB_REGISTERUSER, 0, (LPARAM) & nlu);
    hHookOptsInit = HookEvent(ME_OPT_INITIALISE, gg_optionsinit);
    hHookUserInfoInit = HookEvent(ME_USERINFO_INITIALISE, gg_detailsinit);
    hHookSettingDeleted = HookEvent(ME_DB_CONTACT_DELETED, gg_userdeleted);
    hHookSettingChanged = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, gg_dbsettingchanged);

	// Init SSL library
	gg_ssl_init();

	// Init misc thingies
    //gg_inituserinfo();
    gg_initkeepalive();
    gg_initimport();
    gg_initchpass();
    gg_img_load();

	// Make error message
	char *error = Translate("Error");
	ggProtoError = malloc(strlen(ggProtoName) + strlen(error) + 2);
	strcpy(ggProtoError, ggProtoName);
	strcat(ggProtoError, " ");
	strcat(ggProtoError, error);

	// Do last plugin cleanup if not actual version
	DWORD version;
	if((version = DBGetContactSettingDword(NULL, GG_PROTO, GG_PLUGINVERSION, 0)) < pluginInfo.version)
		gg_cleanuplastplugin(version);

    return 0;
}

//////////////////////////////////////////////////////////
// Init multiple instances proto name
void init_protonames()
{
	char text[MAX_PATH], *p, *q;
	GetModuleFileName(hInstance, text, sizeof(text));

	if(p = strrchr(text, '\\')) p++;
	if(q = strrchr(p, '.'))	*q = '\0';
	if((q = strstr(p, "debug")) && strlen(q) == 5)
		*q = '\0';

	ggProto = strdup(p);
	strupr(ggProto);
	// Is it default GG.dll if yes do Gadu-Gadu as a title
	if(!strcmp(ggProto, GGDEF_PROTO))
		ggProtoName = strdup(GGDEF_PROTONAME);
	else
		ggProtoName = strdup(p);
}

//////////////////////////////////////////////////////////
// When plugin is loaded
int __declspec(dllexport) Load(PLUGINLINK * link)
{
    // Init winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD( 1, 1 ), &wsaData))
        return 1;

	// Init proto names
	init_protonames();

    PROTOCOLDESCRIPTOR pd;

    pluginLink = link;
    hHookModulesLoaded = HookEvent(ME_SYSTEM_MODULESLOADED, gg_modulesloaded);
    ZeroMemory(&pd, sizeof(pd));
    ZeroMemory(&ggModeMsg, sizeof(ggModeMsg));
    pd.cbSize = sizeof(pd);
    pd.szName = GG_PROTO;
    pd.type = PROTOTYPE_PROTOCOL;
    CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM) & pd);
    pthread_mutex_init(&modeMsgsMutex, NULL);
    pthread_mutex_init(&connectionHandleMutex, NULL);
    pthread_mutex_init(&dccWatchesMutex, NULL);
    gg_registerservices();
    gg_setalloffline();
    gg_refreshblockedicon();
    return 0;
}

//////////////////////////////////////////////////////////
// when plugin is unloaded
int __declspec(dllexport) Unload()
{
    // Log off
    if (gg_isonline()) gg_disconnect();
#ifdef DEBUGMODE
    gg_netlog("Unload(): destroying plugin");
#endif
    //gg_destroyuserinfo();
    gg_destroykeepalive();
    pthread_mutex_destroy(&modeMsgsMutex);
    pthread_mutex_destroy(&connectionHandleMutex);
    pthread_mutex_destroy(&dccWatchesMutex);
    LocalEventUnhook(hHookOptsInit);
    LocalEventUnhook(hHookModulesLoaded);
    LocalEventUnhook(hHookSettingDeleted);
    LocalEventUnhook(hHookSettingChanged);
#ifdef DEBUGMODE
    gg_netlog("Unload(): closing hNetlib");
#endif
    Netlib_CloseHandle(hNetlib);

    // Free status messages
    if(ggModeMsg.szOnline)      free(ggModeMsg.szOnline);
    if(ggModeMsg.szAway)        free(ggModeMsg.szAway);
    if(ggModeMsg.szInvisible)   free(ggModeMsg.szInvisible);
    if(ggModeMsg.szOffline)     free(ggModeMsg.szOffline);

	// Cleanup protonames
	if(ggProto) free(ggProto);
	if(ggProtoName) free(ggProtoName);
	if(ggProtoError) free(ggProtoError);

   // Close mutex handles
   gg_img_unload();
	// Uninit SSL library
	gg_ssl_uninit();
    // Cleanup WinSock
    WSACleanup();
    return 0;
}


//////////////////////////////////////////////////////////
// DEBUGING FUNCTIONS

#ifdef DEBUGMODE
struct
{
	int type;
	char *text;
} ggdebug_eventype2string[] =
{
	{GG_EVENT_NONE,					"GG_EVENT_NONE"},
	{GG_EVENT_MSG,					"GG_EVENT_MSG"},
	{GG_EVENT_NOTIFY,				"GG_EVENT_NOTIFY"},
	{GG_EVENT_NOTIFY_DESCR,			"GG_EVENT_NOTIFY_DESCR"},
	{GG_EVENT_STATUS,				"GG_EVENT_STATUS"},
	{GG_EVENT_ACK,					"GG_EVENT_ACK"},
	{GG_EVENT_PONG,					"GG_EVENT_PONG"},
	{GG_EVENT_CONN_FAILED,			"GG_EVENT_CONN_FAILED"},
	{GG_EVENT_CONN_SUCCESS,			"GG_EVENT_CONN_SUCCESS"},
	{GG_EVENT_DISCONNECT,			"GG_EVENT_DISCONNECT"},
	{GG_EVENT_DCC_NEW,				"GG_EVENT_DCC_NEW"},
	{GG_EVENT_DCC_ERROR,			"GG_EVENT_DCC_ERROR"},
	{GG_EVENT_DCC_DONE,				"GG_EVENT_DCC_DONE"},
	{GG_EVENT_DCC_CLIENT_ACCEPT,	"GG_EVENT_DCC_CLIENT_ACCEPT"},
	{GG_EVENT_DCC_CALLBACK,			"GG_EVENT_DCC_CALLBACK"},
	{GG_EVENT_DCC_NEED_FILE_INFO,	"GG_EVENT_DCC_NEED_FILE_INFO"},
	{GG_EVENT_DCC_NEED_FILE_ACK,	"GG_EVENT_DCC_NEED_FILE_ACK"},
	{GG_EVENT_DCC_NEED_VOICE_ACK,	"GG_EVENT_DCC_NEED_VOICE_ACK"},
	{GG_EVENT_DCC_VOICE_DATA, 		"GG_EVENT_DCC_VOICE_DATA"},
	{GG_EVENT_PUBDIR50_SEARCH_REPLY,"GG_EVENT_PUBDIR50_SEARCH_REPLY"},
	{GG_EVENT_PUBDIR50_READ,		"GG_EVENT_PUBDIR50_READ"},
	{GG_EVENT_PUBDIR50_WRITE,		"GG_EVENT_PUBDIR50_WRITE"},
	{GG_EVENT_STATUS60,		        "GG_EVENT_STATUS60"},
	{GG_EVENT_NOTIFY60,		        "GG_EVENT_NOTIFY60"},
	{GG_EVENT_USERLIST,		        "GG_EVENT_USERLIST"},
	{GG_EVENT_IMAGE_REQUEST,		"GG_EVENT_IMAGE_REQUEST"},
	{GG_EVENT_IMAGE_REPLY,		    "GG_EVENT_IMAGE_REPLY"},
	{GG_EVENT_DCC_ACK,		        "GG_EVENT_DCC_ACK"},
	{-1,							"<unknown event>"}
};

const char *ggdebug_eventtype(struct gg_event *e)
{
	int i;
	for(i = 0; ggdebug_eventype2string[i].type != -1; i++)
		if(ggdebug_eventype2string[i].type == e->type)
			return ggdebug_eventype2string[i].text;
	return ggdebug_eventype2string[i].text;
}

void gg_debughandler(int level, const char *format, va_list ap)
{
    char szText[1024], *szFormat = strdup(format);
	strcpy(szText, GG_PROTO);
	strcat(szText, "      >> libgadu << \0");
	// Kill end line
	char *nl = strrchr(szFormat, '\n');
	if(nl) *nl = 0;

    _vsnprintf(szText + strlen(szText), sizeof(szText) - strlen(szText), szFormat, ap);
    CallService(MS_NETLIB_LOG, (WPARAM) hNetlib, (LPARAM) szText);
	free(szFormat);
}

////////////////////////////////////////////////////////////
// Log funcion
int gg_netlog(const char *fmt, ...)
{
    va_list va;
    char szText[1024];
	strcpy(szText, GG_PROTO);
	strcat(szText, "::\0");

    va_start(va, fmt);
    _vsnprintf(szText + strlen(szText), sizeof(szText) - strlen(szText), fmt, va);
    va_end(va);
    return CallService(MS_NETLIB_LOG, (WPARAM) hNetlib, (LPARAM) szText);
}

#endif

////////////////////////////////////////////////////////////////////////////
// Image dialog call thread
void gg_img_dlgcall(void *empty);
void *__stdcall gg_img_dlgthread(void *empty)
{
    gg_img_dlgcall(empty);
    return NULL;
}

//////////////////////////////////////////////////////////
// main DLL function
BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    crc_gentable();
    hInstance = hInst;
#ifdef DEBUGMODE
    gg_debug_handler = gg_debughandler;
#endif
    return TRUE;
}

