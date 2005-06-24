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


static BOOL CALLBACK DlgProcIcqOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK DlgProcIcqContactsOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK DlgProcIcqFeaturesOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK DlgProcIcqPrivacyOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);


static const char* szLogLevelDescr[] = {"Display all problems", "Display problems causing possible loss of data", "Display explanations for disconnection", "Display problems requiring user intervention"};


int IcqOptInit(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp = {0};
	char* pszTreeItemName = NULL;
	int nNameLen = 0;


	odp.cbSize = sizeof(odp);
	odp.position = -800000000;
	odp.hInstance = hInst;

	// Add "icq" option
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_ICQ);
	odp.pszGroup = Translate("Network");
	odp.pszTitle = Translate(gpszICQProtoName);
	odp.pfnDlgProc = DlgProcIcqOpts;
	odp.flags = ODPF_BOLDGROUPS;
	odp.nIDBottomSimpleControl = IDC_STICQGROUP;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM)&odp);

  // Add "contacts" option
	nNameLen = strlen(Translate(gpszICQProtoName)) + 1 + strlen(Translate("Contacts"));
	pszTreeItemName = calloc(1, nNameLen+1);
	mir_snprintf(pszTreeItemName, nNameLen+1, "%s %s", Translate(gpszICQProtoName), Translate("Contacts"));
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_ICQCONTACTS);
    odp.pszTitle = pszTreeItemName;
	odp.pfnDlgProc = DlgProcIcqContactsOpts;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM)&odp);
	SAFE_FREE(&pszTreeItemName);
	
	// Add "features" option
	nNameLen = strlen(Translate(gpszICQProtoName)) + 1 + strlen(Translate("Features"));
	pszTreeItemName = calloc(1, nNameLen+1);
	mir_snprintf(pszTreeItemName, nNameLen+1, "%s %s", Translate(gpszICQProtoName), Translate("Features"));
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_ICQFEATURES);
    odp.pszTitle = pszTreeItemName;
	odp.pfnDlgProc = DlgProcIcqFeaturesOpts;
  odp.flags |= ODPF_EXPERTONLY;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM)&odp);
	SAFE_FREE(&pszTreeItemName);

	// Add "privacy" option
	nNameLen = strlen(Translate(gpszICQProtoName)) + 1 + strlen(Translate("Privacy"));
	pszTreeItemName = calloc(1, nNameLen+1);
	mir_snprintf(pszTreeItemName, nNameLen+1, "%s %s", Translate(gpszICQProtoName), Translate("Privacy"));
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPT_ICQPRIVACY);
	odp.pszTitle = pszTreeItemName;
	odp.pfnDlgProc = DlgProcIcqPrivacyOpts;
	odp.flags = ODPF_BOLDGROUPS;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM)&odp);
	SAFE_FREE(&pszTreeItemName);
	
	return 0;
}



