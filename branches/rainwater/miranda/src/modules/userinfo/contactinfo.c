/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2003 Miranda ICQ/IM project, 
all portions of this codebase are copyrighted to the people 
listed in contributors.txt.

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
#include "../../core/commonheaders.h"

static HFONT hEmailFont=NULL;
static HCURSOR hHandCursor=NULL;

static BOOL CALLBACK EditUserEmailDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
			if(*(char*)lParam) SetWindowText(hwndDlg,Translate("Edit E-Mail Address"));
			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg,IDC_EMAIL,(char*)lParam);
			EnableWindow(GetDlgItem(hwndDlg,IDOK),*(char*)lParam);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					GetDlgItemText(hwndDlg,IDC_EMAIL,(char*)GetWindowLong(hwndDlg,GWL_USERDATA),256);
					//fall through
				case IDCANCEL:
					EndDialog(hwndDlg,wParam);
				case IDC_EMAIL:
					if(HIWORD(wParam)==EN_CHANGE)
						EnableWindow(GetDlgItem(hwndDlg,IDOK),GetWindowTextLength(GetDlgItem(hwndDlg,IDC_EMAIL)));
					break;
			}
			break;
	}
	return FALSE;
}

static BOOL CALLBACK EditUserPhoneDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_INITDIALOG:
		{	char *szText=(char*)lParam;
			int i,item,countryCount;
			struct CountryListEntry *countries;
			SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
			if(szText[0]) SetWindowText(hwndDlg,"Edit Phone Number");
			TranslateDialogDefault(hwndDlg);
			if(lstrlen(szText)>4 && !lstrcmp(szText+lstrlen(szText)-4," SMS")) {
				CheckDlgButton(hwndDlg,IDC_SMS,BST_CHECKED);
				szText[lstrlen(szText)-4]='\0';
			}
			EnableWindow(GetDlgItem(hwndDlg,IDOK),szText[0]);
			SendDlgItemMessage(hwndDlg,IDC_AREA,EM_LIMITTEXT,31,0);
			SendDlgItemMessage(hwndDlg,IDC_NUMBER,EM_LIMITTEXT,63,0);
			CallService(MS_UTILS_GETCOUNTRYLIST,(WPARAM)&countryCount,(LPARAM)&countries);
			for(i=0;i<countryCount;i++) {
				if(countries[i].id==0 || countries[i].id==0xFFFF) continue;
				item=SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_ADDSTRING,0,(LPARAM)countries[i].szName);
				SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_SETITEMDATA,item,countries[i].id);
			}
			SetDlgItemText(hwndDlg,IDC_PHONE,szText);
			return TRUE;
		}
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				static int noRecursion=0;

				case IDOK:
					{	char *szText=(char*)GetWindowLong(hwndDlg,GWL_USERDATA);
						int isValid=1;
						GetDlgItemText(hwndDlg,IDC_PHONE,szText,252);
						if(lstrlen(szText)<7 || szText[0]!='+') isValid=0;
						if(isValid) isValid=(lstrlen(szText+1)==(int)strspn(szText+1,"0123456789 ()-"));
						if(!isValid) {
							MessageBox(hwndDlg,Translate("The phone number should start with a + and consist of numbers, spaces, brackets and hyphens only."),Translate("Invalid Phone Number"),MB_OK);
							break;
						}
						if(IsDlgButtonChecked(hwndDlg,IDC_SMS)) lstrcat(szText," SMS");
					}
					//fall through
				case IDCANCEL:
					EndDialog(hwndDlg,wParam);
				case IDC_COUNTRY:
					if(HIWORD(wParam)!=CBN_SELCHANGE) break;
				case IDC_AREA:
				case IDC_NUMBER:
					if(LOWORD(wParam)!=IDC_COUNTRY && HIWORD(wParam)!=EN_CHANGE) break;
					if(noRecursion) break;
					EnableWindow(GetDlgItem(hwndDlg,IDOK),TRUE);
					{	char szPhone[96],szArea[32],szNumber[64];
						GetDlgItemText(hwndDlg,IDC_AREA,szArea,sizeof(szArea));
						GetDlgItemText(hwndDlg,IDC_NUMBER,szNumber,sizeof(szNumber));
						_snprintf(szPhone,sizeof(szPhone),"+%u (%s) %s",SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_GETITEMDATA,SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_GETCURSEL,0,0),0),szArea,szNumber);
						noRecursion=1;
						SetDlgItemText(hwndDlg,IDC_PHONE,szPhone);
						noRecursion=0;
					}
					break;
				case IDC_PHONE:
					if(HIWORD(wParam)!=EN_UPDATE) break;
					if(noRecursion) break;
					noRecursion=1;
					{	char szText[256],*pText,*pArea,*pNumber;
						int isValid=1;
						GetDlgItemText(hwndDlg,IDC_PHONE,szText,sizeof(szText));
						if(szText[0]!='+') isValid=0;
						if(isValid) {
							int i,country=strtol(szText+1,&pText,10);
							if(pText-szText>4) isValid=0;
							else for(i=SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_GETCOUNT,0,0)-1;i>=0;i--)
									if(country==SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_GETITEMDATA,i,0))
										{SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_SETCURSEL,i,0); break;}
							if(i<0) isValid=0;
						}
						if(isValid) {
							pArea=pText+strcspn(pText,"0123456789");
							pText=pArea+strspn(pArea,"0123456789");
							if(*pText) {
								*pText='\0';
								pNumber=pText+1+strcspn(pText+1,"0123456789");
								SetDlgItemText(hwndDlg,IDC_NUMBER,pNumber);
							}
							SetDlgItemText(hwndDlg,IDC_AREA,pArea);
						}
						if(!isValid) {
							SendDlgItemMessage(hwndDlg,IDC_COUNTRY,CB_SETCURSEL,-1,0);
							SetDlgItemText(hwndDlg,IDC_AREA,"");
							SetDlgItemText(hwndDlg,IDC_NUMBER,"");
						}
					}
					noRecursion=0;
					EnableWindow(GetDlgItem(hwndDlg,IDOK),GetWindowTextLength(GetDlgItem(hwndDlg,IDC_PHONE)));
					break;
			}
			break;
	}
	return FALSE;
}

