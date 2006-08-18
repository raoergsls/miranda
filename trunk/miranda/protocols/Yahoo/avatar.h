/*
 * $Id$
 *
 * myYahoo Miranda Plugin 
 *
 * Authors: Gennady Feldman (aka Gena01) 
 *          Laurent Marechal (aka Peorth)
 *
 * This code is under GPL and is based on AIM, MSN and Miranda source code.
 * I want to thank Robert Rainwater and George Hazan for their code and support
 * and for answering some of my questions during development of this plugin.
 */
#ifndef _YAHOO_AVATAR_H_
#define _YAHOO_AVATAR_H_

void YAHOO_request_avatar(const char* who);
void GetAvatarFileName(HANDLE hContact, char* pszDest, int cbLen, int type);

void YAHOO_SendAvatar(const char *szFile);

void YAHOO_set_avatar(int buddy_icon);

void YAHOO_bcast_picture_update(int buddy_icon);

void YAHOO_bcast_picture_checksum(int cksum);

int YAHOO_SaveBitmapAsAvatar( HBITMAP hBitmap, const char* szFileName );

HBITMAP YAHOO_StretchBitmap( HBITMAP hBitmap );

void yahoo_reset_avatar(HANDLE 	hContact);

HBITMAP YAHOO_SetAvatar(const char *szFile);

void YAHOO_get_avatar(const char *who, const char *pic_url, long cksum);

#endif
