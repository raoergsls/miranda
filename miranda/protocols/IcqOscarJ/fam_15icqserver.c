// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright � 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright � 2001,2002 Jon Keating, Richard Hughes
// Copyright � 2002,2003,2004 Martin �berg, Sam Kothari, Robert Rainwater
// Copyright � 2004,2005 Joe Kucera
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
// File name      : $Source$
// Revision       : $Revision$
// Last change on : $Date$
// Last change by : $Author$
//
// DESCRIPTION:
//
//  Describe me here please...
//
// -----------------------------------------------------------------------------

#include "icqoscar.h"



static void handleExtensionError(unsigned char *buf, WORD wPackLen);
static void handleExtensionServerInfo(unsigned char *buf, WORD wPackLen, WORD wFlags);
static void parseOfflineMessage(unsigned char *databuf, WORD wPacketLen);
static void parseEndOfOfflineMessages(unsigned char *databuf, WORD wPacketLen);
static void handleExtensionMetaResponse(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wFlags);
static void parseSearchReplies(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wReplySubtype, BYTE bResultCode);
static void parseUserInfoRequestReplies(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wFlags, WORD wReplySubtype, BYTE bResultCode);
static void parseUserInfoUpdateAck(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wReplySubtype, BYTE bResultCode);



void handleIcqExtensionsFam(unsigned char *pBuffer, WORD wBufferLength, snac_header* pSnacHeader)
{
  switch (pSnacHeader->wSubtype)
  {

  case SRV_ICQEXT_ERROR:
    handleExtensionError(pBuffer, wBufferLength);
    break;

  case SRV_META_REPLY:
    handleExtensionServerInfo(pBuffer, wBufferLength, pSnacHeader->wFlags);
    break;

  default:
    NetLog_Server("Warning: Ignoring SNAC(x%02x,x%02x) - Unknown SNAC (Flags: %u, Ref: %u)", ICQ_EXTENSIONS_FAMILY, pSnacHeader->wSubtype, pSnacHeader->wFlags, pSnacHeader->dwRef);
    break;
  }
}



static void handleExtensionError(unsigned char *buf, WORD wPackLen)
{
  WORD wErrorCode;

  if (wPackLen < 2)
  {
    wErrorCode = 0;
  }
  if (wPackLen >= 2 && wPackLen <= 6)
  {
    unpackWord(&buf, &wErrorCode);
  }
  else
  { // TODO: cookies need to be handled and freed here on error
    oscar_tlv_chain *chain = NULL;

    unpackWord(&buf, &wErrorCode);
    wPackLen -= 2;
    chain = readIntoTLVChain(&buf, wPackLen, 0);
    if (chain)
    {
      oscar_tlv* pTLV;

      pTLV = getTLV(chain, 0x21, 1); // get meta error data
      if (pTLV && pTLV->wLen >= 8)
      {
        unsigned char* pBuffer = pTLV->pData;
        WORD wData;
        pBuffer += 6;
        unpackLEWord(&pBuffer, &wData); // get request type
        switch (wData)
        {
        case CLI_OFFLINE_MESSAGE_REQ: 
          NetLog_Server("Offline messages request failed with error 0x%02x", wData, wErrorCode);
          break;

        case CLI_DELETE_OFFLINE_MSGS_REQ:
          NetLog_Server("Deleting offline messages from server failed with error 0x%02x", wErrorCode);
          icq_LogMessage(LOG_WARNING, "Deleting Offline Messages from server failed.\nYou will probably receive them again.");
          break;

        case CLI_META_INFO_REQ:
          if (pTLV->wLen >= 12)
          {
            WORD wSubType;
            WORD wCookie;

            unpackWord(&pBuffer, &wCookie);
            unpackLEWord(&pBuffer, &wSubType);
            // more sofisticated detection, send ack
            if (wSubType == META_REQUEST_FULL_INFO)
            {
              DWORD dwCookieUin;
              fam15_cookie_data* pCookieData = NULL;
              int foundCookie;

              foundCookie = FindCookie(wCookie, &dwCookieUin, (void**)&pCookieData);
              if (foundCookie && pCookieData)
              {
                HANDLE hContact = HContactFromUIN(dwCookieUin, NULL);
                ICQBroadcastAck(hContact,  ACKTYPE_GETINFO, ACKRESULT_FAILED, (HANDLE)1 ,0);

                SAFE_FREE(&pCookieData); // we do not leak cookie and memory
                FreeCookie(wCookie); 
              }

              NetLog_Server("Full info request error 0x%02x received", wErrorCode);
            }
          }
          else 
            NetLog_Server("Meta request error 0x%02x received", wErrorCode);

          break;

        default:
          NetLog_Server("Unknown request 0x%02x error 0x%02x received", wData, wErrorCode);
        }
        disposeChain(&chain);
        return;
      }
      disposeChain(&chain);
    }
  }
  LogFamilyError(ICQ_EXTENSIONS_FAMILY, wErrorCode);
}



