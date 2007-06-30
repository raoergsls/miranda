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

File name      : $Source: /cvsroot/miranda/miranda/protocols/JabberG/jabber_byte.cpp,v $
Revision       : $Revision$
Last change on : $Date$
Last change by : $Author$

*/

#include "jabber.h"
#include "jabber_iq.h"
#include "jabber_byte.h"
#include "jabber_caps.h"

#define JABBER_NETWORK_BUFFER_SIZE 4096

///////////////// Bytestream sending /////////////////////////

static void JabberByteInitiateResult( XmlNode *iqNode, void *userdata, CJabberIqRequestInfo *pInfo );
static void JabberByteSendConnection( HANDLE hNewConnection, DWORD dwRemoteIP );
static int JabberByteSendParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen );
static int JabberByteSendProxyParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen );
void JabberByteSendViaProxy( JABBER_BYTE_TRANSFER *jbt );

TCHAR* PrepareJid(TCHAR *jid)
{
	if ( !jid ) return NULL;
	TCHAR* szNewJid = mir_tstrdup(jid);
	if ( !szNewJid ) return NULL;
	TCHAR* pDelimiter = _tcschr( szNewJid, _T('/') );
	if ( pDelimiter ) *pDelimiter = _T('\0');
	CharLower( szNewJid );
	if ( pDelimiter ) *pDelimiter = _T('/');
	return szNewJid;
}

void JabberByteFreeJbt( JABBER_BYTE_TRANSFER *jbt )
{
	if ( jbt )  {
		mir_free( jbt->srcJID );
		mir_free( jbt->dstJID );
		mir_free( jbt->streamhostJID );
		mir_free( jbt->iqId );
		mir_free( jbt->sid );
		if ( jbt->iqNode ) delete jbt->iqNode;

		// XEP-0065 proxy support
		mir_free( jbt->szProxyHost );
		mir_free( jbt->szProxyPort );
		mir_free( jbt->szProxyJid );
		mir_free( jbt->szStreamhostUsed );

		mir_free( jbt );
}	}

static void JabberIqResultProxyDiscovery( XmlNode* iqNode, void* userdata, CJabberIqRequestInfo *pInfo )
{
	JABBER_BYTE_TRANSFER *jbt = ( JABBER_BYTE_TRANSFER * )pInfo->GetUserData();

	if ( pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT ) {
		XmlNode *queryNode = JabberXmlGetChild( iqNode, "query" );
		if ( queryNode ) {
			TCHAR *queryXmlns = JabberXmlGetAttrValue( queryNode, "xmlns" );
			if (queryXmlns && !_tcscmp( queryXmlns, _T(JABBER_FEAT_BYTESTREAMS))) {
				XmlNode *streamHostNode = JabberXmlGetChild( queryNode, "streamhost" );
				if ( streamHostNode ) {
					TCHAR *streamJid = JabberXmlGetAttrValue( streamHostNode, "jid" );
					TCHAR *streamHost = JabberXmlGetAttrValue( streamHostNode, "host" );
					TCHAR *streamPort = JabberXmlGetAttrValue( streamHostNode, "port" );
					if ( streamJid && streamHost && streamPort ) {
						jbt->szProxyHost = mir_tstrdup( streamHost );
						jbt->szProxyJid = mir_tstrdup( streamJid );
						jbt->szProxyPort = mir_tstrdup( streamPort );
						jbt->bProxyDiscovered = TRUE;
	}	}	}	}	}

	if ( jbt->hProxyEvent )
		SetEvent( jbt->hProxyEvent );
}

