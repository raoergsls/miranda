/*

Jabber Protocol Plugin for Miranda IM
Copyright ( C ) 2002-04  Santithorn Bunchua
Copyright ( C ) 2005-07  George Hazan
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

#ifndef _JABBER_CAPS_H_
#define _JABBER_CAPS_H_

#include "jabber_iq.h"

typedef unsigned __int64 JabberCapsBits;

#define JABBER_RESOURCE_CAPS_QUERY_TIMEOUT	10000

#define JABBER_RESOURCE_CAPS_ERROR        0x8000000000000000
#define JABBER_RESOURCE_CAPS_IN_PROGRESS  0x8000000000000001
#define JABBER_RESOURCE_CAPS_TIMEOUT      0x8000000000000002
#define JABBER_RESOURCE_CAPS_NONE         0x0000000000000000

#define JABBER_FEAT_DISCO_INFO                  "http://jabber.org/protocol/disco#info"
#define JABBER_CAPS_DISCO_INFO                  ((JabberCapsBits)1)
#define JABBER_FEAT_DISCO_ITEMS                 "http://jabber.org/protocol/disco#items"
#define JABBER_CAPS_DISCO_ITEMS                 ((JabberCapsBits)1<<1)
#define JABBER_FEAT_ENTITY_CAPS                 "http://jabber.org/protocol/caps"
#define JABBER_CAPS_ENTITY_CAPS                 ((JabberCapsBits)1<<2)
#define JABBER_FEAT_SI                          "http://jabber.org/protocol/si"
#define JABBER_CAPS_SI                          ((JabberCapsBits)1<<3)
#define JABBER_FEAT_SI_FT                       "http://jabber.org/protocol/si/profile/file-transfer"
#define JABBER_CAPS_SI_FT                       ((JabberCapsBits)1<<4)
#define JABBER_FEAT_BYTESTREAMS                 "http://jabber.org/protocol/bytestreams"
#define JABBER_CAPS_BYTESTREAMS                 ((JabberCapsBits)1<<5)
#define JABBER_FEAT_IBB                         "http://jabber.org/protocol/ibb"
#define JABBER_CAPS_IBB                         ((JabberCapsBits)1<<6)
#define JABBER_FEAT_OOB                         "jabber:iq:oob"
#define JABBER_CAPS_OOB                         ((JabberCapsBits)1<<7)
#define JABBER_FEAT_COMMANDS                    "http://jabber.org/protocol/commands"
#define JABBER_CAPS_COMMANDS                    ((JabberCapsBits)1<<8)
#define JABBER_FEAT_REGISTER                    "jabber:iq:register"
#define JABBER_CAPS_REGISTER                    ((JabberCapsBits)1<<9)
#define JABBER_FEAT_MUC                         "http://jabber.org/protocol/muc"
#define JABBER_CAPS_MUC                         ((JabberCapsBits)1<<10)
#define JABBER_FEAT_CHATSTATES                  "http://jabber.org/protocol/chatstates"
#define JABBER_CAPS_CHATSTATES                  ((JabberCapsBits)1<<11)
#define JABBER_FEAT_LAST_ACTIVITY               "jabber:iq:last"
#define JABBER_CAPS_LAST_ACTIVITY               ((JabberCapsBits)1<<12)
#define JABBER_FEAT_VERSION                     "jabber:iq:version"
#define JABBER_CAPS_VERSION                     ((JabberCapsBits)1<<13)
#define JABBER_FEAT_ENTITY_TIME                 "urn:xmpp:time"
#define JABBER_CAPS_ENTITY_TIME                 ((JabberCapsBits)1<<14)
#define JABBER_FEAT_PING                        "urn:xmpp:ping"
#define JABBER_CAPS_PING                        ((JabberCapsBits)1<<15)
#define JABBER_FEAT_DATA_FORMS                  "jabber:x:data"
#define JABBER_CAPS_DATA_FORMS                  ((JabberCapsBits)1<<16)
#define JABBER_FEAT_MESSAGE_EVENTS              "jabber:x:event"
#define JABBER_CAPS_MESSAGE_EVENTS              ((JabberCapsBits)1<<17)
#define JABBER_FEAT_VCARD_TEMP                  "vcard-temp"
#define JABBER_CAPS_VCARD_TEMP                  ((JabberCapsBits)1<<18)
#define JABBER_FEAT_AVATAR                      "jabber:iq:avatar"
#define JABBER_CAPS_AVATAR                      ((JabberCapsBits)1<<19)
#define JABBER_FEAT_XHTML                       "http://jabber.org/protocol/xhtml-im"
#define JABBER_CAPS_XHTML                       ((JabberCapsBits)1<<20)
#define JABBER_FEAT_AGENTS                      "jabber:iq:agents"
#define JABBER_CAPS_AGENTS                      ((JabberCapsBits)1<<21)
#define JABBER_FEAT_BROWSE                      "jabber:iq:browse"
#define JABBER_CAPS_BROWSE                      ((JabberCapsBits)1<<22)
#define JABBER_FEAT_FEATURE_NEG                 "http://jabber.org/protocol/feature-neg"
#define JABBER_CAPS_FEATURE_NEG                 ((JabberCapsBits)1<<23)
#define JABBER_FEAT_AMP                         "http://jabber.org/protocol/amp"
#define JABBER_CAPS_AMP                         ((JabberCapsBits)1<<24)
#define JABBER_FEAT_USER_MOOD                   "http://jabber.org/protocol/mood"
#define JABBER_CAPS_USER_MOOD                   ((JabberCapsBits)1<<25)
#define JABBER_FEAT_USER_MOOD_NOTIFY            "http://jabber.org/protocol/mood+notify"
#define JABBER_CAPS_USER_MOOD_NOTIFY            ((JabberCapsBits)1<<26)
#define JABBER_FEAT_PUBSUB                      "http://jabber.org/protocol/pubsub"
#define JABBER_CAPS_PUBSUB                      ((JabberCapsBits)1<<27)
#define JABBER_FEAT_SECUREIM                    "http://miranda-im.org/caps/secureim"
#define JABBER_CAPS_SECUREIM                    ((JabberCapsBits)1<<28)
#define JABBER_FEAT_PRIVACY_LISTS               "jabber:iq:privacy"
#define JABBER_CAPS_PRIVACY_LISTS               ((JabberCapsBits)1<<29)
#define JABBER_FEAT_MESSAGE_RECEIPTS            "urn:xmpp:receipts"
#define JABBER_CAPS_MESSAGE_RECEIPTS            ((JabberCapsBits)1<<30)
#define JABBER_FEAT_USER_TUNE                   "http://jabber.org/protocol/tune"
#define JABBER_CAPS_USER_TUNE                   ((JabberCapsBits)1<<31)
#define JABBER_FEAT_USER_TUNE_NOTIFY            "http://jabber.org/protocol/tune+notify"
#define JABBER_CAPS_USER_TUNE_NOTIFY            ((JabberCapsBits)1<<32)
#define JABBER_FEAT_PRIVATE_STORAGE             "jabber:iq:private"
#define JABBER_CAPS_PRIVATE_STORAGE             ((JabberCapsBits)1<<33)
#define JABBER_FEAT_ATTENTION                   "http://www.xmpp.org/extensions/xep-0224.html#ns"
#define JABBER_CAPS_ATTENTION                   ((JabberCapsBits)1<<34)

#define JABBER_FEAT_PUBSUB_EVENT                "http://jabber.org/protocol/pubsub#event"
#define JABBER_FEAT_PUBSUB_NODE_CONFIG          "http://jabber.org/protocol/pubsub#node_config"

#define JABBER_CAPS_MESSAGE_EVENTS_NO_DELIVERY  ((JabberCapsBits)1<<60)

#define JABBER_CAPS_MIRANDA_NODE                "http://miranda-im.org/caps"
#define JABBER_CAPS_MIRANDA_ALL                 (JABBER_CAPS_DISCO_INFO|JABBER_CAPS_DISCO_ITEMS|JABBER_CAPS_ENTITY_CAPS|JABBER_CAPS_MUC|JABBER_CAPS_ENTITY_CAPS|JABBER_CAPS_SI|JABBER_CAPS_SI_FT|JABBER_CAPS_BYTESTREAMS|JABBER_CAPS_IBB|JABBER_CAPS_OOB|JABBER_CAPS_CHATSTATES|JABBER_CAPS_AGENTS|JABBER_CAPS_BROWSE|JABBER_CAPS_VERSION|JABBER_CAPS_LAST_ACTIVITY|JABBER_CAPS_DATA_FORMS|JABBER_CAPS_MESSAGE_EVENTS|JABBER_CAPS_VCARD_TEMP|JABBER_CAPS_ENTITY_TIME|JABBER_CAPS_PING|JABBER_CAPS_PRIVACY_LISTS|JABBER_CAPS_MESSAGE_RECEIPTS|JABBER_CAPS_SECUREIM|JABBER_CAPS_COMMANDS|JABBER_CAPS_USER_MOOD_NOTIFY|JABBER_CAPS_USER_TUNE_NOTIFY|JABBER_CAPS_ATTENTION|JABBER_CAPS_PRIVATE_STORAGE)

#define JABBER_CAPS_MIRANDA_PARTIAL             (JABBER_CAPS_DISCO_INFO|JABBER_CAPS_DISCO_ITEMS|JABBER_CAPS_ENTITY_CAPS|JABBER_CAPS_MUC|JABBER_CAPS_ENTITY_CAPS|JABBER_CAPS_SI|JABBER_CAPS_SI_FT|JABBER_CAPS_BYTESTREAMS|JABBER_CAPS_IBB|JABBER_CAPS_OOB|JABBER_CAPS_CHATSTATES|JABBER_CAPS_AGENTS|JABBER_CAPS_BROWSE|JABBER_CAPS_VERSION|JABBER_CAPS_LAST_ACTIVITY|JABBER_CAPS_DATA_FORMS|JABBER_CAPS_MESSAGE_EVENTS|JABBER_CAPS_VCARD_TEMP|JABBER_CAPS_ENTITY_TIME|JABBER_CAPS_PING|JABBER_CAPS_PRIVACY_LISTS|JABBER_CAPS_MESSAGE_RECEIPTS|JABBER_CAPS_ATTENTION|JABBER_CAPS_PRIVATE_STORAGE)

#define JABBER_EXT_SECUREIM                     "secureim"
#define JABBER_EXT_COMMANDS                     "cmds"
#define JABBER_EXT_USER_MOOD                    "mood"
#define JABBER_EXT_USER_TUNE                    "tune"

#define JABBER_FEAT_EXT_ADDRESSING              "http://jabber.org/protocol/address"
#define JABBER_FEAT_NESTED_ROSTER_GROUPS        "roster:delimiter"

#define JABBER_FEAT_RC                          "http://jabber.org/protocol/rc"
#define JABBER_FEAT_RC_SET_STATUS               "http://jabber.org/protocol/rc#set-status"
#define JABBER_FEAT_RC_SET_OPTIONS              "http://jabber.org/protocol/rc#set-options"
#define JABBER_FEAT_RC_FORWARD                  "http://jabber.org/protocol/rc#forward"
#define JABBER_FEAT_RC_LEAVE_GROUPCHATS			"http://jabber.org/protocol/rc#leave-groupchats"
#define JABBER_FEAT_RC_WS_LOCK                  "http://miranda-im.org/rc#lock_workstation"
#define JABBER_FEAT_RC_QUIT_MIRANDA             "http://miranda-im.org/rc#quit"

#define JABBER_FEAT_IQ_ROSTER                   "jabber:iq:roster"
#define JABBER_FEAT_DELAY                       "jabber:x:delay"

#define JABBER_FEAT_MUC_USER                    "http://jabber.org/protocol/muc#user"


class CJabberClientPartialCaps
{

protected:
	TCHAR *m_szVer;
	JabberCapsBits m_jcbCaps;
	CJabberClientPartialCaps *m_pNext;
	int m_nIqId;
	DWORD m_dwRequestTime;

public:
	CJabberClientPartialCaps( TCHAR *szVer );
	~CJabberClientPartialCaps();

	CJabberClientPartialCaps* SetNext( CJabberClientPartialCaps *pCaps );
	__inline CJabberClientPartialCaps* GetNext()
	{	return m_pNext;
	}

	void SetCaps( JabberCapsBits jcbCaps, int nIqId = -1 );
	JabberCapsBits GetCaps();

	__inline TCHAR* GetVersion()
	{	return m_szVer;
	}

	__inline int GetIqId()
	{	return m_nIqId;
	}
};

class CJabberClientCaps
{

protected:
	TCHAR *m_szNode;
	CJabberClientPartialCaps *m_pCaps;
	CJabberClientCaps *m_pNext;

protected:
	CJabberClientPartialCaps* FindByVersion( TCHAR *szVer );
	CJabberClientPartialCaps* FindById( int nIqId );

public:
	CJabberClientCaps( TCHAR *szNode );
	~CJabberClientCaps();

	CJabberClientCaps* SetNext( CJabberClientCaps *pClient );
	__inline CJabberClientCaps* GetNext()
	{	return m_pNext;
	}

	JabberCapsBits GetPartialCaps( TCHAR *szVer );
	BOOL SetPartialCaps( TCHAR *szVer, JabberCapsBits jcbCaps, int nIqId = -1 );
	BOOL SetPartialCaps( int nIqId, JabberCapsBits jcbCaps );

	__inline TCHAR* GetNode()
	{	return m_szNode;
	}
};

class CJabberClientCapsManager
{

protected:
	CRITICAL_SECTION m_cs;
	CJabberClientCaps *m_pClients;

protected:
	CJabberClientCaps *FindClient( TCHAR *szNode );

public:
	CJabberClientCapsManager();
	~CJabberClientCapsManager();

	__inline void Lock()
	{	EnterCriticalSection( &m_cs );
	}
	__inline void Unlock()
	{	LeaveCriticalSection( &m_cs );
	}

	void AddDefaultCaps();

	JabberCapsBits GetClientCaps( TCHAR *szNode, TCHAR *szVer );
	BOOL SetClientCaps( TCHAR *szNode, TCHAR *szVer, JabberCapsBits jcbCaps, int nIqId = -1 );
	BOOL SetClientCaps( int nIqId, JabberCapsBits jcbCaps );

	BOOL HandleInfoRequest( XmlNode* iqNode, void* userdata, CJabberIqInfo* pInfo, TCHAR* szNode );

	BOOL TranslateCaps();
};

struct JabberFeatCapPair
{
	TCHAR *szFeature;
	JabberCapsBits jcbCap;
	TCHAR *szDescription;
};

extern JabberFeatCapPair g_JabberFeatCapPairs[];
extern JabberFeatCapPair g_JabberFeatCapPairsExt[];

extern CJabberClientCapsManager g_JabberClientCapsManager;

JabberCapsBits JabberGetResourceCapabilites( TCHAR *jid, BOOL appendBestResource = TRUE );
JabberCapsBits JabberGetTotalJidCapabilites( TCHAR *jid );

#endif