static void handleExtensionServerInfo(unsigned char *buf, WORD wPackLen, WORD wFlags)
{
  WORD wBytesRemaining;
  WORD wRequestType;
  WORD wCookie;
  DWORD dwMyUin;
  oscar_tlv_chain* chain;
  oscar_tlv* dataTlv;
  unsigned char* databuf;
  
  
  // The entire packet is encapsulated in a TLV type 1
  chain = readIntoTLVChain(&buf, wPackLen, 0);
  if (chain == NULL)
  {
    NetLog_Server("Error: Broken snac 15/3 %d", 1);
    return;
  }

  dataTlv = getTLV(chain, 0x0001, 1);
  if (dataTlv == NULL)
  {
    disposeChain(&chain);
    NetLog_Server("Error: Broken snac 15/3 %d", 2);
    return;
  }
  databuf = dataTlv->pData;
  wPackLen -= 4;
  
  _ASSERTE(dataTlv->wLen == wPackLen);
  _ASSERTE(wPackLen >= 10);

  if ((dataTlv->wLen == wPackLen) && (wPackLen >= 10))
  {
    unpackLEWord(&databuf, &wBytesRemaining);
    unpackLEDWord(&databuf, &dwMyUin);
    unpackLEWord(&databuf, &wRequestType);
    unpackWord(&databuf, &wCookie);
    
    _ASSERTE(wBytesRemaining == (wPackLen - 2));
    if (wBytesRemaining == (wPackLen - 2))
    {
      wPackLen -= 10;      
      switch (wRequestType)
      {
      case SRV_OFFLINE_MESSAGE:     // This is an offline message
        parseOfflineMessage(databuf, wPackLen);
        break;
        
      case SRV_END_OF_OFFLINE_MSGS: // This packets marks the end of offline messages
        parseEndOfOfflineMessages(databuf, wPackLen);
        break;
        
      case SRV_META_INFO_REPLY:     // SRV_META request replies
        handleExtensionMetaResponse(databuf, wPackLen, wCookie, wFlags);
        break;
      }
    }
  }
  else
  {
    NetLog_Server("Error: Broken snac 15/3 %d", 3);
  }

  if (chain)
    disposeChain(&chain);
}