void __cdecl JabberByteSendThread( JABBER_BYTE_TRANSFER *jbt )
{
	BOOL bDirect, bProxy;
	char* localAddr;
	struct in_addr in;
	DBVARIANT dbv;
	NETLIBBIND nlb = {0};
	TCHAR szPort[8];
	HANDLE hEvent;
	TCHAR* proxyJid;
	CJabberIqRequestInfo *pInfo = NULL;
	int nIqId = 0;

	JabberLog( "Thread started: type=bytestream_send" );

	bDirect = JGetByte( "BsDirect", TRUE );
	bProxy = JGetByte( "BsProxy", TRUE );

	BOOL bProxyManual = JGetByte( "BsProxyManual" , FALSE );
	if ( bProxy && bProxyManual ) {
		proxyJid = NULL;
		if ( !DBGetContactSetting( NULL, jabberProtoName, "BsProxyServer", &dbv )) {
			proxyJid = a2t( dbv.pszVal );
			JFreeVariant( &dbv );
		}

		if ( proxyJid ) {
			jbt->bProxyDiscovered = FALSE;
			jbt->szProxyHost = NULL;
			jbt->szProxyPort = NULL;
			jbt->szProxyJid = NULL;
			jbt->hProxyEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

			pInfo = g_JabberIqRequestManager.AddHandler( JabberIqResultProxyDiscovery, JABBER_IQ_TYPE_GET, proxyJid, 0, -1, jbt );
			nIqId = pInfo->GetIqId();
			XmlNodeIq iq( pInfo );
			XmlNode* query = iq.addQuery( JABBER_FEAT_BYTESTREAMS );
			jabberThreadInfo->send( iq );

			WaitForSingleObject( jbt->hProxyEvent, INFINITE );
			g_JabberIqRequestManager.ExpireIq( nIqId );
			CloseHandle( jbt->hProxyEvent );
			jbt->hProxyEvent = NULL;

			mir_free( proxyJid );
	}	}

	pInfo = g_JabberIqRequestManager.AddHandler( JabberByteInitiateResult, JABBER_IQ_TYPE_SET, jbt->dstJID, 0, -1, jbt );
	nIqId = pInfo->GetIqId();
	XmlNodeIq iq( pInfo );
	XmlNode* query = iq.addQuery( JABBER_FEAT_BYTESTREAMS );
	query->addAttr( "sid", jbt->sid );

	if ( bDirect ) {
		localAddr = NULL;
		if ( JGetByte( "BsDirectManual", FALSE ) == TRUE ) {
			if ( !DBGetContactSetting( NULL, jabberProtoName, "BsDirectAddr", &dbv )) {
				localAddr = mir_strdup( dbv.pszVal );
				JFreeVariant( &dbv );
		}	}

		if ( localAddr == NULL ) {
			in.S_un.S_addr = jabberLocalIP;
			localAddr = mir_strdup( inet_ntoa( in ));
		}
		nlb.cbSize = sizeof( NETLIBBIND );
		nlb.pfnNewConnection = JabberByteSendConnection;
		nlb.wPort = 0;	// Use user-specified incoming port ranges, if available
		jbt->hConn = ( HANDLE ) JCallService( MS_NETLIB_BINDPORT, ( WPARAM ) hNetlibUser, ( LPARAM )&nlb );
		if ( jbt->hConn == NULL ) {
			JabberLog( "Cannot allocate port for bytestream_send thread, thread ended." );
			JabberByteFreeJbt( jbt );
			return;
		}

		mir_sntprintf( szPort, SIZEOF( szPort ), _T("%d"), nlb.wPort );
		JABBER_LIST_ITEM *item = JabberListAdd( LIST_BYTE, szPort );
		item->jbt = jbt;
		hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		jbt->hEvent = hEvent;
		jbt->hSendEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		XmlNode* h = query->addChild( "streamhost" );
		h->addAttr( "jid", jabberThreadInfo->fullJID ); h->addAttr( "host", localAddr ); h->addAttr( "port", nlb.wPort );
		mir_free( localAddr );
	}

	if ( jbt->bProxyDiscovered ) {
		XmlNode* h = query->addChild( "streamhost" );
		h->addAttr( "jid", jbt->szProxyJid );
		h->addAttr( "host", jbt->szProxyHost );
		h->addAttr( "port", jbt->szProxyPort );
	}

	jbt->hProxyEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	jbt->szStreamhostUsed = NULL;

	jabberThreadInfo->send( iq );

	WaitForSingleObject( jbt->hProxyEvent, INFINITE );
	g_JabberIqRequestManager.ExpireIq( nIqId );
	CloseHandle( jbt->hProxyEvent );
	jbt->hProxyEvent = NULL;

	if ( !jbt->szStreamhostUsed ) {
		if (bDirect) {
			SetEvent( jbt->hSendEvent );
			CloseHandle( jbt->hSendEvent );
			CloseHandle( hEvent );
			jbt->hEvent = NULL;
			jbt->pfnFinal(( jbt->state==JBT_DONE )?TRUE:FALSE, jbt->userdata );
			if ( jbt->hConn != NULL )
				Netlib_CloseHandle( jbt->hConn );
			JabberListRemove( LIST_BYTE, szPort );
		}
		JabberByteFreeJbt( jbt );
		return;
	}

	if ( jbt->bProxyDiscovered && !_tcscmp( jbt->szProxyJid, jbt->szStreamhostUsed )) {
		// jabber proxy used
		if ( bDirect ) {
			SetEvent( jbt->hSendEvent );
			CloseHandle( jbt->hSendEvent );
			CloseHandle( hEvent );
			jbt->hEvent = NULL;
			if ( jbt->hConn != NULL )
				Netlib_CloseHandle( jbt->hConn );
			JabberListRemove( LIST_BYTE, szPort );
		}
		JabberByteSendViaProxy( jbt );
	}
	else {
		SetEvent( jbt->hSendEvent );
		WaitForSingleObject( hEvent, INFINITE );
		CloseHandle( hEvent );
		CloseHandle( jbt->hSendEvent );
		jbt->hEvent = NULL;
		jbt->pfnFinal(( jbt->state == JBT_DONE ) ? TRUE : FALSE, jbt->userdata );
		if ( jbt->hConn != NULL )
			Netlib_CloseHandle( jbt->hConn );
		JabberListRemove( LIST_BYTE, szPort );
	}

	JabberByteFreeJbt( jbt );
	JabberLog( "Thread ended: type=bytestream_send" );
}