static int IsOverEmail(HWND hwndDlg,char *szEmail,int cchEmail)
{
	RECT rc;
	HWND hwndEmails;
	char szText[256];
	HDC hdc;
	SIZE textSize;
	LVHITTESTINFO hti;

	hwndEmails=GetDlgItem(hwndDlg,IDC_EMAILS);
	GetCursorPos(&hti.pt);
	ScreenToClient(hwndEmails,&hti.pt);
	GetClientRect(hwndEmails,&rc);
	if(!PtInRect(&rc,hti.pt)) return 0;
	if(ListView_SubItemHitTest(hwndEmails,&hti)==-1) return 0;
	if(hti.iSubItem!=1) return 0;
	if(!(hti.flags&LVHT_ONITEMLABEL)) return 0;
	ListView_GetSubItemRect(hwndEmails,hti.iItem,1,LVIR_LABEL,&rc);
	ListView_GetItemText(hwndEmails,hti.iItem,1,szText,sizeof(szText));
	hdc=GetDC(hwndEmails);
	SelectObject(hdc,hEmailFont);
	GetTextExtentPoint32(hdc,szText,lstrlen(szText),&textSize);
	ReleaseDC(hwndEmails,hdc);
	if(hti.pt.x<rc.left+textSize.cx) {
		if(szEmail && cchEmail) lstrcpyn(szEmail,szText,cchEmail);
		return 1;
	}
	return 0;
}