static void parseOfflineMessage(unsigned char *databuf, WORD wPacketLen)
{
  _ASSERTE(wPacketLen >= 14);
  if (wPacketLen >= 14)
  {
    DWORD dwUin;
    DWORD dwTimestamp;
    WORD wYear;
    WORD wMsgLen;
    BYTE nMonth;
    BYTE nDay;
    BYTE nHour;
    BYTE nMinute;
    BYTE bType;
    BYTE bFlags;
    
    
    unpackLEDWord(&databuf, &dwUin);
    unpackLEWord(&databuf, &wYear);
    unpackByte(&databuf, &nMonth);
    unpackByte(&databuf, &nDay);
    unpackByte(&databuf, &nHour);
    unpackByte(&databuf, &nMinute);
    unpackByte(&databuf, &bType);
    unpackByte(&databuf, &bFlags);
    unpackLEWord(&databuf, &wMsgLen);
    wPacketLen -=14;

    
    NetLog_Server("Offline message time: %u-%u-%u %u:%u", wYear, nMonth, nDay, nHour, nMinute);
    NetLog_Server("Offline message type %u from %u", bType, dwUin);

    _ASSERTE(wMsgLen == wPacketLen);
    if (wMsgLen == wPacketLen)
    {
      struct tm *sentTm;

      // Hack around the timezone problem
      // This is probably broken in some countries
      // but I'll leave it like this for now (todo)
      time(&dwTimestamp);
      sentTm = gmtime(&dwTimestamp);
      sentTm->tm_sec  = 28800;
      sentTm->tm_min  = nMinute;
      sentTm->tm_hour = nHour;
      sentTm->tm_mday = nDay;
      sentTm->tm_mon  = nMonth - 1;
      sentTm->tm_year = wYear - 1900;
      mktime(sentTm);

      // I *guess* server runs in US time-8 hours.
      // It might be UK time or something.
      // More observations reqd at changeover.
      if ((sentTm->tm_mon > 3 && sentTm->tm_mon < 9)
        || (sentTm->tm_mon == 3
          && (sentTm->tm_mday > 7
            || (sentTm->tm_wday != 0 && sentTm->tm_mday > sentTm->tm_wday)
            || (sentTm->tm_wday == 0 && sentTm->tm_hour >= 2)))
        || (sentTm->tm_mon == 9
          && (sentTm->tm_mday < 25
            || (sentTm->tm_wday != 0 && sentTm->tm_mday < 25 + sentTm->tm_wday)
            || (sentTm->tm_wday == 0 && sentTm->tm_hour <= 2))))
        sentTm->tm_hour--;

      sentTm->tm_sec -= 28800 + _timezone;
      {  // _daylight global variable is reversed in southern hemisphere. Silly.
        TIME_ZONE_INFORMATION tzinfo;
        if (GetTimeZoneInformation(&tzinfo) == TIME_ZONE_ID_DAYLIGHT)
          sentTm->tm_hour++;
      }
      dwTimestamp = mktime(sentTm);

      // :NOTE:
      // This is a check for the problem with offline messages being marked
      // with the wrong year by the server. It can cause other problems when
      // the internal system clock is incorrect but I think this is the most
      // generic fix.
      if (dwTimestamp > (unsigned long)time(NULL))
        dwTimestamp = time(NULL);

      { // Check if the time is not behind last user event, if yes, get current time
        HANDLE hContact = HContactFromUIN(dwUin, NULL);

        if (hContact)
        { // we have contact
          HANDLE hEvent = (HANDLE)CallService(MS_DB_EVENT_FINDLAST, (WPARAM)hContact, 0);
          
          if (hEvent)
          { // contact has events
            DBEVENTINFO dbei;
            DWORD dummy;
            
            dbei.cbSize = sizeof (DBEVENTINFO);
            dbei.pBlob = (char*)&dummy;
            dbei.cbBlob = 2;
            if (!CallService(MS_DB_EVENT_GET, (WPARAM)hEvent, (LPARAM)&dbei))
            { // got that event, if newer than ts then reset to current time
              if (dwTimestamp<dbei.timestamp) dwTimestamp = time(NULL);
            }
          }
        }
      }

      // Handle the actual message
      handleMessageTypes(dwUin, dwTimestamp, 0, 0, 0, bType, bFlags,
        0, wPacketLen, wMsgLen, databuf, FALSE);

      // Success
      return; 
    }
  }

  // Failure
  NetLog_Server("Error: Broken offline message");
}



static void parseEndOfOfflineMessages(unsigned char *databuf, WORD wPacketLen)
{
  BYTE bMissedMessages = 0;
  icq_packet packet;

  if (wPacketLen == 1)
  {
    unpackByte(&databuf, &bMissedMessages);
    NetLog_Server("End of offline msgs, %u dropped", bMissedMessages);
  }
  else
  {
    NetLog_Server("Error: Malformed end of offline msgs");
  }

  // Send 'got offline msgs'
  // This will delete the messages stored on server
  serverPacketInit(&packet, 24);
  packFNACHeader(&packet, ICQ_EXTENSIONS_FAMILY, CLI_META_REQ);
  packWord(&packet, 1);             // TLV Type
  packWord(&packet, 10);            // TLV Length
  packLEWord(&packet, 8);           // Data length
  packLEDWord(&packet, dwLocalUIN); // My UIN
  packLEWord(&packet, CLI_DELETE_OFFLINE_MSGS_REQ); // Ack offline msgs
  packLEWord(&packet, 0x0000);      // Request sequence number (we dont use this for now)

  // Send it
  sendServPacket(&packet);
}