static void JabberByteInitiateResult( XmlNode *iqNode, void *userdata, CJabberIqRequestInfo *pInfo )
{
	JABBER_BYTE_TRANSFER *jbt = ( JABBER_BYTE_TRANSFER * )pInfo->GetUserData();

	if ( pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT ) {
		XmlNode* queryNode = JabberXmlGetChild( iqNode, "query" );
		if ( queryNode ) {
			TCHAR* queryXmlns = JabberXmlGetAttrValue( queryNode, "xmlns" );
			if ( queryXmlns && !_tcscmp( queryXmlns, _T( JABBER_FEAT_BYTESTREAMS ))) {
				XmlNode* streamHostNode = JabberXmlGetChild( queryNode, "streamhost-used" );
				if ( streamHostNode ) {
					TCHAR* streamJid = JabberXmlGetAttrValue( streamHostNode, "jid" );
					if ( streamJid )
						jbt->szStreamhostUsed = mir_tstrdup( streamJid );
	}	}	}	}

	if ( jbt->hProxyEvent )
		SetEvent( jbt->hProxyEvent );
}

static void JabberByteSendConnection( HANDLE hConn, DWORD dwRemoteIP )
{
	SOCKET s;
	SOCKADDR_IN saddr;
	int len;
	WORD localPort;
	TCHAR szPort[8];
	JABBER_BYTE_TRANSFER *jbt;
	int recvResult, bytesParsed;
	HANDLE hListen;
	JABBER_LIST_ITEM *item;
	char* buffer;
	int datalen;

	localPort = 0;
	if (( s = JCallService( MS_NETLIB_GETSOCKET, ( WPARAM ) hConn, 0 )) != INVALID_SOCKET ) {
		len = sizeof( saddr );
		if ( getsockname( s, ( SOCKADDR * ) &saddr, &len ) != SOCKET_ERROR )
			localPort = ntohs( saddr.sin_port );
	}
	if ( localPort == 0 ) {
		JabberLog( "bytestream_send_connection unable to determine the local port, connection closed." );
		Netlib_CloseHandle( hConn );
		return;
	}

	mir_sntprintf( szPort, SIZEOF( szPort ), _T("%d"), localPort );
	JabberLog( "bytestream_send_connection incoming connection accepted: local_port=" TCHAR_STR_PARAM, szPort );

	if (( item = JabberListGetItemPtr( LIST_BYTE, szPort )) == NULL ) {
		JabberLog( "No bytestream session is currently active, connection closed." );
		Netlib_CloseHandle( hConn );
		return;
	}

	jbt = item->jbt;

	if (( buffer = ( char* )mir_alloc( JABBER_NETWORK_BUFFER_SIZE )) == NULL ) {
		JabberLog( "bytestream_send cannot allocate network buffer, connection closed." );
		jbt->state = JBT_ERROR;
		Netlib_CloseHandle( hConn );
		if ( jbt->hEvent != NULL ) SetEvent( jbt->hEvent );
		return;
	}

	hListen = jbt->hConn;
	jbt->hConn = hConn;
	jbt->state = JBT_INIT;
	datalen = 0;
	while ( jbt->state!=JBT_DONE && jbt->state!=JBT_ERROR ) {
		recvResult = Netlib_Recv( hConn, buffer+datalen, JABBER_NETWORK_BUFFER_SIZE-datalen, 0 );
		if ( recvResult <= 0 )
			break;

		datalen += recvResult;
		bytesParsed = JabberByteSendParse( hConn, jbt, buffer, datalen );
		if ( bytesParsed < datalen )
			memmove( buffer, buffer+bytesParsed, datalen-bytesParsed );
		datalen -= bytesParsed;
	}

	if ( jbt->hConn )
		Netlib_CloseHandle( jbt->hConn );

	JabberLog( "bytestream_send_connection closing connection" );
	jbt->hConn = hListen;
	mir_free( buffer );

	if ( jbt->hEvent != NULL )
		SetEvent( jbt->hEvent );
}