#define M_REMAKELISTS  (WM_USER+1)
BOOL CALLBACK ContactDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
			if(hEmailFont) DeleteObject(hEmailFont);
			{	LOGFONT lf;
				hEmailFont=(HFONT)SendDlgItemMessage(hwndDlg,IDC_EMAILS,WM_GETFONT,0,0);
				GetObject(hEmailFont,sizeof(lf),&lf);
				lf.lfUnderline=1;
				hEmailFont=CreateFontIndirect(&lf);
			}
			if(hHandCursor==NULL) {
				if(IsWinVer2000Plus()) hHandCursor=LoadCursor(NULL,IDC_HAND);
				else hHandCursor=LoadCursor(GetModuleHandle(NULL),MAKEINTRESOURCE(IDC_HYPERLINKHAND));
			}
			TranslateDialogDefault(hwndDlg);
			{	LVCOLUMN lvc;
				RECT rc;
				GetClientRect(GetDlgItem(hwndDlg,IDC_EMAILS),&rc);
				rc.right-=GetSystemMetrics(SM_CXVSCROLL);
				lvc.mask=LVCF_WIDTH;
				lvc.cx=rc.right/4;
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_EMAILS),0,&lvc);
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_PHONES),0,&lvc);
				lvc.cx=rc.right-rc.right/4-40;
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_EMAILS),1,&lvc);
				lvc.cx=rc.right-rc.right/4-90;
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_PHONES),1,&lvc);
				lvc.cx=50;
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_PHONES),2,&lvc);
				lvc.cx=20;
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_EMAILS),2,&lvc);
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_EMAILS),3,&lvc);
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_PHONES),3,&lvc);
				ListView_InsertColumn(GetDlgItem(hwndDlg,IDC_PHONES),4,&lvc);
			}
			return TRUE;
		case M_REMAKELISTS:
			{	char *szProto;
			LVITEM lvi;
			int i;
			char idstr[33];
			DBVARIANT dbv;
			HANDLE hContact=(HANDLE)GetWindowLong(hwndDlg,GWL_USERDATA);
			
			if (hContact != NULL) {
				szProto=(char*)CallService(MS_PROTO_GETCONTACTBASEPROTO,(WPARAM)hContact,0);
				if (szProto==NULL) break;
				//e-mails
				ListView_DeleteAllItems(GetDlgItem(hwndDlg,IDC_EMAILS));
				lvi.mask=LVIF_TEXT|LVIF_PARAM;
				lvi.lParam=(LPARAM)(-1);
				lvi.iSubItem=0;
				lvi.iItem=0;
				for(i=-1;;i++) {
					if(i==-1) {
						if(DBGetContactSetting(hContact,szProto,"e-mail",&dbv))
							continue;
						lvi.pszText=Translate("Primary");
					}
					else {
						wsprintf(idstr,"e-mail%d",i);
						if(DBGetContactSetting(hContact,szProto,idstr,&dbv))
							break;
						lvi.pszText=idstr;
						wsprintf(idstr,"%d",i+2);
					}
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_EMAILS),&lvi);
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_EMAILS),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				lvi.iSubItem=0;
				for(i=0;;i++) {
					lvi.lParam=i;
					wsprintf(idstr,"Mye-mail%d",i);
					if(DBGetContactSetting(hContact,"UserInfo",idstr,&dbv))
						break;
					lvi.pszText=idstr;
					wsprintf(idstr,Translate("Custom %d"),i+1);
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_EMAILS),&lvi);
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_EMAILS),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				lvi.mask=LVIF_PARAM;
				lvi.lParam=(LPARAM)(-2);
				ListView_InsertItem(GetDlgItem(hwndDlg,IDC_EMAILS),&lvi);
				//phones
				ListView_DeleteAllItems(GetDlgItem(hwndDlg,IDC_PHONES));
				lvi.mask=LVIF_TEXT|LVIF_PARAM;
				lvi.lParam=(LPARAM)(-1);
				lvi.iSubItem=0;
				lvi.iItem=0;
				if(!DBGetContactSetting(hContact,szProto,"Phone",&dbv)) {
					lvi.pszText=Translate("Primary");
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_PHONES),&lvi);
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				if(!DBGetContactSetting(hContact,szProto,"Fax",&dbv)) {
					lvi.pszText=Translate("Fax");
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_PHONES),&lvi);
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				if(!DBGetContactSetting(hContact,szProto,"Cellular",&dbv)) {
					lvi.pszText=Translate("Mobile");
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_PHONES),&lvi);
					if(lstrlen(dbv.pszVal)>4 && !lstrcmp(dbv.pszVal+lstrlen(dbv.pszVal)-4," SMS")) {
						ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,2,"y");
						dbv.pszVal[lstrlen(dbv.pszVal)-4]='\0';
					}
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				lvi.iSubItem=0;
				for(i=0;;i++) {
					lvi.lParam=i;
					wsprintf(idstr,"MyPhone%d",i);
					if(DBGetContactSetting(hContact,"UserInfo",idstr,&dbv))
						break;
					lvi.pszText=idstr;
					wsprintf(idstr,Translate("Custom %d"),i+1);
					ListView_InsertItem(GetDlgItem(hwndDlg,IDC_PHONES),&lvi);
					if(lstrlen(dbv.pszVal)>4 && !lstrcmp(dbv.pszVal+lstrlen(dbv.pszVal)-4," SMS")) {
						ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,2,"y");
						dbv.pszVal[lstrlen(dbv.pszVal)-4]='\0';
					}
					ListView_SetItemText(GetDlgItem(hwndDlg,IDC_PHONES),lvi.iItem,1,dbv.pszVal);
					DBFreeVariant(&dbv);
					lvi.iItem++;
				}
				lvi.mask=LVIF_PARAM;
				lvi.lParam=(LPARAM)(-2);
				ListView_InsertItem(GetDlgItem(hwndDlg,IDC_PHONES),&lvi);
			}
			break;
		}
		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->idFrom) {
				case 0:
					switch (((LPNMHDR)lParam)->code) {
						case PSN_INFOCHANGED:
							SendMessage(hwndDlg,M_REMAKELISTS,0,0);
							break;
					}
					break;
				case IDC_EMAILS:
				case IDC_PHONES:
					switch (((LPNMHDR)lParam)->code) {
						case NM_CUSTOMDRAW:
						{	NMLVCUSTOMDRAW *nm=(NMLVCUSTOMDRAW*)lParam;
							switch(nm->nmcd.dwDrawStage) {
								case CDDS_PREPAINT:
								case CDDS_ITEMPREPAINT:
									SetWindowLong(hwndDlg,DWL_MSGRESULT,CDRF_NOTIFYSUBITEMDRAW);
									return TRUE;
								case CDDS_SUBITEM|CDDS_ITEMPREPAINT:
								{
									RECT rc;
									HICON hIcon;
									ListView_GetSubItemRect(nm->nmcd.hdr.hwndFrom,nm->nmcd.dwItemSpec,nm->iSubItem,LVIR_LABEL,&rc);
									if(nm->iSubItem==1 && nm->nmcd.hdr.idFrom==IDC_EMAILS) {
										HFONT hoFont;
										char szText[256];
										ListView_GetItemText(nm->nmcd.hdr.hwndFrom,nm->nmcd.dwItemSpec,nm->iSubItem,szText,sizeof(szText));
										hoFont=(HFONT)SelectObject(nm->nmcd.hdc,hEmailFont);
										SetTextColor(nm->nmcd.hdc,RGB(0,0,255));
										DrawText(nm->nmcd.hdc,szText,-1,&rc,DT_END_ELLIPSIS|DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_TOP);
										SelectObject(nm->nmcd.hdc,hoFont);
										SetWindowLong(hwndDlg,DWL_MSGRESULT,CDRF_SKIPDEFAULT);
										return TRUE;
									}
									if(nm->nmcd.lItemlParam==(LPARAM)(-2) && nm->iSubItem-3==(nm->nmcd.hdr.idFrom==IDC_PHONES))
										hIcon=LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ADDCONTACT),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0);
									else if(nm->iSubItem>1 && nm->nmcd.lItemlParam!=(LPARAM)(-1) && nm->nmcd.lItemlParam!=(LPARAM)(-2)) {
										static int iconResources[3]={IDI_RENAME,IDI_DELETE};
										if(nm->iSubItem==2 && nm->nmcd.hdr.idFrom==IDC_PHONES) {
											char szText[2];
											ListView_GetItemText(nm->nmcd.hdr.hwndFrom,nm->nmcd.dwItemSpec,nm->iSubItem,szText,sizeof(szText));
											if(szText[0]) hIcon=LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_SMS),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0);
										}
										else hIcon=LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(iconResources[nm->iSubItem-3+(nm->nmcd.hdr.idFrom==IDC_EMAILS)]),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0);
									}
									else break;
									DrawIconEx(nm->nmcd.hdc,(rc.left+rc.right-GetSystemMetrics(SM_CXSMICON))/2,(rc.top+rc.bottom-GetSystemMetrics(SM_CYSMICON))/2,hIcon,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0,NULL,DI_NORMAL);
									DestroyIcon(hIcon);
									SetWindowLong(hwndDlg,DWL_MSGRESULT,CDRF_SKIPDEFAULT);
									return TRUE;
								}
							}
							break;
						}
						case NM_CLICK:
						{	NMLISTVIEW *nm=(NMLISTVIEW*)lParam;
							LVITEM lvi;
							char szEmail[256];
							HANDLE hContact=(HANDLE)GetWindowLong(hwndDlg,GWL_USERDATA);
							char *szIdTemplate=nm->hdr.idFrom==IDC_PHONES?"MyPhone%d":"Mye-mail%d";
							LVHITTESTINFO hti;

							if(IsOverEmail(hwndDlg,szEmail,sizeof(szEmail))) {
								char szExec[264];
								lstrcpy(szExec,"mailto:");
								lstrcat(szExec,szEmail);
								ShellExecute(hwndDlg,"open",szExec,NULL,NULL,SW_SHOW);
								break;
							}
							if(nm->iSubItem<2) break;
							hti.pt.x=(short)LOWORD(GetMessagePos());
							hti.pt.y=(short)HIWORD(GetMessagePos());
							ScreenToClient(nm->hdr.hwndFrom,&hti.pt);
							if(ListView_SubItemHitTest(nm->hdr.hwndFrom,&hti)==-1) break;
							lvi.mask=LVIF_PARAM;
							lvi.iItem=hti.iItem;
							lvi.iSubItem=0;
							ListView_GetItem(nm->hdr.hwndFrom,&lvi);
							if(lvi.lParam==(LPARAM)(-1)) break;
							if(lvi.lParam==(LPARAM)(-2)) {
								if(hti.iSubItem-3==(nm->hdr.idFrom==IDC_PHONES)) {
									//add
									char szNewData[256]="",idstr[33];
									int i;
									DBVARIANT dbv;
									if(IDOK!=DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(nm->hdr.idFrom==IDC_PHONES?IDD_ADDPHONE:IDD_ADDEMAIL),hwndDlg,nm->hdr.idFrom==IDC_PHONES?EditUserPhoneDlgProc:EditUserEmailDlgProc,(LPARAM)szNewData))
										break;
									for(i=0;;i++) {
										wsprintf(idstr,szIdTemplate,i);
										if(DBGetContactSetting(hContact,"UserInfo",idstr,&dbv)) break;
										DBFreeVariant(&dbv);
									}
									DBWriteContactSettingString(hContact,"UserInfo",idstr,szNewData);
									SendMessage(hwndDlg,M_REMAKELISTS,0,0);
								}
							}
							else {
								if(hti.iSubItem-3==(nm->hdr.idFrom==IDC_PHONES)) {
									//delete
									int i;
									char idstr[33];
									DBVARIANT dbv;
									for(i=lvi.lParam;;i++) {
										wsprintf(idstr,szIdTemplate,i+1);
										if(DBGetContactSetting(hContact,"UserInfo",idstr,&dbv)) break;
										wsprintf(idstr,szIdTemplate,i);
										DBWriteContactSettingString(hContact,"UserInfo",idstr,dbv.pszVal);
										DBFreeVariant(&dbv);
									}
									wsprintf(idstr,szIdTemplate,i);
									DBDeleteContactSetting(hContact,"UserInfo",idstr);
									SendMessage(hwndDlg,M_REMAKELISTS,0,0);
								}
								else if(hti.iSubItem-2==(nm->hdr.idFrom==IDC_PHONES)) {
									//edit
									char szText[256],idstr[33];
									DBVARIANT dbv;
									wsprintf(idstr,szIdTemplate,lvi.lParam);
									if(DBGetContactSetting(hContact,"UserInfo",idstr,&dbv)) break;
									lstrcpyn(szText,dbv.pszVal,sizeof(szText));
									DBFreeVariant(&dbv);
									if(IDOK!=DialogBoxParam(GetModuleHandle(NULL),MAKEINTRESOURCE(nm->hdr.idFrom==IDC_PHONES?IDD_ADDPHONE:IDD_ADDEMAIL),hwndDlg,nm->hdr.idFrom==IDC_PHONES?EditUserPhoneDlgProc:EditUserEmailDlgProc,(LPARAM)szText))
										break;
									DBWriteContactSettingString(hContact,"UserInfo",idstr,szText);
									SendMessage(hwndDlg,M_REMAKELISTS,0,0);
								}
							}
							break;
						}
					}
					break;
			}
			break;
		case WM_SETCURSOR:
			if(LOWORD(lParam)!=HTCLIENT) break;
			if(GetForegroundWindow()==GetParent(hwndDlg)) {
				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hwndDlg,&pt);
				SetFocus(ChildWindowFromPoint(hwndDlg,pt));	  //ugly hack because listviews ignore their first click
			}
			if(IsOverEmail(hwndDlg,NULL,0)) {
				SetCursor(hHandCursor);
				SetWindowLong(hwndDlg,DWL_MSGRESULT,TRUE);
				return TRUE;
			}
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDCANCEL:
					SendMessage(GetParent(hwndDlg),msg,wParam,lParam);
					break;
			}
			break;
	}
	return FALSE;
}