static void handleExtensionMetaResponse(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wFlags)
{
  WORD wReplySubtype;
  BYTE bResultCode;
  
  _ASSERTE(wPacketLen >= 3);
  if (wPacketLen >= 3)
  {
    // Reply subtype
    unpackLEWord(&databuf, &wReplySubtype);
    wPacketLen -= 2;
    
    // Success byte
    unpackByte(&databuf, &bResultCode);
    wPacketLen -= 1;
    
    switch (wReplySubtype)
    {
    case META_SET_HOMEINFO_ACK:
    case META_SET_WORKINFO_ACK:
    case META_SET_MOREINFO_ACK:
    case META_SET_NOTES_ACK:
    case META_SET_EMAILINFO_ACK:
    case META_SET_INTINFO_ACK:
      //  case META_SET_AFFINFO_ACK:
    case META_SET_PERMS_ACK:
    case META_SET_PASSWORD_ACK:
    case META_SET_HPAGECAT_ACK:
    case META_SET_FULLINFO_ACK:
      parseUserInfoUpdateAck(databuf, wPacketLen, wCookie, wReplySubtype, bResultCode);
      break;
      
    case SRV_RANDOM_FOUND:
    case SRV_USER_FOUND:
    case SRV_LAST_USER_FOUND:
      parseSearchReplies(databuf, wPacketLen, wCookie, wReplySubtype, bResultCode);
      break;
      
    case META_SHORT_USERINFO:
    case META_BASIC_USERINFO:
    case META_WORK_USERINFO:
    case META_MORE_USERINFO:
    case META_NOTES_USERINFO:
    case META_EMAIL_USERINFO:
    case META_INTERESTS_USERINFO:
    case META_AFFILATIONS_USERINFO:
    case META_HPAGECAT_USERINFO:
      parseUserInfoRequestReplies(databuf, wPacketLen, wCookie, wFlags, wReplySubtype, bResultCode);
      break;
      
    case META_PROCESSING_ERROR:  // Meta processing error server reply
      // Todo: We only use this as an SMS ack, that will have to change
      {
        char *pszInfo;
        
        // Terminate buffer
        pszInfo = (char *)malloc(wPacketLen + 1);
        if (wPacketLen > 0)
          memcpy(pszInfo, databuf, wPacketLen);
        pszInfo[wPacketLen] = 0;
        
        ICQBroadcastAck(NULL, ICQACKTYPE_SMS, ACKRESULT_FAILED, (HANDLE)wCookie, (LPARAM)pszInfo);
        SAFE_FREE(&pszInfo);
        FreeCookie(wCookie);
        break;
      }
      break;
      
    case META_SMS_DELIVERY_RECEIPT:
      // Todo: This overlaps with META_SET_AFFINFO_ACK.
      // Todo: Check what happens if result != A
      if (wPacketLen > 8)
      {
        WORD wNetworkNameLen;
        WORD wAckLen;
        char *pszInfo;
        
        
        databuf += 6;    // Some unknowns
        wPacketLen -= 6;
        
        unpackWord(&databuf, &wNetworkNameLen);
        if (wPacketLen >= (wNetworkNameLen + 2))
        {
          databuf += wNetworkNameLen;
          wPacketLen -= wNetworkNameLen;
          
          unpackWord(&databuf, &wAckLen);
          if (pszInfo = (char *)malloc(wAckLen + 1))
          {
            // Terminate buffer
            if (wAckLen > 0)
              memcpy(pszInfo, databuf, wAckLen);
            pszInfo[wAckLen] = 0;
            
            ICQBroadcastAck(NULL, ICQACKTYPE_SMS, ACKRESULT_SENTREQUEST, (HANDLE)wCookie, (LPARAM)pszInfo);
            SAFE_FREE(&pszInfo);
            FreeCookie(wCookie);
            
            // Parsing success
            break; 
          }
        }
      }
      
      // Parsing failure
      NetLog_Server("Error: Failure parsing META_SMS_DELIVERY_RECEIPT");
      break;
      
    default:
      NetLog_Server("Warning: Ignored 15/03 replysubtype x%x", wReplySubtype);
//      _ASSERTE(0);
      break;
    }
    
    // Success
    return;
  }

  // Failure
  NetLog_Server("Warning: Broken 15/03 ExtensionMetaResponse");
}