static int JabberByteSendParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen )
{
	int nMethods;
	BYTE data[10];
	int i;
	char* str;

	switch ( jbt->state ) {
	case JBT_INIT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 nmethods
		// 02-xx list of methods ( nmethods bytes )
		// send:
		// 00-00 ver ( 0x05 )
		// 01-01 select method ( 0=no auth required )
		if ( datalen>=2 && buffer[0]==5 && buffer[1]+2==datalen ) {
			nMethods = buffer[1];
			for ( i=0; i<nMethods && buffer[2+i]!=0; i++ );
			if ( i < nMethods ) {
				data[1] = 0;
				jbt->state = JBT_CONNECT;
			}
			else {
				data[1] = 0xff;
				jbt->state = JBT_ERROR;
			}
			data[0] = 5;
			Netlib_Send( hConn, ( char* )data, 2, 0 );
		}
		else jbt->state = JBT_ERROR;
		break;

	case JBT_CONNECT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 cmd ( 1=connect )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 3 )
		// 04-44 dst.addr ( 41 bytes: 1-byte length, 40-byte SHA1 hash of [sid,srcJID,dstJID] )
		// 45-46 dst.port ( 0 )
		// send:
		// 00-00 ver ( 0x05 )
		// 01-01 reply ( 0=success,2=not allowed )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 1=IPv4 address )
		// 04-07 bnd.addr server bound address
		// 08-09 bnd.port server bound port
		if ( datalen == 47 && *(( DWORD* )buffer )==0x03000105 && buffer[4]==40 && *(( WORD* )( buffer+45 ))==0 ) {
			TCHAR text[256];

			TCHAR *szInitiatorJid = PrepareJid(jbt->srcJID);
			TCHAR *szTargetJid = PrepareJid(jbt->dstJID);
			mir_sntprintf( text, SIZEOF( text ), _T("%s%s%s"), jbt->sid, szInitiatorJid, szTargetJid );
			mir_free(szInitiatorJid);
			mir_free(szTargetJid);

			char* szAuthString = mir_utf8encodeT( text );
			JabberLog( "Auth: '%s'", szAuthString );
			if (( str = JabberSha1( szAuthString )) != NULL ) {
				for ( i=0; i<40 && buffer[i+5]==str[i]; i++ );
				mir_free( str );

				ZeroMemory( data, 10 );
				data[1] = ( i>=20 )?0:2;
				data[0] = 5;
				data[3] = 1;
				Netlib_Send( hConn, ( char* )data, 10, 0 );

				// wait stream activation
				WaitForSingleObject( jbt->hSendEvent, INFINITE );

				if ( i>=20 && jbt->pfnSend( hConn, jbt->userdata )==TRUE )
					jbt->state = JBT_DONE;
				else
					jbt->state = JBT_ERROR;
			}
			mir_free( szAuthString );
		}
		else
			jbt->state = JBT_ERROR;
		break;
	}

	return datalen;
}

