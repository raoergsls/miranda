/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol.
Copyright (c) 2006-7 Boris Krasnovskiy.
Copyright (c) 2003-5 George Hazan.
Copyright (c) 2002-3 Richard Hughes (original version).

Miranda IM: the free icq client for MS Windows
Copyright (C) 2000-2002 Richard Hughes, Roland Rabien & Tristan Van de Vreede

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

#include "msn_global.h"

HANDLE  MSN_HContactFromEmail( const char* msnEmail, const char* msnNick, int addIfNeeded, int temporary )
{
	HANDLE hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
	while ( hContact != NULL )
	{
		if ( MSN_IsMyContact( hContact )) {
			char tEmail[ MSN_MAX_EMAIL_LEN ];
			if ( !MSN_GetStaticString( "e-mail", hContact, tEmail, sizeof( tEmail )))
				if ( !_stricmp( msnEmail, tEmail ))
					return hContact;
		}

		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 );
	}

	if ( addIfNeeded )
	{
		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_ADD, 0, 0 );
		MSN_CallService( MS_PROTO_ADDTOCONTACT, ( WPARAM )hContact, ( LPARAM )msnProtocolName );
		MSN_SetString( hContact, "e-mail", msnEmail );
		MSN_SetStringUtf( hContact, "Nick", ( char* )msnNick );
		if ( temporary )
			DBWriteContactSettingByte( hContact, "CList", "NotOnList", 1 );

		return hContact;
	}

	return NULL;
}

HANDLE  MSN_HContactFromEmailT( const TCHAR* msnEmail )
{
	HANDLE hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
	while ( hContact != NULL )
	{
		if ( MSN_IsMyContact( hContact )) {
			DBVARIANT dbv;
			if ( !MSN_GetStringT( "e-mail", hContact, &dbv ))
				if ( !lstrcmpi( msnEmail, dbv.ptszVal ))
					return hContact;
		}

		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 );
	}

	return NULL;
}

HANDLE  MSN_HContactById( const char* szGuid )
{
	HANDLE hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
	while ( hContact != NULL )
	{
		if ( MSN_IsMyContact( hContact )) {
			char tId[ 100 ];
			if ( !MSN_GetStaticString( "ID", hContact, tId, sizeof tId ))
				if ( !_stricmp( szGuid, tId ))
					return hContact;
		}

		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 );
	}

	return NULL;
}


static void AddDelUserContList(const char* email, const int list, const int netId, const bool del)
{
	char buf[512];
	size_t sz;

	const char* dom = strchr(email, '@');
	if (dom == NULL)
	{
		sz = mir_snprintf(buf, sizeof(buf),
			"<ml><t><c n=\"%s\" l=\"%d\"/></t></ml>",
			email, list);
	}
	else
	{
		*(char*)dom = 0;
		sz = mir_snprintf(buf, sizeof(buf),
			"<ml><d n=\"%s\"><c n=\"%s\" l=\"%d\" t=\"%d\"/></d></ml>",
			dom+1, email, list, netId);
		*(char*)dom = '@';
	}
	msnNsThread->sendPacket(del ? "RML" : "ADL", "%d\r\n%s", sz, buf);

	if (del)
		Lists_Remove(list, email);
	else
		Lists_Add(list, netId, email);
}


/////////////////////////////////////////////////////////////////////////////////////////
// MSN_AddUser - adds a e-mail address to one of the MSN server lists

bool MSN_AddUser( HANDLE hContact, const char* email, int netId, int flags )
{
	bool needRemove = (flags & LIST_REMOVE) != 0;
	flags &= 0xFF;

	if (needRemove != Lists_IsInList(flags, email))
		return true;

	bool res = false;
	if (flags == LIST_FL) 
	{
		if (needRemove) 
		{
			if (hContact == NULL)
			{
				hContact = MSN_HContactFromEmail( email, NULL, 0, 0 );
				if ( hContact == NULL ) return false;
			}

			char id[ MSN_GUID_LEN ];
			if ( !MSN_GetStaticString( "ID", hContact, id, sizeof( id ))) 
			{
				res = MSN_ABAddDelContactGroup(id , NULL, "ABContactDelete");
				if (res) AddDelUserContList(email, flags, Lists_GetNetId(email), true);
			}
		}
		else 
		{
			DBVARIANT dbv = {0};
			if ( !strcmp( email, MyOptions.szEmail ))
				DBGetContactSettingStringUtf( NULL, msnProtocolName, "Nick", &dbv );

			unsigned res1 = MSN_ABContactAdd(email, dbv.pszVal, netId, false);
			if (netId == 1 && res1 == 2)
			{
				netId = 2;
				res = MSN_ABContactAdd(email, dbv.pszVal, netId, false) == 0;
			}
			else
				res = (res1 == 0);

			if (res)
				AddDelUserContList(email, flags, netId, false);
			else
			{
				if (netId == 1 && strstr(email, "@yahoo.com") != 0)
					MSN_FindYahooUser(email);
			}
			MSN_FreeVariant( &dbv );
		}
	}
	else {
		const char* listName = (flags & LIST_AL) ? "Allow" :  "Block";
		res = MSN_SharingAddDelMember(email, listName, needRemove ? "DeleteMember" : "AddMember");
		if (res) AddDelUserContList(email, flags, Lists_GetNetId(email), needRemove);
	}
	return res;
}


void MSN_FindYahooUser(const char* email)
{
	const char* dom = strchr(email, '@');
	if (dom)
	{
		char buf[512];
		size_t sz;

		*(char*)dom = '\0';
		sz = mir_snprintf(buf, sizeof(buf), "<ml><d n=\"%s\"><c n=\"%s\"/></d></ml>", dom+1, email);
		*(char*)dom = '@';
		msnNsThread->sendPacket("FQY", "%d\r\n%s", sz, buf);
	}
}

void MSN_RefreshContactList(void)
{
	Lists_Wipe();
	MSN_SharingFindMembership();
	MSN_ABGetFull();
	MSN_CleanupLists();

	msnLoggedIn = true;

	MSN_CreateContList();
}