static void parseSearchReplies(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wReplySubtype, BYTE bResultCode)
{
  BYTE bParsingOK = FALSE; // For debugging purposes only
  BOOL bLastUser = FALSE;
  search_cookie* pCookie;

  if (!FindCookie(wCookie, NULL, &pCookie))
  {
    NetLog_Server("Warning: Received unexpected search reply");
    pCookie = NULL;
  }

  switch (wReplySubtype)
  {
    
  case SRV_LAST_USER_FOUND: // Search: last user found reply
    bLastUser = TRUE;

  case SRV_USER_FOUND:      // Search: user found reply
    if (bLastUser)
      NetLog_Server("SNAC(0x15,0x3): Last search reply");
    else
      NetLog_Server("SNAC(0x15,0x3): Search reply");

    if (bResultCode == 0xA)
    {
      ICQSEARCHRESULT sr = {0};
      DWORD dwUin;
      WORD wLen;

      sr.hdr.cbSize = sizeof(sr);

      // Remaining bytes
      if (wPacketLen < 2)
        break;
      unpackLEWord(&databuf, &wLen);
      wPacketLen -= 2;
      
      _ASSERTE(wLen <= wPacketLen);
      if (wLen > wPacketLen)
        break;
      
      // Uin
      if (wPacketLen < 4)
        break;
      unpackLEDWord(&databuf, &dwUin); // Uin
      wPacketLen -= 4;
      sr.uin = dwUin;
      
      // Nick
      if (wPacketLen < 2)
        break;
      unpackLEWord(&databuf, &wLen);
      wPacketLen -= 2;
      if (wLen > 0)
      {
        if (wPacketLen < wLen || (databuf[wLen-1] != 0))
          break;
        sr.hdr.nick = databuf;
        databuf += wLen;
      }
      else
      {
        sr.hdr.nick = NULL;
      }

      // First name
      if (wPacketLen < 2)
        break;
      unpackLEWord(&databuf, &wLen);
      wPacketLen -= 2;
      if (wLen > 0)
      {
        if (wPacketLen < wLen || (databuf[wLen-1] != 0))
          break;
        sr.hdr.firstName = databuf;
        databuf += wLen;
      }
      else
      {
        sr.hdr.firstName = NULL;
      }

      // Last name
      if (wPacketLen < 2)
        break;
      unpackLEWord(&databuf, &wLen);
      wPacketLen -= 2;
      if (wLen > 0)
      {
        if (wPacketLen < wLen || (databuf[wLen-1] != 0))
          break;
        sr.hdr.lastName = databuf;
        databuf += wLen;
      }
      else
      {
        sr.hdr.lastName = NULL;
      }

      // E-mail name
      if (wPacketLen < 2)
        break;
      unpackLEWord(&databuf, &wLen);
      wPacketLen -= 2;
      if (wLen > 0)
      {
        if (wPacketLen < wLen || (databuf[wLen-1] != 0))
          break;
        sr.hdr.email = databuf;
        databuf += wLen;
      }
      else
      {
        sr.hdr.email = NULL;
      }

      // Authentication needed flag
      if (wPacketLen < 1)
        break;
      unpackByte(&databuf, &sr.auth);

      sr.uid = NULL; // icq contact
      // Finally, broadcast the result
      ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)wCookie, (LPARAM)&sr);
      
      // Broadcast "Last result" ack if this was the last user found
      if (wReplySubtype == SRV_LAST_USER_FOUND)
      {
        if (wPacketLen>=10)
        {
          DWORD dwLeft;

          databuf += 5;
          unpackLEDWord(&databuf, &dwLeft);
          if (dwLeft)
            NetLog_Server("Warning: %d search results omitted", dwLeft);
        }
        if (pCookie)
        {
          FreeCookie(wCookie);
          if (pCookie->dwMainId) // is paralel search present?
          {
            if (pCookie->dwStatus)
            { // paralel search already finished
              SAFE_FREE(&pCookie);
              ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
            } // state we finished, the rest will do paralel search
            else
              pCookie->dwStatus = 1;
          }
          else
          {
            SAFE_FREE(&pCookie);
            ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
          }

        }
        else
          ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
      }

      bParsingOK = TRUE;
    }
    else 
    {
      // Failed search
      NetLog_Server("SNAC(0x15,0x3): Search error %u", bResultCode);
      if (pCookie)
      {
        FreeCookie(wCookie);
        if (pCookie->dwMainId)
        {
          if (pCookie->dwStatus)
          {
            SAFE_FREE(&pCookie);
            ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
          }
          else
            pCookie->dwStatus = 1;
        }
        else
        {
          SAFE_FREE(&pCookie);
          ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
        }
      }
      else
        ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);

      bParsingOK = TRUE;
    }
    break;

  case SRV_RANDOM_FOUND: // Random search server reply
  default:
    if (pCookie)
    {
      FreeCookie(wCookie);
      SAFE_FREE(&pCookie);
    }
    break;

  }

  // For debugging purposes only
  if (!bParsingOK)
  {
    NetLog_Server("Warning: Parsing error in 15/03 search reply type x%x", wReplySubtype);
    _ASSERTE(!bParsingOK);
  }
}