///////////////// Bytestream receiving /////////////////////////

static int JabberByteReceiveParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen );

static void JabberIqResultStreamActivate( XmlNode* iqNode, void* userdata )
{
	int id = JabberGetPacketID( iqNode );

	TCHAR listJid[256];
	mir_sntprintf(listJid, SIZEOF( listJid ), _T("ftproxy_%d"), id);

	JABBER_LIST_ITEM *item = JabberListGetItemPtr( LIST_FTIQID, listJid );
	if ( !item )
		return;

	if ( !lstrcmp( JabberXmlGetAttrValue( iqNode, "type" ), _T( "result" )))
		item->jbt->bStreamActivated = TRUE;

	if ( item->jbt->hProxyEvent )
		SetEvent( item->jbt->hProxyEvent );
}


void JabberByteSendViaProxy( JABBER_BYTE_TRANSFER *jbt )
{
	TCHAR *szHost, *szPort;
	WORD port;
	HANDLE hConn;
	char data[3];
	char* buffer;
	int datalen, bytesParsed, recvResult;
	BOOL validStreamhost;

	if ( jbt == NULL ) return;
	if (( buffer=( char* )mir_alloc( JABBER_NETWORK_BUFFER_SIZE )) == NULL ) {
		XmlNodeIq iq( "error", jbt->iqId, jbt->srcJID );
		XmlNode* e = iq.addChild( "error" ); e->addAttr( "code", 406 ); e->addAttr( "type", "auth" );
		XmlNode* na = e->addChild( "not-acceptable" ); na->addAttr( "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas" );
		jabberThreadInfo->send( iq );
		return;
	}

	jbt->state = JBT_INIT;
	validStreamhost = FALSE;
	szPort = jbt->szProxyPort;
	szHost = jbt->szProxyHost;

	port = ( WORD )_ttoi( szPort );
	if ( jbt->streamhostJID ) mir_free( jbt->streamhostJID );
	jbt->streamhostJID = mir_tstrdup( jbt->szProxyJid );

	NETLIBOPENCONNECTION nloc = { 0 };
	nloc.cbSize = sizeof( nloc );
	#if defined( _UNICODE )
		nloc.szHost = u2a(szHost);
	#else
		nloc.szHost = szHost;
	#endif
	nloc.wPort = port;
	hConn = ( HANDLE ) JCallService( MS_NETLIB_OPENCONNECTION, ( WPARAM ) hNetlibUser, ( LPARAM )&nloc );
	#if defined( _UNICODE )
		mir_free((void*)nloc.szHost);
	#endif
	if ( hConn != NULL ) {
		jbt->hConn = hConn;

		data[0] = 5;
		data[1] = 1;
		data[2] = 0;
		Netlib_Send( hConn, data, 3, 0 );

		jbt->state = JBT_INIT;
		datalen = 0;
		while ( jbt->state!=JBT_DONE && jbt->state!=JBT_ERROR && jbt->state!=JBT_SOCKSERR ) {
			recvResult = Netlib_Recv( hConn, buffer+datalen, JABBER_NETWORK_BUFFER_SIZE-datalen, 0 );
			if ( recvResult <= 0 )
				break;

			datalen += recvResult;
			bytesParsed = JabberByteSendProxyParse( hConn, jbt, buffer, datalen );
			if ( bytesParsed < datalen )
				memmove( buffer, buffer+bytesParsed, datalen-bytesParsed );
			datalen -= bytesParsed;
			if ( jbt->state == JBT_DONE ) validStreamhost = TRUE;
		}
		Netlib_CloseHandle( hConn );
	}
	mir_free( buffer );
	jbt->pfnFinal(( jbt->state == JBT_DONE ) ? TRUE : FALSE, jbt->userdata );
	if ( !validStreamhost ) {
		XmlNodeIq iq( "error", jbt->iqId, jbt->srcJID );
		XmlNode* e = iq.addChild( "error" ); e->addAttr( "code", 404 ); e->addAttr( "type", _T("cancel"));
		XmlNode* na = e->addChild( "item-not-found" ); na->addAttr( "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas" );
		jabberThreadInfo->send( iq );
}	}