static BOOL CALLBACK DlgProcIcqOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_INITDIALOG:
    {
      DBVARIANT dbv;

			TranslateDialogDefault(hwndDlg);

			SetDlgItemInt(hwndDlg, IDC_ICQNUM, DBGetContactSettingDword(NULL, gpszICQProtoName, UNIQUEIDSETTING, 0), FALSE);

			if (!DBGetContactSetting(NULL, gpszICQProtoName, "Password", &dbv))
			{
				//bit of a security hole here, since it's easy to extract a password from an edit box
				CallService(MS_DB_CRYPT_DECODESTRING, strlen(dbv.pszVal) + 1, (LPARAM)dbv.pszVal);
				SetDlgItemText(hwndDlg, IDC_PASSWORD, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			
			if (!DBGetContactSetting(NULL, gpszICQProtoName, "OscarServer", &dbv))
			{
				SetDlgItemText(hwndDlg, IDC_ICQSERVER, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			else
			{
				SetDlgItemText(hwndDlg, IDC_ICQSERVER, DEFAULT_SERVER_HOST);
			}
			
			SetDlgItemInt(hwndDlg, IDC_ICQPORT, DBGetContactSettingWord(NULL, gpszICQProtoName, "OscarPort", DEFAULT_SERVER_PORT), FALSE);
			CheckDlgButton(hwndDlg, IDC_KEEPALIVE, DBGetContactSettingByte(NULL, gpszICQProtoName, "KeepAlive", 0));
			CheckDlgButton(hwndDlg, IDC_USEGATEWAY, DBGetContactSettingByte(NULL, gpszICQProtoName, "UseGateway", 0));
			CheckDlgButton(hwndDlg, IDC_SLOWSEND, DBGetContactSettingByte(NULL, gpszICQProtoName, "SlowSend", 1));
			SendDlgItemMessage(hwndDlg, IDC_LOGLEVEL, TBM_SETRANGE, FALSE, MAKELONG(0, 3));
			SendDlgItemMessage(hwndDlg, IDC_LOGLEVEL, TBM_SETPOS, TRUE, 3-DBGetContactSettingByte(NULL, gpszICQProtoName, "ShowLogLevel", LOG_WARNING));
			SetDlgItemText(hwndDlg, IDC_LEVELDESCR, Translate(szLogLevelDescr[3-SendDlgItemMessage(hwndDlg, IDC_LOGLEVEL, TBM_GETPOS, 0, 0)]));
			ShowWindow(GetDlgItem(hwndDlg, IDC_RECONNECTREQD), SW_HIDE);
			
			return TRUE;
		}
		
	case WM_HSCROLL:
		SetDlgItemText(hwndDlg, IDC_LEVELDESCR, Translate(szLogLevelDescr[3-SendDlgItemMessage(hwndDlg, IDC_LOGLEVEL,TBM_GETPOS, 0, 0)]));
		SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
		break;
		
	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{

			case IDC_LOOKUPLINK:
				CallService(MS_UTILS_OPENURL, 1, (LPARAM)URL_FORGOT_PASSWORD);
				return TRUE;
				
			case IDC_NEWUINLINK:
				CallService(MS_UTILS_OPENURL, 1, (LPARAM)URL_REGISTER);
				return TRUE;
				
			case IDC_RESETSERVER:
				SetDlgItemText(hwndDlg, IDC_ICQSERVER, DEFAULT_SERVER_HOST);
				SetDlgItemInt(hwndDlg, IDC_ICQPORT, DEFAULT_SERVER_PORT, FALSE);
				SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
				return TRUE;
			}
			
			if (icqOnline && LOWORD(wParam) != IDC_SLOWSEND)
			{
				
				char szClass[80];
				
				
				GetClassName((HWND)lParam, szClass, sizeof(szClass));
				
				if (lstrcmpi(szClass, "EDIT") || HIWORD(wParam) == EN_CHANGE)
					ShowWindow(GetDlgItem(hwndDlg, IDC_RECONNECTREQD), SW_SHOW);
			}
			
			if ((LOWORD(wParam)==IDC_ICQNUM || LOWORD(wParam)==IDC_PASSWORD || LOWORD(wParam)==IDC_ICQSERVER || LOWORD(wParam)==IDC_ICQPORT) &&
				(HIWORD(wParam)!=EN_CHANGE || (HWND)lParam!=GetFocus()))
			{
				return 0;
			}
			
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}
		
	case WM_NOTIFY:
		{
			switch (((LPNMHDR)lParam)->code)
			{
				
			case PSN_APPLY:
				{
					char str[128];
					
					DBWriteContactSettingDword(NULL, gpszICQProtoName, UNIQUEIDSETTING, (DWORD)GetDlgItemInt(hwndDlg, IDC_ICQNUM, NULL, FALSE));
					GetDlgItemText(hwndDlg, IDC_PASSWORD, str, sizeof(str));
					CallService(MS_DB_CRYPT_ENCODESTRING, sizeof(str), (LPARAM)str);
					DBWriteContactSettingString(NULL, gpszICQProtoName, "Password", str);
					GetDlgItemText(hwndDlg,IDC_ICQSERVER, str, sizeof(str));
					DBWriteContactSettingString(NULL, gpszICQProtoName, "OscarServer", str);
					DBWriteContactSettingWord(NULL, gpszICQProtoName, "OscarPort", (WORD)GetDlgItemInt(hwndDlg, IDC_ICQPORT, NULL, FALSE));
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "KeepAlive", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_KEEPALIVE));
          DBWriteContactSettingByte(NULL, gpszICQProtoName, "UseGateway", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_USEGATEWAY));
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "SlowSend", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_SLOWSEND));
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "ShowLogLevel", (BYTE)(3-SendDlgItemMessage(hwndDlg, IDC_LOGLEVEL, TBM_GETPOS, 0, 0)));

					return TRUE;
				}
			}
		}
		break;
  }

  return FALSE;
}