static void parseUserInfoRequestReplies(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wFlags, WORD wReplySubtype, BYTE bResultCode)
{
  BOOL bMoreDataFollows;
  DWORD dwCookieUin;
  fam15_cookie_data* pCookieData = NULL;
  HANDLE hContact = INVALID_HANDLE_VALUE;
  int foundCookie;
  BOOL bOK = TRUE;
  

  foundCookie = FindCookie(wCookie, &dwCookieUin, (void**)&pCookieData);
  if (foundCookie && pCookieData)
  {
    if (pCookieData->bRequestType == REQUESTTYPE_OWNER)
      hContact = NULL; // this is here for situation when we have own uin in clist
    else
      hContact = HContactFromUIN(dwCookieUin, NULL);
  }
  else
  {
    NetLog_Server("Warning: Ignoring unrequested 15/03 user info reply type 0x%x", wReplySubtype);

    return;
  }

  if (bResultCode != 0x0A)
  {
    NetLog_Server("Warning: Got 15/03 user info failure reply type 0x%x", wReplySubtype);
  }

  // Check if this is the last packet for this request
  bMoreDataFollows = wFlags&0x0001;


  switch (wReplySubtype)
  {

  case META_BASIC_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "BASIC", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Nick", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "FirstName", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "LastName", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "e-mail", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "City", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "State", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Phone", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Fax", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Street", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Cellular", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "ZIP", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "Country", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByte(hContact, "Timezone", &databuf, &wPacketLen);
      
      if (hContact == NULL && bOK && (wPacketLen >= 3))
      {
        ICQWriteContactSettingByte(hContact, "Auth", (BYTE)!(*databuf));
        databuf += 1;

        ICQWriteContactSettingByte(hContact, "WebAware", (*databuf));
        databuf += 1;

        ICQWriteContactSettingByte(hContact, "PublishPrimaryEmail", (BYTE)!(*databuf));
        databuf += 1;

        wPacketLen -= 3;
      }
    }
    break;
    
  case META_WORK_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "WORK", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyCity", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyState", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyPhone", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyFax", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyStreet", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyZIP", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "CompanyCountry", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Company", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyDepartment",&databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyPosition", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "CompanyOccupation",&databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "CompanyHomepage", &databuf, &wPacketLen);
    }
    break;

  case META_MORE_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "MORE", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "Age", &databuf, &wPacketLen);
      if (bOK && (wPacketLen >= 1))
      {
        if (*databuf)
          ICQWriteContactSettingByte(hContact, "Gender", (BYTE)(*databuf == 1 ? 'F' : 'M'));
        else 
          // Undefined gender
          ICQDeleteContactSetting(hContact, "Gender");
        databuf += 1;
        wPacketLen -= 1;
      }
      else
        bOK = FALSE;
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Homepage", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "BirthYear", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByte(hContact, "BirthMonth", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByte(hContact, "BirthDay", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByteWithTable(hContact, "Language1", languageField, &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByteWithTable(hContact, "Language2", languageField, &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingByteWithTable(hContact, "Language3", languageField, &databuf, &wPacketLen);

      if (bOK && (wPacketLen >= 2))
      {
        databuf += 2;
        wPacketLen -= 2;
      }
      if (bOK) bOK = writeDbInfoSettingString(hContact, "OriginCity", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "OriginState", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingWord(hContact, "OriginCountry", &databuf, &wPacketLen);

      if (bOK && (wPacketLen >= 1))
      {
        BYTE bStatus = *databuf;
        ICQWriteContactSettingByte(hContact, "MaritalStatus", bStatus);
        databuf++;
        wPacketLen--;
      }
    }
    break;

  case META_NOTES_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "NOTES", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      if (bOK) bOK = writeDbInfoSettingString(hContact, "About", &databuf, &wPacketLen);
    }
    break;
    
  case META_EMAIL_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "EMAIL", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      int nCount = 0;
      char pszDatabaseKey[33];
      WORD wEmailLength;
      

      // This value used to be a e-mail counter. Either that was wrong or
      // Mirabilis changed the behaviour again. It usually says NULL now so
      // I use the packet byte count to extract the e-mails instead.
      databuf++;
      wPacketLen--;

      while (wPacketLen > 4)
      {

        // Don't publish flag
        databuf += 1;
        wPacketLen -= 1;

        // E-mail length
        unpackLEWord(&databuf, &wEmailLength);
        wPacketLen -= 2;

        // Check for buffer overflows
        if ((wEmailLength > wPacketLen) || (databuf[wEmailLength-1] != 0))
          break;

        // Rewind buffer pointer for writeDbInfoSettingString().
        databuf -= 2;
        wPacketLen += 2;

        if (wEmailLength > 1)
        { 
          null_snprintf(pszDatabaseKey, 33, "e-mail%d", nCount);
          if (bOK) bOK = writeDbInfoSettingString(hContact, pszDatabaseKey, &databuf, &wPacketLen);

          // Stop on parsing errors
          if (!bOK)
            break;

          // Increase counter
          nCount++;
        }
        else
        {
          databuf += wEmailLength;
          wPacketLen -= wEmailLength;
        }
      }

      // Delete the next key (this may not exist but that is OK)
      // :TODO:
      // We should probably continue to enumerate some keys here just in case
      // many e-mails were deleted. But it is not that important.
      if (bOK)
      {
        // We only delete e-mails when the parsing was successful since nCount
        // may be incorrect otherwise
        null_snprintf(pszDatabaseKey, 33, "e-mail%d", nCount);
        ICQDeleteContactSetting(hContact, pszDatabaseKey);
      }
    }
    break;

  case META_INTERESTS_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "INTERESTS", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      int i, count;
      char idstr[33];
      
      wPacketLen--;
      count = *databuf++;
      // 4 is the maximum allowed personal interests, if count is
      // higher it's likely a parsing error
      _ASSERTE(count <= 4);
      for (i = 0; i < 4; i++)
      {
        if (i < count)
        {
          null_snprintf(idstr, 33, "Interest%dCat", i);
          if (bOK) bOK = writeDbInfoSettingWordWithTable(hContact, idstr, interestsField, &databuf, &wPacketLen);

          null_snprintf(idstr, 33, "Interest%dText", i);
          if (bOK) bOK = writeDbInfoSettingString(hContact, idstr, &databuf, &wPacketLen);

          if (!bOK)
            break;
        }
        else
        {
          // Delete older entries if the count has decreased since last update
          null_snprintf(idstr, 33, "Interest%dCat", i);
          ICQDeleteContactSetting(hContact, idstr);

          null_snprintf(idstr, 33, "Interest%dText", i);
          ICQDeleteContactSetting(hContact, idstr);
        }
      }
    }
    break;
    
  case META_AFFILATIONS_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "AFFILATIONS", dwCookieUin);
    if (bResultCode == 0x0A)
    {
      int i;
      int count;
      char idstr[33];

      wPacketLen--;
      count = *databuf++;
      // 3 is the maximum allowed backgrounds, if count is
      // higher it's likely a parsing error
      _ASSERTE(count <= 3);
      for (i = 0; i < 3; i++)
      {
        if (i < count)
        {
          null_snprintf(idstr, 33, "Past%d", i);
          if (bOK) bOK = writeDbInfoSettingWordWithTable(hContact, idstr, pastField, &databuf, &wPacketLen);

          null_snprintf(idstr, 33, "Past%dText", i);
          if (bOK) bOK = writeDbInfoSettingString(hContact, idstr, &databuf, &wPacketLen);

          if (!bOK)
            break;
        }
        else
        {
          // Delete older entries if the count has decreased since last update
          null_snprintf(idstr, 33, "Past%d", i);
          ICQDeleteContactSetting(hContact, idstr);

          null_snprintf(idstr, 33, "Past%dText", i);
          ICQDeleteContactSetting(hContact, idstr);
        }
      }

      wPacketLen--;
      count = *databuf++;
      // 3 is the maximum allowed affiliations, if count is
      // higher it's likely a parsing error
      _ASSERTE(count <= 3);
      for (i = 0; i < 3; i++)
      {
        if (i < count)
        {
          null_snprintf(idstr, 33, "Affiliation%d", i);
          if (bOK) bOK = writeDbInfoSettingWordWithTable(hContact, idstr, affiliationField, &databuf, &wPacketLen);

          null_snprintf(idstr, 33, "Affiliation%dText", i);
          if (bOK) bOK = writeDbInfoSettingString(hContact, idstr, &databuf, &wPacketLen);

          if (!bOK)
            break;
        }
        else 
        {
          // Delete older entries if the count has decreased since last update
          null_snprintf(idstr, 33, "Affiliation%d", i);
          ICQDeleteContactSetting(hContact, idstr);
          
          null_snprintf(idstr, 33, "Affiliation%dText", i);
          ICQDeleteContactSetting(hContact, idstr);
        }
      }
      
    }
    break;

  // This is either a auto update reply or a GetInfo Minimal reply
  case META_SHORT_USERINFO: 
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "SHORT", dwCookieUin);
    if (bResultCode == 0xA)
    {
      if (bOK) bOK = writeDbInfoSettingString(hContact, "Nick", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "FirstName", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "LastName", &databuf, &wPacketLen);
      if (bOK) bOK = writeDbInfoSettingString(hContact, "e-mail", &databuf, &wPacketLen);
    }
    break;
    
  case META_HPAGECAT_USERINFO:
    NetLog_Server("SNAC(0x15,0x3): META_%s_USERINFO for %u", "HPAGECAT", dwCookieUin);
    break;

  default:
    NetLog_Server("Warning: Ignored 15/03 user info reply type x%x", wReplySubtype);