static int JabberByteSendProxyParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen )
{
	int num = datalen;

	switch ( jbt->state ) {
	case JBT_INIT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 selected method ( 0=no auth, 0xff=error )
		// send:
		// 00-00 ver ( 0x05 )
		// 01-01 cmd ( 1=connect )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 3 )
		// 04-44 dst.addr ( 41 bytes: 1-byte length, 40-byte SHA1 hash of [sid,srcJID,dstJID] )
		// 45-46 dst.port ( 0 )
		if ( datalen==2 && buffer[0]==5 && buffer[1]==0 ) {
			BYTE data[47];
			ZeroMemory( data, sizeof( data ));
			*(( DWORD* )data ) = 0x03000105;
			data[4] = 40;

			TCHAR text[256];

			TCHAR *szInitiatorJid = PrepareJid(jbt->srcJID);
			TCHAR *szTargetJid = PrepareJid(jbt->dstJID);
			mir_sntprintf( text, SIZEOF( text ), _T("%s%s%s"), jbt->sid, szInitiatorJid, szTargetJid );
			mir_free(szInitiatorJid);
			mir_free(szTargetJid);

			char* szAuthString = mir_utf8encodeT( text );
			JabberLog( "Auth: '%s'", szAuthString );
			char* szHash = JabberSha1( szAuthString );
			strncpy(( char* )( data+5 ), szHash, 40 );
			mir_free( szHash );
			Netlib_Send( hConn, ( char* )data, 47, 0 );
			jbt->state = JBT_CONNECT;
			mir_free( szAuthString );
		}
		else jbt->state = JBT_SOCKSERR;
		break;

	case JBT_CONNECT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 reply ( 0=success,2=not allowed )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 1=IPv4 address,3=host address )
		// 04-mm bnd.addr server bound address ( 4-byte IP if IPv4, 1-byte length + n-byte host address string if host address )
		// nn-nn+1 bnd.port server bound port
		if ( datalen>=5 && buffer[0]==5 && buffer[1]==0 && ( buffer[3]==1 || buffer[3]==3 || buffer[3]==0 )) {
			if ( buffer[3]==1 && datalen>=10 )
				num = 10;
			else if ( buffer[3]==3 && datalen>=buffer[4]+7 )
				num = buffer[4] + 7;
			else if ( buffer[3]==0 && datalen>=6 )
				num = 6;
			else {
				jbt->state = JBT_SOCKSERR;
				break;
			}
			jbt->state = JBT_SENDING;

			jbt->hProxyEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
			jbt->bStreamActivated = FALSE;

			int iqId = JabberSerialNext();

			TCHAR listJid[256];
			mir_sntprintf(listJid, SIZEOF( listJid ), _T("ftproxy_%d"), iqId);

			JABBER_LIST_ITEM *item = JabberListAdd( LIST_FTIQID, listJid );
			item->jbt = jbt;

			JabberIqAdd( iqId, IQ_PROC_NONE, JabberIqResultStreamActivate );
			XmlNodeIq iq( "set", iqId, jbt->streamhostJID );
			XmlNode* query = iq.addQuery( JABBER_FEAT_BYTESTREAMS );
			query->addAttr( "sid", jbt->sid );
			XmlNode* stream = query->addChild( "activate", jbt->dstJID );

			jabberThreadInfo->send( iq );

			WaitForSingleObject( jbt->hProxyEvent, INFINITE );

			CloseHandle( jbt->hProxyEvent );
			jbt->hProxyEvent = NULL;

			JabberListRemove( LIST_FTIQID, listJid );

			if ( jbt->bStreamActivated) 
				jbt->state = jbt->pfnSend( hConn, jbt->userdata ) ? JBT_DONE : JBT_ERROR;
			else
				jbt->state = JBT_ERROR;
		}
		else jbt->state = JBT_SOCKSERR;
		break;
	}

	return num;
}