static const UINT icqPrivacyControls[]={IDC_DCALLOW_ANY, IDC_DCALLOW_CLIST, IDC_DCALLOW_AUTH, IDC_ADD_ANY, IDC_ADD_AUTH, IDC_WEBAWARE, IDC_PUBLISHPRIMARY, IDC_STATIC_DC1, IDC_STATIC_DC2, IDC_STATIC_CLIST};
static BOOL CALLBACK DlgProcIcqPrivacyOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		
	case WM_INITDIALOG:
		{
			int nDcType;
			int nAddAuth;


			nDcType = DBGetContactSettingByte(NULL, gpszICQProtoName, "DCType", 0);
			nAddAuth = DBGetContactSettingByte(NULL, gpszICQProtoName, "Auth", 1);
			
			TranslateDialogDefault(hwndDlg);
			if (!icqOnline)
			{
				icq_EnableMultipleControls(hwndDlg, icqPrivacyControls, sizeof(icqPrivacyControls)/sizeof(icqPrivacyControls[0]), FALSE);
				ShowWindow(GetDlgItem(hwndDlg, IDC_STATIC_NOTONLINE), SW_SHOW);
			}
			else {
				ShowWindow(GetDlgItem(hwndDlg, IDC_STATIC_NOTONLINE), SW_HIDE);
			}
			CheckDlgButton(hwndDlg, IDC_DCALLOW_ANY, (nDcType == 0));
			CheckDlgButton(hwndDlg, IDC_DCALLOW_CLIST, (nDcType == 1));
			CheckDlgButton(hwndDlg, IDC_DCALLOW_AUTH, (nDcType == 2));
			CheckDlgButton(hwndDlg, IDC_ADD_ANY, (nAddAuth == 0));
			CheckDlgButton(hwndDlg, IDC_ADD_AUTH, (nAddAuth == 1));
			CheckDlgButton(hwndDlg, IDC_WEBAWARE, DBGetContactSettingByte(NULL, gpszICQProtoName, "WebAware", 0));
			CheckDlgButton(hwndDlg, IDC_PUBLISHPRIMARY, DBGetContactSettingByte(NULL, gpszICQProtoName, "PublishPrimaryEmail", 0));
			CheckDlgButton(hwndDlg, IDC_STATUSMSG_CLIST, DBGetContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyCList", 0));
			CheckDlgButton(hwndDlg, IDC_STATUSMSG_VISIBLE, DBGetContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyVisible", 0));
			if (!DBGetContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyCList", 0))
				EnableWindow(GetDlgItem(hwndDlg, IDC_STATUSMSG_VISIBLE), FALSE);
			return TRUE;
			break;
		}
		
	case WM_COMMAND:
		switch (LOWORD(wParam))	
    {
		case IDC_DCALLOW_ANY:
		case IDC_DCALLOW_CLIST:
		case IDC_DCALLOW_AUTH:
		case IDC_ADD_ANY:
		case IDC_ADD_AUTH:
		case IDC_WEBAWARE:
		case IDC_PUBLISHPRIMARY:
		case IDC_STATUSMSG_VISIBLE:
			if ((HWND)lParam != GetFocus())	return 0;
			break;
		case IDC_STATUSMSG_CLIST:
			if (IsDlgButtonChecked(hwndDlg, IDC_STATUSMSG_CLIST)) 
      {
				EnableWindow(GetDlgItem(hwndDlg, IDC_STATUSMSG_VISIBLE), TRUE);
				CheckDlgButton(hwndDlg, IDC_STATUSMSG_VISIBLE, DBGetContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyVisible", 0));
			}
			else 
      {
				EnableWindow(GetDlgItem(hwndDlg, IDC_STATUSMSG_VISIBLE), FALSE);
				CheckDlgButton(hwndDlg, IDC_STATUSMSG_VISIBLE, FALSE);
			}
			break;
		default:
			return 0;
			break;
		}
		SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
		break;
		
		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code) 
      {
			case PSN_APPLY:
				DBWriteContactSettingByte(NULL, gpszICQProtoName, "WebAware", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_WEBAWARE));
				DBWriteContactSettingByte(NULL, gpszICQProtoName, "PublishPrimaryEmail", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_PUBLISHPRIMARY));
				DBWriteContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyCList", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_STATUSMSG_CLIST));
				DBWriteContactSettingByte(NULL, gpszICQProtoName, "StatusMsgReplyVisible", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_STATUSMSG_VISIBLE));
				if (IsDlgButtonChecked(hwndDlg, IDC_DCALLOW_AUTH))
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "DCType", 2);
				else if (IsDlgButtonChecked(hwndDlg, IDC_DCALLOW_CLIST))
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "DCType", 1);
				else DBWriteContactSettingByte(NULL, gpszICQProtoName, "DCType", 0);
				if (IsDlgButtonChecked(hwndDlg, IDC_ADD_AUTH))
					DBWriteContactSettingByte(NULL, gpszICQProtoName, "Auth", 1);
				else DBWriteContactSettingByte(NULL, gpszICQProtoName, "Auth", 0);
				
				
				if (icqOnline)
				{
					PBYTE buf=NULL;
					int buflen=0;
					
					ppackLEWord(&buf, &buflen, 0);
					ppackLELNTS(&buf, &buflen, "Nick");
					ppackLELNTS(&buf, &buflen, "FirstName");
					ppackLELNTS(&buf, &buflen, "LastName");
					ppackLELNTS(&buf, &buflen, "e-mail");
					ppackLELNTS(&buf, &buflen, "City");
					ppackLELNTS(&buf, &buflen, "State");
					ppackLELNTS(&buf ,&buflen, "Phone");
					ppackLELNTS(&buf ,&buflen, "Fax");
					ppackLELNTS(&buf ,&buflen, "Street");
					ppackLELNTS(&buf ,&buflen, "Cellular");
					ppackLELNTS(&buf ,&buflen, "ZIP");
					ppackLEWord(&buf ,&buflen, (WORD)DBGetContactSettingWord(NULL, gpszICQProtoName, "Country", 0));
					ppackByte(&buf ,&buflen, (BYTE)DBGetContactSettingByte(NULL, gpszICQProtoName, "Timezone", 0));
					ppackByte(&buf ,&buflen, (BYTE)!DBGetContactSettingByte(NULL, gpszICQProtoName, "PublishPrimaryEmail", 0));
					*(PWORD)buf = buflen-2;
					{
//						char* pszServiceName = calloc(1, strlen(gpszICQProtoName)+strlen(PS_CHANGEINFO)+1);
//						strcpy(pszServiceName, gpszICQProtoName);
//						strcat(pszServiceName, PS_CHANGEINFO); // TODO: call directly
            IcqChangeInfo(ICQCHANGEINFO_MAIN, (LPARAM)buf);
//						CallService(pszServiceName, ICQCHANGEINFO_MAIN, (LPARAM)buf);
//						SAFE_FREE(&pszServiceName);
					}
					
					buflen=2;
					
					if (DBGetContactSettingByte(NULL, gpszICQProtoName, "Auth", 1) == 0)
						ppackByte(&buf, &buflen, (BYTE)1);
					else
						ppackByte(&buf, &buflen, (BYTE)0);
					ppackByte(&buf, &buflen, (BYTE)DBGetContactSettingByte(NULL, gpszICQProtoName, "WebAware", 0));
					ppackByte(&buf, &buflen, (BYTE)DBGetContactSettingByte(NULL, gpszICQProtoName, "DCType", 0));
					ppackByte(&buf, &buflen, 0); // User type?
					*(PWORD)buf = buflen-2;
					{
//						char* pszServiceName = calloc(1, strlen(gpszICQProtoName)+strlen(PS_CHANGEINFO)+1);
//						strcpy(pszServiceName, gpszICQProtoName);
//						strcat(pszServiceName, PS_CHANGEINFO);
            IcqChangeInfo(ICQCHANGEINFO_SECURITY, (LPARAM)buf);
//						CallService(pszServiceName, ICQCHANGEINFO_SECURITY, (LPARAM)buf);
//						SAFE_FREE(&pszServiceName);
					}
					SAFE_FREE(&buf);
					
					
					// Send a status packet to notify the server about the webaware setting
					{
						WORD wStatus;
						
						wStatus = MirandaStatusToIcq(gnCurrentStatus);
						
						if (gnCurrentStatus == ID_STATUS_INVISIBLE) 
            {
							// Tell who is on our visible list
							icq_sendEntireVisInvisList(0);
							if (gbSsiEnabled)
								updateServVisibilityCode(3);
							icq_setstatus(wStatus);
						}
						else
						{
							// Tell who is on our invisible list
							icq_sendEntireVisInvisList(1);
							icq_setstatus(wStatus);
							if (gbSsiEnabled)
								updateServVisibilityCode(4);
						}
					}
					
				}
				return TRUE;
			}
			break;
	}
	
	return FALSE;	
}