//    _ASSERTE(0);
    break;
  }

  if (!bOK)
  {
    NetLog_Server("Error: Failed parsing 15/03 user info reply type x%x", wReplySubtype);
  }

  // :TRICKY:  * Dont change the following section unless you really understand it *
  // I have now switched to only send one GETINFO ack instead of 8. The multiple ack
  // sending originated in a incorrect assumption in the old code and that is long
  // gone now. The ack will be sent when the last packet has arrived
  // or when an error has occured. I'm not sure if a error packet will be marked
  // as the last one but it probably is. Doesn't matter anyway.
  // The cookie will be freed for all "last packets" but the ack will only be sent if the
  // request originated from a PS_GETINFO call
  if (((pCookieData->bRequestType == REQUESTTYPE_USERDETAILED) ||
    (pCookieData->bRequestType == REQUESTTYPE_USERMINIMAL))
    &&
    ((bResultCode != 0x0A) || !bMoreDataFollows))
  {
    ICQBroadcastAck(hContact, ACKTYPE_GETINFO, ACKRESULT_SUCCESS, (HANDLE)1 ,0);
  }

  // Free cookie
  if (!bMoreDataFollows || bResultCode != 0x0A)
  {
    SAFE_FREE(&pCookieData);
    FreeCookie(wCookie);
  }

  // Remove user from info update queue. Removing is fast so we always call this
  // even if it is likely that the user is not queued at all.
  if (!bMoreDataFollows || bResultCode != 0x0A)
  {
    ICQWriteContactSettingDword(hContact, "InfoTS", time(NULL));
    icq_DequeueUser(dwCookieUin);
  }

  // :NOTE:
  // bResultcode can be xA (success), x14 or x32 (failure). I dont know the difference
  // between the two failures.
}