void __cdecl JabberByteReceiveThread( JABBER_BYTE_TRANSFER *jbt )
{
	XmlNode *iqNode, *queryNode, *n;
	TCHAR *from, *to, *sid, *szId, *szHost, *szPort, *str;
	int i;
	WORD port;
	HANDLE hConn;
	char data[3];
	char* buffer;
	int datalen, bytesParsed, recvResult;
	BOOL validStreamhost;

	if ( jbt == NULL ) return;
	if (( iqNode=jbt->iqNode )!=NULL &&
		( from=JabberXmlGetAttrValue( iqNode, "from" ))!=NULL &&
		( to=JabberXmlGetAttrValue( iqNode, "to" ))!=NULL &&
		( queryNode=JabberXmlGetChild( iqNode, "query" ))!=NULL &&
		( sid=JabberXmlGetAttrValue( queryNode, "sid" ))!=NULL &&
		( n=JabberXmlGetChild( queryNode, "streamhost" ))!=NULL ) {

		szId = JabberXmlGetAttrValue( iqNode, "id" );
		jbt->iqId = mir_tstrdup( szId );
		jbt->srcJID = mir_tstrdup( from );
		jbt->dstJID = mir_tstrdup( to );
		jbt->sid = mir_tstrdup( sid );

		if (( buffer=( char* )mir_alloc( JABBER_NETWORK_BUFFER_SIZE )) == NULL ) {
			JabberLog( "bytestream_send cannot allocate network buffer, connection closed." );

			XmlNodeIq iq( "error", jbt->iqId, jbt->srcJID );
			XmlNode* e = iq.addChild( "error" ); e->addAttr( "code", 406 ); e->addAttr( "type", "auth" );
			XmlNode* na = e->addChild( "not-acceptable" ); na->addAttr( "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas" );
			jabberThreadInfo->send( iq );

			JabberByteFreeJbt( jbt );
			return;
		}

		jbt->state = JBT_INIT;
		validStreamhost = FALSE;
		for ( i=1; ( n=JabberXmlGetNthChild( queryNode, "streamhost", i ))!=NULL; i++ ) {
			if (( szHost=JabberXmlGetAttrValue( n, "host" ))!=NULL &&
				( szPort=JabberXmlGetAttrValue( n, "port" ))!=NULL &&
				( str=JabberXmlGetAttrValue( n, "jid" ))!=NULL ) {

				port = ( WORD )_ttoi( szPort );
				if ( jbt->streamhostJID ) mir_free( jbt->streamhostJID );
				jbt->streamhostJID = mir_tstrdup( str );

				JabberLog( "bytestream_recv connecting to " TCHAR_STR_PARAM ":%d", szHost, port );
				NETLIBOPENCONNECTION nloc = { 0 };
				nloc.cbSize = sizeof( nloc );
				#if defined( _UNICODE )
					nloc.szHost = u2a(szHost);
				#else
					nloc.szHost = szHost;
				#endif
				nloc.wPort = port;
				hConn = ( HANDLE ) JCallService( MS_NETLIB_OPENCONNECTION, ( WPARAM ) hNetlibUser, ( LPARAM )&nloc );
				#if defined( _UNICODE )
					mir_free((void*)nloc.szHost);
				#endif
				if ( hConn == NULL ) {
					JabberLog( "bytestream_recv_connection connection failed ( %d ), try next streamhost", WSAGetLastError());
					continue;
				}

				jbt->hConn = hConn;

				data[0] = 5;
				data[1] = 1;
				data[2] = 0;
				Netlib_Send( hConn, data, 3, 0 );

				jbt->state = JBT_INIT;
				datalen = 0;
				while ( jbt->state!=JBT_DONE && jbt->state!=JBT_ERROR && jbt->state!=JBT_SOCKSERR ) {
					recvResult = Netlib_Recv( hConn, buffer+datalen, JABBER_NETWORK_BUFFER_SIZE-datalen, 0 );
					if ( recvResult <= 0 ) break;
					datalen += recvResult;
					bytesParsed = JabberByteReceiveParse( hConn, jbt, buffer, datalen );
					if ( bytesParsed < datalen )
						memmove( buffer, buffer+bytesParsed, datalen-bytesParsed );
					datalen -= bytesParsed;
					if ( jbt->state == JBT_RECVING ) validStreamhost = TRUE;
				}
				Netlib_CloseHandle( hConn );
				JabberLog( "bytestream_recv_connection closing connection" );
			}
			if ( jbt->state==JBT_ERROR || validStreamhost==TRUE )
				break;
			JabberLog( "bytestream_recv_connection stream cannot be established, try next streamhost" );
		}
		mir_free( buffer );
		jbt->pfnFinal(( jbt->state==JBT_DONE )?TRUE:FALSE, jbt->userdata );
		if ( !validStreamhost ) {
			JabberLog( "bytestream_recv_connection session not completed" );

			XmlNodeIq iq( "error", jbt->iqId, jbt->srcJID );
			XmlNode* e = iq.addChild( "error" ); e->addAttr( "code", 404 ); e->addAttr( "type", _T("cancel"));
			XmlNode* na = e->addChild( "item-not-found" ); na->addAttr( "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas" );
			jabberThreadInfo->send( iq );
	}	}

	JabberByteFreeJbt( jbt );
	JabberLog( "Thread ended: type=bytestream_recv" );
}