static HWND hCpCombo;

struct CPTABLE {
  UINT cpId;
  char *cpName;
};

struct CPTABLE cpTable[] = {
  {	874,	"Thai" },
  {	932,	"Japanese" },
  {	936,	"Simplified Chinese" },
  {	949,	"Korean" },
  {	950,	"Traditional Chinese" },
  {	1250,	"Central European" },
  {	1251,	"Cyrillic" },
  {	1252,	"Latin I" },
  {	1253,	"Greek" },
  {	1254,	"Turkish" },
  {	1255,	"Hebrew" },
  {	1256,	"Arabic" },
  {	1257,	"Baltic" },
  {	1258,	"Vietnamese" },
  {	1361,	"Korean (Johab)" },
  {   -1, NULL}
};

static BOOL CALLBACK FillCpCombo(LPCSTR str)
{
  int i;
  UINT cp;

  cp = atoi(str);
  for (i=0; cpTable[i].cpName != NULL && cpTable[i].cpId!=cp; i++);
  if (cpTable[i].cpName != NULL) 
  {
    LRESULT iIndex = SendMessageA(hCpCombo, CB_ADDSTRING, -1, (LPARAM) Translate(cpTable[i].cpName));
    SendMessage(hCpCombo, CB_SETITEMDATA, (WPARAM)iIndex, cpTable[i].cpId);
  }
  return TRUE;
}