static void parseUserInfoUpdateAck(unsigned char *databuf, WORD wPacketLen, WORD wCookie, WORD wReplySubtype, BYTE bResultCode)
{
  switch (wReplySubtype)
  {

  case META_SET_HOMEINFO_ACK:  // Set user home info server ack
  case META_SET_WORKINFO_ACK:  // Set user work info server ack
  case META_SET_MOREINFO_ACK:  // Set user more info server ack
  case META_SET_NOTES_ACK:     // Set user notes info server ack
  case META_SET_EMAILINFO_ACK: // Set user email(s) info server ack
  case META_SET_INTINFO_ACK:   // Set user interests info server ack
  case META_SET_AFFINFO_ACK:   // Set user affilations info server ack
  case META_SET_PERMS_ACK:     // Set user permissions server ack
  case META_SET_PASSWORD_ACK:  // Set user password server ack
  case META_SET_HPAGECAT_ACK:  // Set user homepage category server ack
  case META_SET_FULLINFO_ACK:  // Server ack for set fullinfo command

    if (bResultCode == 0xA)
      ICQBroadcastAck(NULL, ACKTYPE_SETINFO, ACKRESULT_SUCCESS, (HANDLE)wCookie, 0);
    else
      ICQBroadcastAck(NULL, ACKTYPE_SETINFO, ACKRESULT_FAILED, (HANDLE)wCookie, 0);

    FreeCookie(wCookie);
    break;

  default:
    NetLog_Server("Warning: Ignored 15/03 user info update ack type x%x", wReplySubtype);
    break;
  }
}