static int JabberByteReceiveParse( HANDLE hConn, JABBER_BYTE_TRANSFER *jbt, char* buffer, int datalen )
{
	int bytesReceived, num = datalen;

	switch ( jbt->state ) {
	case JBT_INIT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 selected method ( 0=no auth, 0xff=error )
		// send:
		// 00-00 ver ( 0x05 )
		// 01-01 cmd ( 1=connect )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 3 )
		// 04-44 dst.addr ( 41 bytes: 1-byte length, 40-byte SHA1 hash of [sid,srcJID,dstJID] )
		// 45-46 dst.port ( 0 )
		if ( datalen==2 && buffer[0]==5 && buffer[1]==0 ) {
			BYTE data[47];
			ZeroMemory( data, sizeof( data ));
			*(( DWORD* )data ) = 0x03000105;
			data[4] = 40;

			TCHAR text[256];
			TCHAR *szInitiatorJid = PrepareJid(jbt->srcJID);
			TCHAR *szTargetJid = PrepareJid(jbt->dstJID);
			mir_sntprintf( text, SIZEOF( text ), _T("%s%s%s"), jbt->sid, szInitiatorJid, szTargetJid );
			mir_free(szInitiatorJid);
			mir_free(szTargetJid);
			char* szAuthString = mir_utf8encodeT( text );
			JabberLog( "Auth: '%s'", szAuthString );
			char* szHash = JabberSha1( szAuthString );
			strncpy(( char* )( data+5 ), szHash, 40 );
			mir_free( szHash );
			Netlib_Send( hConn, ( char* )data, 47, 0 );
			jbt->state = JBT_CONNECT;
			mir_free( szAuthString );
		}
		else jbt->state = JBT_SOCKSERR;
		break;

	case JBT_CONNECT:
		// received:
		// 00-00 ver ( 0x05 )
		// 01-01 reply ( 0=success,2=not allowed )
		// 02-02 reserved ( 0 )
		// 03-03 address type ( 1=IPv4 address,3=host address )
		// 04-mm bnd.addr server bound address ( 4-byte IP if IPv4, 1-byte length + n-byte host address string if host address )
		// nn-nn+1 bnd.port server bound port
		if ( datalen>=5 && buffer[0]==5 && buffer[1]==0 && ( buffer[3]==1 || buffer[3]==3 || buffer[3]==0 )) {
			if ( buffer[3]==1 && datalen>=10 )
				num = 10;
			else if ( buffer[3]==3 && datalen>=buffer[4]+7 )
				num = buffer[4] + 7;
			else if ( buffer[3]==0 && datalen>=6 )
				num = 6;
			else {
				jbt->state = JBT_SOCKSERR;
				break;
			}
			jbt->state = JBT_RECVING;

			XmlNodeIq iq( "result", jbt->iqId, jbt->srcJID );
			XmlNode* query = iq.addQuery( JABBER_FEAT_BYTESTREAMS );
			XmlNode* stream = query->addChild( "streamhost-used" ); stream->addAttr( "jid", jbt->streamhostJID );
			jabberThreadInfo->send( iq );
		}
		else jbt->state = JBT_SOCKSERR;
		break;

	case JBT_RECVING:
		bytesReceived = jbt->pfnRecv( hConn, jbt->userdata, buffer, datalen );
		if ( bytesReceived < 0 )
			jbt->state = JBT_ERROR;
		else if ( bytesReceived == 0 )
			jbt->state = JBT_DONE;
		break;
	}

	return num;
}