static const UINT icqUnicodeControls[] = {IDC_UTFALL,IDC_UTFSTATIC,IDC_UTFCODEPAGE};
static const UINT icqDCMsgControls[] = {IDC_DCPASSIVE};
static BOOL CALLBACK DlgProcIcqFeaturesOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_INITDIALOG:
    {
      BYTE bData;
      int sCodePage;
      int i;

      TranslateDialogDefault(hwndDlg);
      bData = DBGetContactSettingByte(NULL, gpszICQProtoName, "UtfEnabled", DEFAULT_UTF_ENABLED);
      CheckDlgButton(hwndDlg, IDC_UTFENABLE, bData?TRUE:FALSE);
      CheckDlgButton(hwndDlg, IDC_UTFALL, bData==2?TRUE:FALSE);
      icq_EnableMultipleControls(hwndDlg, icqUnicodeControls, sizeof(icqUnicodeControls)/sizeof(icqUnicodeControls[0]), bData?TRUE:FALSE);
      CheckDlgButton(hwndDlg, IDC_TEMPVISIBLE, DBGetContactSettingByte(NULL,gpszICQProtoName,"TempVisListEnabled",DEFAULT_TEMPVIS_ENABLED));
      bData = DBGetContactSettingByte(NULL, gpszICQProtoName, "DirectMessaging", DEFAULT_DCMSG_ENABLED);
      CheckDlgButton(hwndDlg, IDC_DCENABLE, bData?TRUE:FALSE);
      CheckDlgButton(hwndDlg, IDC_DCPASSIVE, bData==1?TRUE:FALSE);
      icq_EnableMultipleControls(hwndDlg, icqDCMsgControls, sizeof(icqDCMsgControls)/sizeof(icqDCMsgControls[0]), bData?TRUE:FALSE);
      CheckDlgButton(hwndDlg, IDC_XSTATUSENABLE, DBGetContactSettingByte(NULL, gpszICQProtoName, "XStatusEnabled", DEFAULT_XSTATUS_ENABLED));
      CheckDlgButton(hwndDlg, IDC_AIMENABLE, DBGetContactSettingByte(NULL,gpszICQProtoName,"AimEnabled",DEFAULT_AIM_ENABLED));

      hCpCombo = GetDlgItem(hwndDlg, IDC_UTFCODEPAGE);
      sCodePage = DBGetContactSettingDword(NULL, gpszICQProtoName, "UtfCodepage", CP_ACP);
      EnumSystemCodePagesA(FillCpCombo, CP_INSTALLED);
      SendDlgItemMessageA(hwndDlg, IDC_UTFCODEPAGE, CB_INSERTSTRING, 0, (LPARAM)Translate("System default codepage"));
      if(sCodePage == 0)
        SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_SETCURSEL, (WPARAM)0, 0);
      else 
      {
        for(i = 0; i < SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_GETCOUNT, 0, 0); i++) 
        {
          if(SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_GETITEMDATA, (WPARAM)i, 0) == sCodePage)
            SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_SETCURSEL, (WPARAM)i, 0);
        }
      }

      return TRUE;
    }

  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case IDC_UTFENABLE:
      icq_EnableMultipleControls(hwndDlg, icqUnicodeControls, sizeof(icqUnicodeControls)/sizeof(icqUnicodeControls[0]), IsDlgButtonChecked(hwndDlg, IDC_UTFENABLE));
      break;
    case IDC_DCENABLE:
      icq_EnableMultipleControls(hwndDlg, icqDCMsgControls, sizeof(icqDCMsgControls)/sizeof(icqDCMsgControls[0]), IsDlgButtonChecked(hwndDlg, IDC_DCENABLE));
      break;
    }
    SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
    break;

  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code)
    {
    case PSN_APPLY:
      if (IsDlgButtonChecked(hwndDlg, IDC_UTFENABLE))
        gbUtfEnabled = IsDlgButtonChecked(hwndDlg, IDC_UTFALL)?2:1;
      else
        gbUtfEnabled = 0;
      {
        int i = SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_GETCURSEL, 0, 0);
        gbUtfCodepage = SendDlgItemMessage(hwndDlg, IDC_UTFCODEPAGE, CB_GETITEMDATA, (WPARAM)i, 0);
        DBWriteContactSettingDword(NULL, gpszICQProtoName, "UtfCodepage", gbUtfCodepage);
      }
      DBWriteContactSettingByte(NULL, gpszICQProtoName, "UtfEnabled", gbUtfEnabled);
      gbTempVisListEnabled = (BYTE)IsDlgButtonChecked(hwndDlg, IDC_TEMPVISIBLE);
      DBWriteContactSettingByte(NULL, gpszICQProtoName, "TempVisListEnabled", gbTempVisListEnabled);
      if (IsDlgButtonChecked(hwndDlg, IDC_DCENABLE))
        gbDCMsgEnabled = IsDlgButtonChecked(hwndDlg, IDC_DCPASSIVE)?1:2;
      else
        gbDCMsgEnabled = 0;
      DBWriteContactSettingByte(NULL, gpszICQProtoName, "DirectMessaging", gbDCMsgEnabled);
      gbXStatusEnabled = (BYTE)IsDlgButtonChecked(hwndDlg, IDC_XSTATUSENABLE);
      DBWriteContactSettingByte(NULL, gpszICQProtoName, "XStatusEnabled", gbXStatusEnabled);
