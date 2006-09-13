// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright � 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright � 2001,2002 Jon Keating, Richard Hughes
// Copyright � 2002,2003,2004 Martin �berg, Sam Kothari, Robert Rainwater
// Copyright � 2004,2005,2006 Joe Kucera
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
//
// -----------------------------------------------------------------------------
//
// File name      : $Source: /cvsroot/miranda/miranda/protocols/IcqOscarJ/icq_server.h,v $
// Revision       : $Revision$
// Last change on : $Date$
// Last change by : $Author$
//
// DESCRIPTION:
//
//  Declarations for server thread
//
// -----------------------------------------------------------------------------

#ifndef __ICQ_SERVER_H
#define __ICQ_SERVER_H

typedef struct serverthread_start_info_s {
  NETLIBOPENCONNECTION nloc;
  WORD wPassLen;
  char szPass[20];
} serverthread_start_info;

typedef struct serverthread_info_s {
  int bLoggedIn;
  int isLoginServer;
  BYTE szAuthKey[20];
  WORD wAuthKeyLen;
  WORD wServerPort;
  char *newServer;
  BYTE* cookieData;
  int cookieDataLen;
  int newServerReady;
  int isMigrating;
  int bReinitRecver;
  int bMyAvatarInited;
} serverthread_info;

/*---------* Functions *---------------*/

void icq_serverDisconnect(BOOL bBlock);
void sendServPacket(icq_packet *);
void icq_login(const char *szPassword);


#endif /* __ICQ_SERVER_H */