//      DBWriteContactSettingByte(NULL, gpszICQProtoName, "AimEnabled", (BYTE)IsDlgButtonChecked(hwndDlg, IDC_AIMENABLE));
      return TRUE;
    }
    break;
  }
  return FALSE;
}



static const UINT icqContactsControls[] = {IDC_ADDSERVER,IDC_LOADFROMSERVER,IDC_SAVETOSERVER,IDC_UPLOADNOW};
static const UINT icqAvatarControls[] = {IDC_AUTOLOADAVATARS,IDC_LINKAVATARS};
static BOOL CALLBACK DlgProcIcqContactsOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_INITDIALOG:
    TranslateDialogDefault(hwndDlg);
    CheckDlgButton(hwndDlg, IDC_ENABLE, DBGetContactSettingByte(NULL,gpszICQProtoName,"UseServerCList",DEFAULT_SS_ENABLED));
    CheckDlgButton(hwndDlg, IDC_ADDSERVER, DBGetContactSettingByte(NULL,gpszICQProtoName,"ServerAddRemove",DEFAULT_SS_ADDSERVER));
    CheckDlgButton(hwndDlg, IDC_LOADFROMSERVER, DBGetContactSettingByte(NULL,gpszICQProtoName,"LoadServerDetails",DEFAULT_SS_LOAD));
    CheckDlgButton(hwndDlg, IDC_SAVETOSERVER, DBGetContactSettingByte(NULL,gpszICQProtoName,"StoreServerDetails",DEFAULT_SS_STORE));
    CheckDlgButton(hwndDlg, IDC_ENABLEAVATARS, DBGetContactSettingByte(NULL,gpszICQProtoName,"AvatarsEnabled",DEFAULT_AVATARS_ENABLED));
    CheckDlgButton(hwndDlg, IDC_AUTOLOADAVATARS, DBGetContactSettingByte(NULL,gpszICQProtoName,"AvatarsAutoLoad",DEFAULT_LOAD_AVATARS));
    CheckDlgButton(hwndDlg, IDC_LINKAVATARS, DBGetContactSettingByte(NULL,gpszICQProtoName,"AvatarsAutoLink",DEFAULT_LINK_AVATARS));

    icq_EnableMultipleControls(hwndDlg, icqContactsControls, sizeof(icqContactsControls)/sizeof(icqContactsControls[0]), 
      DBGetContactSettingByte(NULL, gpszICQProtoName, "UseServerCList", DEFAULT_SS_ENABLED)?TRUE:FALSE);
    icq_EnableMultipleControls(hwndDlg, icqAvatarControls, sizeof(icqAvatarControls)/sizeof(icqAvatarControls[0]), 
      DBGetContactSettingByte(NULL, gpszICQProtoName, "AvatarsEnabled", DEFAULT_AVATARS_ENABLED)?TRUE:FALSE);

    if (icqOnline)
    {
      ShowWindow(GetDlgItem(hwndDlg, IDC_OFFLINETOENABLE), SW_SHOW);
      EnableWindow(GetDlgItem(hwndDlg, IDC_ENABLE), FALSE);
      EnableWindow(GetDlgItem(hwndDlg, IDC_ENABLEAVATARS), FALSE);
    }
    else
    {
      EnableWindow(GetDlgItem(hwndDlg, IDC_UPLOADNOW), FALSE);
    }
    return TRUE;

  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case IDC_UPLOADNOW:
      ShowUploadContactsDialog();
      return TRUE;
    case IDC_ENABLE:
      icq_EnableMultipleControls(hwndDlg, icqContactsControls, sizeof(icqContactsControls)/sizeof(icqContactsControls[0]), IsDlgButtonChecked(hwndDlg, IDC_ENABLE));
      if (icqOnline) 
        ShowWindow(GetDlgItem(hwndDlg, IDC_RECONNECTREQD), SW_SHOW);
      else 
        EnableWindow(GetDlgItem(hwndDlg, IDC_UPLOADNOW), FALSE);
      break;
    case IDC_ENABLEAVATARS:
      icq_EnableMultipleControls(hwndDlg, icqAvatarControls, sizeof(icqAvatarControls)/sizeof(icqAvatarControls[0]), IsDlgButtonChecked(hwndDlg, IDC_ENABLEAVATARS));
      break;
    }
    SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
    break;

  case WM_NOTIFY:
    switch (((LPNMHDR)lParam)->code)
    {
    case PSN_APPLY:
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"UseServerCList",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_ENABLE));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"ServerAddRemove",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_ADDSERVER));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"LoadServerDetails",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_LOADFROMSERVER));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"StoreServerDetails",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_SAVETOSERVER));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"AvatarsEnabled",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_ENABLEAVATARS));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"AvatarsAutoLoad",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_AUTOLOADAVATARS));
      DBWriteContactSettingByte(NULL,gpszICQProtoName,"AvatarsAutoLink",(BYTE)IsDlgButtonChecked(hwndDlg,IDC_LINKAVATARS));

      return TRUE;
    }
    break;
  }
  return FALSE;
}
