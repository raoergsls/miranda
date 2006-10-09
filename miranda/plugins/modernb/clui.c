/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2006 Miranda ICQ/IM project, 
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

#include "commonheaders.h"

#include "m_clc.h"
#include "m_clui.h"
#include "m_skin.h"
#include "genmenu.h"
#include "wingdi.h"
#include <Winuser.h>
#include "skinengine.h"
#include "modern_statusbar.h"

#include "clui.h"
#include <locale.h>

/*
*  Function CLUI_CheckOwnedByClui returns true if given window is in 
*  frames.
*/
BOOL CLUI_CheckOwnedByClui(HWND hWnd)
{
    HWND hWndMid, hWndClui;
    if (!hWnd) return FALSE;
    hWndClui=(HWND)CallService(MS_CLUI_GETHWND,0,0);
    hWndMid=GetAncestor(hWnd,GA_ROOTOWNER);
    if(hWndMid==hWndClui) return TRUE;
    {
        char buf[255];
        GetClassNameA(hWndMid,buf,254);
        if (!_strcmpi(buf,CLUIFrameSubContainerClassName)) return TRUE;
    }
    return FALSE;
}

/*
*  Function CLUI_ShowWindowMod overloads API ShowWindow in case of
*  dropShaddow is enabled: it force to minimize window before hiding
*  to remove shadow.
*/
int CLUI_ShowWindowMod(HWND hWnd, int nCmd) 
{
    if (hWnd==pcli->hwndContactList 
        && !g_mutex_bChangingMode
        && nCmd==SW_HIDE 
        && !g_bLayered 
        && IsWinVerXPPlus()
        && DBGetContactSettingByte(NULL,"CList","WindowShadow",0))
    {
        ShowWindow(hWnd,SW_MINIMIZE); //removing of shadow
        return ShowWindow(hWnd,nCmd);
    }
    if (hWnd==pcli->hwndContactList
        && !g_mutex_bChangingMode
        && nCmd==SW_RESTORE
        && !g_bLayered 
        && IsWinVerXPPlus()		
        && DBGetContactSettingByte(NULL,"CList","WindowShadow",0)
        && g_bSmoothAnimation
        && !g_bTransparentFlag 
        )
    {
        CLUI_SmoothAlphaTransition(hWnd, 255, 1);
    }
    return ShowWindow(hWnd,nCmd);
}

static BOOL CLUI_WaitThreadsCompletion(HWND hwnd)
{
    static BYTE bEntersCount=0;
    static const BYTE bcMAX_AWAITING_RETRY = 10; //repeat awaiting only 10 times
    TRACE("CLUI_WaitThreadsCompletion Enter");
    if (bEntersCount<bcMAX_AWAITING_RETRY
        &&( g_mutex_nCalcRowHeightLock ||
          g_mutex_nPaintLock || 
          g_hAskAwayMsgThreadID || 
          g_hGetTextThreadID || 
          g_hSmoothAnimationThreadID || 
          g_hFillFontListThreadID) 
         &&!Miranda_Terminated())
    {
        TRACE("Waiting threads");
        TRACEVAR("g_mutex_nCalcRowHeightLock: %x",g_mutex_nCalcRowHeightLock);
        TRACEVAR("g_mutex_nPaintLock: %x",g_mutex_nPaintLock);
        TRACEVAR("g_hAskAwayMsgThreadID: %x",g_hAskAwayMsgThreadID);
        TRACEVAR("g_hGetTextThreadID: %x",g_hGetTextThreadID);
        TRACEVAR("g_hSmoothAnimationThreadID: %x",g_hSmoothAnimationThreadID);
        TRACEVAR("g_hFillFontListThreadID: %x",g_hFillFontListThreadID);
        bEntersCount++;
        PostMessage(hwnd,WM_DESTROY,0,0);
        SleepEx(10,TRUE);
        return TRUE;
    }

    if (bEntersCount==bcMAX_AWAITING_RETRY)
    {   //force to terminate threads after max times repeating of awaiting
        if (g_hAskAwayMsgThreadID)      TerminateThread((HANDLE)g_hAskAwayMsgThreadID,0);
        if (g_hGetTextThreadID)         TerminateThread((HANDLE)g_hGetTextThreadID,0);
        if (g_hSmoothAnimationThreadID) TerminateThread((HANDLE)g_hSmoothAnimationThreadID,0);
        if (g_hFillFontListThreadID)    TerminateThread((HANDLE)g_hFillFontListThreadID,0);
    }
    return FALSE;
}


void CLUI_ChangeWindowMode()
{
    BOOL storedVisMode=FALSE;
    LONG style,styleEx;
    LONG oldStyle,oldStyleEx;
    LONG styleMask=WS_CLIPCHILDREN|WS_BORDER|WS_CAPTION|WS_MINIMIZEBOX|WS_POPUPWINDOW|WS_CLIPCHILDREN|WS_THICKFRAME|WS_SYSMENU;
    LONG styleMaskEx=WS_EX_TOOLWINDOW|WS_EX_LAYERED;
    LONG curStyle,curStyleEx;
    if (!pcli->hwndContactList) return;

    g_mutex_bChangingMode=TRUE;
    g_bTransparentFlag=IsWinVer2000Plus()&&DBGetContactSettingByte( NULL,"CList","Transparent",SETTING_TRANSPARENT_DEFAULT);
    g_bSmoothAnimation=IsWinVer2000Plus()&&DBGetContactSettingByte(NULL, "CLUI", "FadeInOut", 1);
    if (g_bTransparentFlag==0 && g_bCurrentAlpha!=0)
        g_bCurrentAlpha=255;
    //2- Calculate STYLES and STYLESEX
    if (!g_bLayered)
    {
        style=0;
        styleEx=0;
        if (DBGetContactSettingByte(NULL,"CList","ThinBorder",0) || (DBGetContactSettingByte(NULL,"CList","NoBorder",0)))
        {
            style=WS_CLIPCHILDREN| (DBGetContactSettingByte(NULL,"CList","ThinBorder",0)?WS_BORDER:0);
            styleEx=WS_EX_TOOLWINDOW;
        } 
        else if (DBGetContactSettingByte(NULL,"CLUI","ShowCaption",SETTING_SHOWCAPTION_DEFAULT) && DBGetContactSettingByte(NULL,"CList","ToolWindow",SETTING_TOOLWINDOW_DEFAULT))
        {
            styleEx=WS_EX_TOOLWINDOW/*|WS_EX_WINDOWEDGE*/;
            style=WS_CAPTION|WS_MINIMIZEBOX|WS_POPUPWINDOW|WS_CLIPCHILDREN|WS_THICKFRAME;
        }
        else if (DBGetContactSettingByte(NULL,"CLUI","ShowCaption",SETTING_SHOWCAPTION_DEFAULT))
        {
            style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_POPUPWINDOW|WS_CLIPCHILDREN|WS_THICKFRAME;
        }
        else 
        {
            style=WS_POPUPWINDOW|WS_CLIPCHILDREN|WS_THICKFRAME;
            styleEx=WS_EX_TOOLWINDOW/*|WS_EX_WINDOWEDGE*/;
        }
    }
    else 
    {
        style=WS_CLIPCHILDREN;
        styleEx=WS_EX_TOOLWINDOW;
    }
    //3- TODO Update Layered mode
    if(g_bTransparentFlag&&g_bLayered)
        styleEx|=WS_EX_LAYERED;

    //4- Set Title
    {
        TCHAR titleText[255]={0};
        DBVARIANT dbv={0};
        if(DBGetContactSettingTString(NULL,"CList","TitleText",&dbv))
            lstrcpyn(titleText,TEXT(MIRANDANAME),sizeof(titleText));
        else 
        {
            lstrcpyn(titleText,dbv.ptszVal,sizeof(titleText));
            DBFreeVariant(&dbv);
        }
        SetWindowText(pcli->hwndContactList,titleText);
    }
    //<->
    //1- If visible store it and hide

    if (g_bLayered && (DBGetContactSettingByte(NULL,"CList","OnDesktop", 0)))// && !flag_bFirstTimeCall))
    {
        SetParent(pcli->hwndContactList,NULL);
        CLUIFrames_SetParentForContainers(NULL);
        UpdateWindow(pcli->hwndContactList);
        g_bOnDesktop=0;
    }
    //5- TODO Apply Style
    oldStyleEx=curStyleEx=GetWindowLong(pcli->hwndContactList,GWL_EXSTYLE);
    oldStyle=curStyle=GetWindowLong(pcli->hwndContactList,GWL_STYLE);	

    curStyleEx=(curStyleEx&(~styleMaskEx))|styleEx;
    curStyle=(curStyle&(~styleMask))|style;
    if (oldStyleEx!=curStyleEx || oldStyle!=curStyle)
    {
        if (IsWindowVisible(pcli->hwndContactList)) 
        {
            storedVisMode=TRUE;
            mutex_bShowHideCalledFromAnimation=TRUE;
            ShowWindow(pcli->hwndContactList,SW_HIDE);
            CLUIFrames_OnShowHide(pcli->hwndContactList,0);
        }
        SetWindowLong(pcli->hwndContactList,GWL_EXSTYLE,curStyleEx);
        SetWindowLong(pcli->hwndContactList,GWL_STYLE,curStyle);
    }

    if(g_bLayered || !DBGetContactSettingByte(NULL,"CLUI","ShowMainMenu",SETTING_SHOWMAINMENU_DEFAULT)) 
        SetMenu(pcli->hwndContactList,NULL);
    else
        SetMenu(pcli->hwndContactList,g_hMenuMain);

    if (g_bLayered&&(DBGetContactSettingByte(NULL,"CList","OnDesktop", 0)))
        SkinEngine_UpdateWindowImage();
    //6- Pin to desktop mode
    if (DBGetContactSettingByte(NULL,"CList","OnDesktop", 0)) 
    {
        HWND hProgMan=FindWindow(TEXT("Progman"),NULL);
        if (IsWindow(hProgMan)) 
        {
            SetParent(pcli->hwndContactList,hProgMan);
            CLUIFrames_SetParentForContainers(hProgMan);
            g_bOnDesktop=1;
        }
    } 
    else 
    {
        //	HWND parent=GetParent(pcli->hwndContactList);
        //	HWND progman=FindWindow(TEXT("Progman"),NULL);
        //	if (parent==progman)
        {
            SetParent(pcli->hwndContactList,NULL);
            CLUIFrames_SetParentForContainers(NULL);
        }
        g_bOnDesktop=0;
    }

    //7- if it was visible - show
    if (storedVisMode) 
    {
        ShowWindow(pcli->hwndContactList,SW_SHOW);
        CLUIFrames_OnShowHide(pcli->hwndContactList,1);
    }
    mutex_bShowHideCalledFromAnimation=FALSE;
    if (!g_bLayered)
    {
        HRGN hRgn1;
        RECT r;
        int v,h;
        int w=10;
        GetWindowRect(pcli->hwndContactList,&r);
        h=(r.right-r.left)>(w*2)?w:(r.right-r.left);
        v=(r.bottom-r.top)>(w*2)?w:(r.bottom-r.top);
        h=(h<v)?h:v;
        hRgn1=CreateRoundRectRgn(0,0,(r.right-r.left+1),(r.bottom-r.top+1),h,h);
        if ((DBGetContactSettingByte(NULL,"CLC","RoundCorners",0)) && (!CallService(MS_CLIST_DOCKINGISDOCKED,0,0)))
            SetWindowRgn(pcli->hwndContactList,hRgn1,1); 
        else 
        {
            DeleteObject(hRgn1);
            SetWindowRgn(pcli->hwndContactList,NULL,1);
        }

        RedrawWindow(pcli->hwndContactList,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_UPDATENOW|RDW_ALLCHILDREN);   
    } 			
    g_mutex_bChangingMode=FALSE;
    flag_bFirstTimeCall=TRUE;
}

int CLUI_UpdateTimer(BYTE BringIn)
{  
    if (g_nBehindEdgeState==0)
    {
        KillTimer(pcli->hwndContactList,TM_BRINGOUTTIMEOUT);
        SetTimer(pcli->hwndContactList,TM_BRINGOUTTIMEOUT,wBehindEdgeHideDelay*100,NULL);
    }
    if (bShowEventStarted==0 && g_nBehindEdgeState>0 ) 
    {
        KillTimer(pcli->hwndContactList,TM_BRINGINTIMEOUT);
        SetTimer(pcli->hwndContactList,TM_BRINGINTIMEOUT,wBehindEdgeShowDelay*100,NULL);
        bShowEventStarted=1;
    }
    return 0;
}

int CLUI_HideBehindEdge()
{
    int method=g_nBehindEdgeSettings;
    if (method)
    {
        // if (DBGetContactSettingByte(NULL, "ModernData", "BehindEdge", 0)==0)
        {
            RECT rcScreen;
            RECT rcWindow;
            int bordersize=0;
            //Need to be moved out of screen
            bShowEventStarted=0;
            //1. get work area rectangle 
            Docking_GetMonitorRectFromWindow(pcli->hwndContactList,&rcScreen);
            //SystemParametersInfo(SPI_GETWORKAREA,0,&rcScreen,FALSE);
            //2. move out
            bordersize=wBehindEdgeBorderSize;
            GetWindowRect(pcli->hwndContactList,&rcWindow);
            switch (method)
            {
            case 1: //left
                rcWindow.left=rcScreen.left-(rcWindow.right-rcWindow.left)+bordersize;
                break;
            case 2: //right
                rcWindow.left=rcScreen.right-bordersize;
                break;
            }
            g_mutex_uPreventDockMoving=0;
            SetWindowPos(pcli->hwndContactList,NULL,rcWindow.left,rcWindow.top,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
            CLUIFrames_OnMoving(pcli->hwndContactList,&rcWindow);
            g_mutex_uPreventDockMoving=1;

            //3. store setting
            DBWriteContactSettingByte(NULL, "ModernData", "BehindEdge",method);
            g_nBehindEdgeState=method;
            return 1;
        }
        return 2;
    }
    return 0;
}


int CLUI_ShowFromBehindEdge()
{
    int method=g_nBehindEdgeSettings;
    bShowEventStarted=0;
    if (g_mutex_bOnTrayRightClick) 
    {
        g_mutex_bOnTrayRightClick=0;
        return 0;
    }
    if (method)// && (DBGetContactSettingByte(NULL, "ModernData", "BehindEdge", 0)==0))
    {
        RECT rcScreen;
        RECT rcWindow;
        int bordersize=0;
        //Need to be moved out of screen

        //1. get work area rectangle 
        //SystemParametersInfo(SPI_GETWORKAREA,0,&rcScreen,FALSE);
        Docking_GetMonitorRectFromWindow(pcli->hwndContactList,&rcScreen);
        //2. move out
        bordersize=wBehindEdgeBorderSize;
        GetWindowRect(pcli->hwndContactList,&rcWindow);
        switch (method)
        {
        case 1: //left
            rcWindow.left=rcScreen.left;
            break;
        case 2: //right
            rcWindow.left=rcScreen.right-(rcWindow.right-rcWindow.left);
            break;
        }
        g_mutex_uPreventDockMoving=0;
        SetWindowPos(pcli->hwndContactList,NULL,rcWindow.left,rcWindow.top,0,0,SWP_NOZORDER|SWP_NOSIZE);
        CLUIFrames_OnMoving(pcli->hwndContactList,&rcWindow);
        g_mutex_uPreventDockMoving=1;

        //3. store setting
        DBWriteContactSettingByte(NULL, "ModernData", "BehindEdge",0);
        g_nBehindEdgeState=0;
    }
    return 0;
}

static int CLUI_FillAlphaChannel(HWND hwnd, HDC hdc, RECT * ParentRect, BYTE alpha)
{
    HRGN hrgn;
    RECT bndRect;
    RECT wrect;
    int res;
    DWORD d;
    RGNDATA * rdata;
    DWORD rgnsz;
    RECT * rect;
    hrgn=CreateRectRgn(0,0,1,1);
    res=GetWindowRgn(hwnd,hrgn);     
    GetWindowRect(hwnd,&wrect);
    if (res==0)
    {
        DeleteObject(hrgn);
        hrgn=CreateRectRgn(wrect.left ,wrect.top ,wrect.right,wrect.bottom);
    }
    OffsetRgn(hrgn,-ParentRect->left,-ParentRect->top);
    res=GetRgnBox(hrgn,&bndRect);
    if (bndRect.bottom-bndRect.top*bndRect.right-bndRect.left ==0) return 0;
    rgnsz=GetRegionData(hrgn,0,NULL);
    rdata=(RGNDATA *) mir_alloc(rgnsz);
    GetRegionData(hrgn,rgnsz,rdata);
    rect=(RECT *)rdata->Buffer;
    for (d=0; d<rdata->rdh.nCount; d++)
    {
        SkinEngine_SetRectOpaque(hdc,&rect[d]);
    }

    mir_free_and_nill(rdata);
    DeleteObject(hrgn);
    return 1;
}

static BOOL CALLBACK CLUI_enum_SubChildWindowsProc(HWND hwnd,LPARAM lParam)
{
    CHECKFILLING * r=(CHECKFILLING *)lParam;
    CLUI_FillAlphaChannel(hwnd,r->hDC,&(r->rcRect),255);
    return 1;
}
static BOOL CALLBACK CLUI_enum_ChildWindowsProc(HWND hwnd,LPARAM lParam)
{
    int ret;
    CHECKFILLING * r=(CHECKFILLING *)lParam;
    if (GetParent(hwnd)!=pcli->hwndContactList) return 1;
    ret=SendMessage(hwnd,WM_USER+100,0,0);
    switch (ret)
    {
    case 0:  //not respond full rect should be alpha=255
        {

            CLUI_FillAlphaChannel(hwnd,r->hDC,&(r->rcRect),255);
            break;
        }
    case 1:
        EnumChildWindows(hwnd,CLUI_enum_SubChildWindowsProc,(LPARAM)r);
        break;
    case 2:
        break;
    }
    return 1;
}


int CLUI_IsInMainWindow(HWND hwnd)
{
    if (hwnd==pcli->hwndContactList) return 1;
    if (GetParent(hwnd)==pcli->hwndContactList) return 2;
    return 0;
}

int CLUI_OnSkinLoad(WPARAM wParam, LPARAM lParam)
{
    SkinEngine_LoadSkinFromDB();
    //    CreateGlyphedObject("Main Window/ScrollBar Up Button");
    //    CreateGlyphedObject("Main Window/ScrollBar Down Button");
    //    CreateGlyphedObject("Main Window/ScrollBar Thumb");       
    //    CreateGlyphedObject("Main Window/Minimize Button");
    //    CreateGlyphedObject("Main Window/Frame Backgrnd");
    //    CreateGlyphedObject("Main Window/Frame Titles Backgrnd");
    //    CreateGlyphedObject("Main Window/Backgrnd");

    //RegisterCLCRowObjects(1);
    //    {
    //        int i=-1;
    //        i=AddStrModernMaskToList("CL%status=Online%type=2%even=*even*","Main Window/Backgrnd",MainModernMaskList,NULL);
    //        i=AddStrModernMaskToList("CL%status=Online%type=1%even=*even*","Main Window/Frame Titles Backgrnd",MainModernMaskList,NULL);
    //   }


    return 0;
}



static int CLUI_ModulesLoaded(WPARAM wParam,LPARAM lParam)
{
    MENUITEMINFO mii;
    /*
    *	Updater support
    */
#ifdef _UNICODE
    CallService("Update/RegisterFL", (WPARAM)2103, (LPARAM)&pluginInfo);
#else
    CallService("Update/RegisterFL", (WPARAM)2996, (LPARAM)&pluginInfo);
#endif 

    /*
    *  End of updater support
    */

    /*
    *  Metacontact groups support
    */
    setlocale(LC_ALL,"");  //fix for case insensitive comparing
    CLCPaint_FillQuickHash();
    if (ServiceExists(MS_MC_DISABLEHIDDENGROUP));
    CallService(MS_MC_DISABLEHIDDENGROUP, (WPARAM)TRUE, (LPARAM)0);

    ZeroMemory(&mii,sizeof(mii));
    mii.cbSize=MENUITEMINFO_V4_SIZE;
    mii.fMask=MIIM_SUBMENU;
    mii.hSubMenu=(HMENU)CallService(MS_CLIST_MENUGETMAIN,0,0);
    SetMenuItemInfo(g_hMenuMain,0,TRUE,&mii);
    mii.hSubMenu=(HMENU)CallService(MS_CLIST_MENUGETSTATUS,0,0);
    SetMenuItemInfo(g_hMenuMain,1,TRUE,&mii);

    ProtocolOrder_CheckOrder();
    CLUIServices_ProtocolStatusChanged(0,0);
    SleepEx(0,TRUE);
    g_flag_bOnModulesLoadedCalled=TRUE;	
    ///pcli->pfnInvalidateDisplayNameCacheEntry(INVALID_HANDLE_VALUE);   
    PostMessage(pcli->hwndContactList,M_CREATECLC,0,0); //$$$
    return 0;
}

static LPPROTOTICKS CLUI_GetProtoTicksByProto(char * szProto)
{
    int i;

    for (i=0;i<64;i++)
    {
        if (CycleStartTick[i].szProto==NULL) break;
        if (mir_strcmp(CycleStartTick[i].szProto,szProto)) continue;
        return(&CycleStartTick[i]);
    }
    for (i=0;i<64;i++)
    {
        if (CycleStartTick[i].szProto==NULL)
        {
            CycleStartTick[i].szProto=mir_strdup(szProto);
            CycleStartTick[i].nCycleStartTick=0;
            CycleStartTick[i].nIndex=i;			
            CycleStartTick[i].bGlobal=_strcmpi(szProto,GLOBAL_PROTO_NAME)==0;
            CycleStartTick[i].himlIconList=NULL;
            return(&CycleStartTick[i]);
        }
    }
    return (NULL);
}

static int CLUI_GetConnectingIconForProtoCount(char *szProto)
{
    char file[MAX_PATH],fileFull[MAX_PATH],szFullPath[MAX_PATH];
    char szPath[MAX_PATH];
    char *str;
    int ret;

    GetModuleFileNameA(GetModuleHandle(NULL), szPath, MAX_PATH);
    str=strrchr(szPath,'\\');
    if(str!=NULL) *str=0;
    _snprintf(szFullPath, sizeof(szFullPath), "%s\\Icons\\proto_conn_%s.dll", szPath, szProto);

    lstrcpynA(file,szFullPath,sizeof(file));
    CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)file, (LPARAM)fileFull);
    ret=ExtractIconExA(fileFull,-1,NULL,NULL,1);
    if (ret==0) ret=8;
    return ret;

}

static HICON CLUI_ExtractIconFromPath(const char *path, BOOL * needFree)
{
    char *comma;
    char file[MAX_PATH],fileFull[MAX_PATH];
    int n;
    HICON hIcon;
    lstrcpynA(file,path,sizeof(file));
    comma=strrchr(file,',');
    if(comma==NULL) n=0;
    else {n=atoi(comma+1); *comma=0;}
    CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)file, (LPARAM)fileFull);
    hIcon=NULL;
    ExtractIconExA(fileFull,n,NULL,&hIcon,1);
    if (needFree)
    {
        *needFree=(hIcon!=NULL);
    }
    return hIcon;
}

HICON CLUI_LoadIconFromExternalFile(char *filename,int i,boolean UseLibrary,boolean registerit,char *IconName,char *SectName,char *Description,int internalidx, BOOL * needFree)
{
    char szPath[MAX_PATH],szMyPath[MAX_PATH], szFullPath[MAX_PATH],*str;
    HICON hIcon=NULL;
    BOOL has_proto_icon=FALSE;
    SKINICONDESC sid={0};
    if (needFree) *needFree=FALSE;
    GetModuleFileNameA(GetModuleHandle(NULL), szPath, MAX_PATH);
    GetModuleFileNameA(g_hInst, szMyPath, MAX_PATH);
    str=strrchr(szPath,'\\');
    if(str!=NULL) *str=0;
    if (UseLibrary&2) 
        _snprintf(szMyPath, sizeof(szMyPath), "%s\\Icons\\%s", szPath, filename);
    _snprintf(szFullPath, sizeof(szFullPath), "%s\\Icons\\%s,%d", szPath, filename, i);
    if (UseLibrary&2)
    {
        BOOL nf;
        HICON hi=CLUI_ExtractIconFromPath(szFullPath,&nf);
        if (hi) has_proto_icon=TRUE;
        if (hi && nf) DestroyIcon(hi);
    }
    if (!UseLibrary||!ServiceExists(MS_SKIN2_ADDICON))
    {		
        hIcon=CLUI_ExtractIconFromPath(szFullPath,needFree);
        if (hIcon) return hIcon;
        if (UseLibrary)
        {
            _snprintf(szFullPath, sizeof(szFullPath), "%s,%d", szMyPath, internalidx);
            hIcon=CLUI_ExtractIconFromPath(szFullPath,needFree);
            if (hIcon) return hIcon;		
        }
    }
    else
    {
        if (registerit&&IconName!=NULL&&SectName!=NULL)	
        {
            sid.cbSize = sizeof(sid);
            sid.cx=16;
            sid.cy=16;
            sid.hDefaultIcon = (has_proto_icon||!(UseLibrary&2))?NULL:(HICON)CallService(MS_SKIN_LOADPROTOICON,(WPARAM)NULL,(LPARAM)(-internalidx));
            sid.pszSection = Translate(SectName);				
            sid.pszName=IconName;
            sid.pszDescription=Description;
            sid.pszDefaultFile=szMyPath;
            sid.iDefaultIndex=(UseLibrary&2)?i:internalidx;
            CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
        }
        return ((HICON)CallService(MS_SKIN2_GETICON, 0, (LPARAM)IconName));
    }




    return (HICON)0;
}

static HICON CLUI_GetConnectingIconForProto(char *szProto,int b)
{
    char szFullPath[MAX_PATH];
    HICON hIcon=NULL;
    BOOL needFree;
    b=b-1;
    _snprintf(szFullPath, sizeof(szFullPath), "proto_conn_%s.dll",szProto);
    hIcon=CLUI_LoadIconFromExternalFile(szFullPath,b+1,FALSE,FALSE,NULL,NULL,NULL,0,&needFree);
    if (hIcon) return hIcon;
    hIcon=(LoadSmallIcon(g_hInst,(TCHAR *)(IDI_ICQC1+b+1)));
    return(hIcon);
}


//wParam = szProto
int CLUI_GetConnectingIconService(WPARAM wParam,LPARAM lParam)
{
    int b;						
    PROTOTICKS *pt=NULL;
    HICON hIcon=NULL;

    char *szProto=(char *)wParam;
    if (!szProto) return 0;

    pt=CLUI_GetProtoTicksByProto(szProto);

    if (pt!=NULL)
    {
        if (pt->nCycleStartTick==0)
        {
            CLUI_CreateTimerForConnectingIcon(ID_STATUS_CONNECTING,wParam);
            pt=CLUI_GetProtoTicksByProto(szProto);
        }
    }
    if (pt!=NULL)
    {
        if (pt->nCycleStartTick!=0&&pt->nIconsCount!=0) 
        {					
            b=((GetTickCount()-pt->nCycleStartTick)/(nAnimatedIconStep))%(pt->nIconsCount);
            //	if (lParam)
            //		hIcon=CLUI_GetConnectingIconForProto("Global",b);
            //	else
            if (pt->himlIconList)
                hIcon=SkinEngine_ImageList_GetIcon(pt->himlIconList,b,ILD_NORMAL);
            else
                hIcon=NULL;
            //hIcon=CLUI_GetConnectingIconForProto(szProto,b);
        };
    }


    return (int)hIcon;
};


static int CLUI_CreateTimerForConnectingIcon(WPARAM wParam,LPARAM lParam)
{

    int status=(int)wParam;
    char *szProto=(char *)lParam;					
    if (!szProto) return (0);
    if (!status) return (0);

    if ((g_StatusBarData.connectingIcon==1)&&status>=ID_STATUS_CONNECTING&&status<=ID_STATUS_CONNECTING+MAX_CONNECT_RETRIES)
    {

        PROTOTICKS *pt=NULL;
        int cnt;

        pt=CLUI_GetProtoTicksByProto(szProto);
        if (pt!=NULL)
        {
            if (pt->nCycleStartTick==0) 
            {					
                KillTimer(pcli->hwndContactList,TM_STATUSBARUPDATE+pt->nIndex);
                cnt=CLUI_GetConnectingIconForProtoCount(szProto);
                if (cnt!=0)
                {
                    int i=0;
                    nAnimatedIconStep=100;/*DBGetContactSettingWord(NULL,"CLUI","DefaultStepConnectingIcon",100);*/
                    pt->nIconsCount=cnt;
                    if (pt->himlIconList) ImageList_Destroy(pt->himlIconList);
                    pt->himlIconList=ImageList_Create(16,16,ILC_MASK|ILC_COLOR32,cnt,1);
                    for (i=0; i<cnt; i++)
                    {
                        HICON ic=CLUI_GetConnectingIconForProto(szProto,i);
                        if (ic) ImageList_AddIcon(pt->himlIconList,ic);
                        DestroyIcon_protect(ic);
                    }
                    SetTimer(pcli->hwndContactList,TM_STATUSBARUPDATE+pt->nIndex,(int)(nAnimatedIconStep)/1,0);
                    pt->bTimerCreated=1;
                    pt->nCycleStartTick=GetTickCount();
                }

            };
        };
    }
    return 0;
}
// Restore protocols to the last global status.
// Used to reconnect on restore after standby.
static void CLUI_RestoreMode(HWND hwnd)
{

    int nStatus;

    nStatus = DBGetContactSettingWord(NULL, "CList", "Status", ID_STATUS_OFFLINE);
    if (nStatus != ID_STATUS_OFFLINE)
    {
        PostMessage(hwnd, WM_COMMAND, nStatus, 0);
    }

}

int CLUI_ReloadCLUIOptions()
{
    KillTimer(pcli->hwndContactList,TM_UPDATEBRINGTIMER);
    g_nBehindEdgeSettings=DBGetContactSettingByte(NULL, "ModernData", "HideBehind", 0);
    wBehindEdgeShowDelay=DBGetContactSettingWord(NULL,"ModernData","ShowDelay",3);
    wBehindEdgeHideDelay=DBGetContactSettingWord(NULL,"ModernData","HideDelay",3);
    wBehindEdgeBorderSize=DBGetContactSettingWord(NULL,"ModernData","HideBehindBorderSize",1);


    //   if (g_nBehindEdgeSettings)  SetTimer(pcli->hwndContactList,TM_UPDATEBRINGTIMER,250,NULL);
    return 0;
}

static int CLUI_OnSettingChanging(WPARAM wParam,LPARAM lParam)
{
    DBCONTACTWRITESETTING *dbcws=(DBCONTACTWRITESETTING *)lParam;
    if (MirandaExiting()) return 0;
    if (wParam==0)
    {
        if ((dbcws->value.type==DBVT_BYTE)&&!mir_strcmp(dbcws->szModule,"CLUI"))
        {
            if (!mir_strcmp(dbcws->szSetting,"SBarShow"))
            {	
                g_bStatusBarShowOptions=dbcws->value.bVal;	
                return(0);
            };
        }
    }
    else
    {		
        if (dbcws==NULL){return(0);};
        if (dbcws->value.type==DBVT_WORD&&!mir_strcmp(dbcws->szSetting,"ApparentMode"))
        {
            ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,(HANDLE)wParam);
            return(0);
        };
        if (dbcws->value.type==DBVT_ASCIIZ&&(!mir_strcmp(dbcws->szSetting,"e-mail") || !mir_strcmp(dbcws->szSetting,"Mye-mail0")))
        {
            ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,(HANDLE)wParam);
            return(0);
        };
        if (dbcws->value.type==DBVT_ASCIIZ&&!mir_strcmp(dbcws->szSetting,"Cellular"))
        {		
            ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,(HANDLE)wParam);
            return(0);
        };

        if (dbcws->value.type==DBVT_ASCIIZ&&!mir_strcmp(dbcws->szModule,"UserInfo"))
        {
            if (!mir_strcmp(dbcws->szSetting,(HANDLE)"MyPhone0"))
            {		
                ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,(HANDLE)wParam);
                return(0);
            };
            if (!mir_strcmp(dbcws->szSetting,(HANDLE)"Mye-mail0"))
            {	
                ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,(HANDLE)wParam);	
                return(0);
            };
        };
    }
    return(0);
};


// Disconnect all protocols.
// Happens on shutdown and standby.
void CLUI_DisconnectAll()
{

    PROTOCOLDESCRIPTOR** ppProtoDesc;
    int nProtoCount;
    int nProto;

    CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&nProtoCount, (LPARAM)&ppProtoDesc);
    for (nProto = 0; nProto < nProtoCount; nProto++)
    {
        if (ppProtoDesc[nProto]->type == PROTOTYPE_PROTOCOL)
            CallProtoService(ppProtoDesc[nProto]->szName, PS_SETSTATUS, ID_STATUS_OFFLINE, 0);
    }

}

static int CLUI_PreCreateCLC(HWND parent)
{
    g_hMainThreadID=GetCurrentThreadId();
    pcli->hwndContactTree=CreateWindow(CLISTCONTROL_CLASS,TEXT(""),
        WS_CHILD|WS_CLIPCHILDREN|CLS_CONTACTLIST
        |(DBGetContactSettingByte(NULL,"CList","UseGroups",SETTING_USEGROUPS_DEFAULT)?CLS_USEGROUPS:0)
        //|CLS_HIDEOFFLINE
        |(DBGetContactSettingByte(NULL,"CList","HideOffline",SETTING_HIDEOFFLINE_DEFAULT)?CLS_HIDEOFFLINE:0)
        |(DBGetContactSettingByte(NULL,"CList","HideEmptyGroups",SETTING_HIDEEMPTYGROUPS_DEFAULT)?CLS_HIDEEMPTYGROUPS:0
        |CLS_MULTICOLUMN
        //|DBGetContactSettingByte(NULL,"CLUI","ExtraIconsAlignToLeft",1)?CLS_EX_MULTICOLUMNALIGNLEFT:0
        ),
        0,0,0,0,parent,NULL,g_hInst,NULL);
    //   SetWindowLong(pcli->hwndContactTree, GWL_EXSTYLE,GetWindowLong(pcli->hwndContactTree, GWL_EXSTYLE) | WS_EX_TRANSPARENT);


    return((int)pcli->hwndContactTree);
};

static int CLUI_CreateCLC(HWND parent)
{

    SleepEx(0,TRUE);
    {	
        //  char * s;
        // create contact list frame
        CLISTFrame Frame;
        memset(&Frame,0,sizeof(Frame));
        Frame.cbSize=sizeof(CLISTFrame);
        Frame.hWnd=pcli->hwndContactTree;
        Frame.align=alClient;
        Frame.hIcon=LoadSkinnedIcon(SKINICON_OTHER_MIRANDA);
        //LoadSmallIconShared(hInst,MAKEINTRESOURCEA(IDI_MIRANDA));
        Frame.Flags=F_VISIBLE|F_SHOWTB|F_SHOWTBTIP|F_NO_SUBCONTAINER;
        Frame.name=Translate("My Contacts");
        hFrameContactTree=(HWND)CallService(MS_CLIST_FRAMES_ADDFRAME,(WPARAM)&Frame,(LPARAM)0);
        CallService(MS_SKINENG_REGISTERPAINTSUB,(WPARAM)Frame.hWnd,(LPARAM)CLCPaint_PaintCallbackProc);

        CallService(MS_CLIST_FRAMES_SETFRAMEOPTIONS,MAKEWPARAM(FO_TBTIPNAME,hFrameContactTree),(LPARAM)Translate("My Contacts"));	
    }

    ExtraImage_ReloadExtraIcons();
    {
        nLastRequiredHeight=0;
        {
            CallService(MS_CLIST_SETHIDEOFFLINE,(WPARAM)bOldHideOffline,0);
        }

        {	int state=DBGetContactSettingByte(NULL,"CList","State",SETTING_STATE_NORMAL);
        //--if(state==SETTING_STATE_NORMAL) CLUI_ShowWindowMod(pcli->hwndContactList, SW_SHOW);
        //--else if(state==SETTING_STATE_MINIMIZED) CLUI_ShowWindowMod(pcli->hwndContactList, SW_SHOWMINIMIZED);
        }


        nLastRequiredHeight=0;
        mutex_bDisableAutoUpdate=0;

    }  

    hSettingChangedHook=HookEvent(ME_DB_CONTACT_SETTINGCHANGED,CLUI_OnSettingChanging);
    return(0);
};

static int CLUI_DrawMenuBackGround(HWND hwnd, HDC hdc, int item)
{
    RECT ra,r1;
    //    HBRUSH hbr;
    HRGN treg,treg2;
    struct ClcData * dat;

    dat=(struct ClcData*)GetWindowLong(pcli->hwndContactTree,0);
    if (!dat) return 1;
    GetWindowRect(hwnd,&ra);
    {
        MENUBARINFO mbi={0};
        mbi.cbSize=sizeof(MENUBARINFO);
        GetMenuBarInfo(hwnd,OBJID_MENU, 0, &mbi);
        if (!(mbi.rcBar.right-mbi.rcBar.left>0 && mbi.rcBar.bottom-mbi.rcBar.top>0)) return 1;
        r1=mbi.rcBar;  
        r1.bottom+= !DBGetContactSettingByte(NULL,"CLUI","LineUnderMenu",0);
        if (item<1)
        {           
            treg=CreateRectRgn(mbi.rcBar.left,mbi.rcBar.top,mbi.rcBar.right,r1.bottom);
            if (item==0)  //should remove item clips
            {
                int t;
                for (t=1; t<=2; t++)
                {
                    GetMenuBarInfo(hwnd,OBJID_MENU, t, &mbi);
                    treg2=CreateRectRgn(mbi.rcBar.left,mbi.rcBar.top,mbi.rcBar.right,mbi.rcBar.bottom);
                    CombineRgn(treg,treg,treg2,RGN_DIFF);
                    DeleteObject(treg2);  
                }

            }
        }
        else
        {
            GetMenuBarInfo(hwnd,OBJID_MENU, item, &mbi);
            treg=CreateRectRgn(mbi.rcBar.left,mbi.rcBar.top,mbi.rcBar.right,mbi.rcBar.bottom+!DBGetContactSettingByte(NULL,"CLUI","LineUnderMenu",0));
        }
        OffsetRgn(treg,-ra.left,-ra.top);
        r1.left-=ra.left;
        r1.top-=ra.top;
        r1.bottom-=ra.top;
        r1.right-=ra.left;
    }   
    //SelectClipRgn(hdc,NULL);
    SelectClipRgn(hdc,treg);
    DeleteObject(treg); 
    {
        RECT rc;
        HWND hwnd=(HWND)CallService(MS_CLUI_GETHWND,0,0);
        GetWindowRect(hwnd,&rc);
        OffsetRect(&rc,-rc.left, -rc.top);
        FillRect(hdc,&r1,GetSysColorBrush(COLOR_MENU));
        SkinEngine_SetRectOpaque(hdc,&r1);
        //SkinEngine_BltBackImage(hwnd,hdc,&r1);
    }
    SkinDrawGlyph(hdc,&r1,&r1,"Main,ID=MenuBar");
    /*   New Skin Engine
    if (dat->hMenuBackground)
    {
    BITMAP bmp;
    HBITMAP oldbm;
    HDC hdcBmp;
    int x,y;
    int maxx,maxy;
    int destw,desth;
    RECT clRect=r1;


    // XXX: Halftone isnt supported on 9x, however the scretch problems dont happen on 98.
    SetStretchBltMode(hdc, HALFTONE);

    GetObject(dat->hMenuBackground,sizeof(bmp),&bmp);
    hdcBmp=CreateCompatibleDC(hdc);
    oldbm=SelectObject(hdcBmp,dat->hMenuBackground);
    y=clRect.top;
    x=clRect.left;
    maxx=dat->MenuBmpUse&CLBF_TILEH?maxx=r1.right:x+1;
    maxy=dat->MenuBmpUse&CLBF_TILEV?maxy=r1.bottom:y+1;
    switch(dat->MenuBmpUse&CLBM_TYPE) {
    case CLB_STRETCH:
    if(dat->MenuBmpUse&CLBF_PROPORTIONAL) {
    if(clRect.right-clRect.left*bmp.bmHeight<clRect.bottom-clRect.top*bmp.bmWidth) 
    {
    desth=clRect.bottom-clRect.top;
    destw=desth*bmp.bmWidth/bmp.bmHeight;
    }
    else 
    {
    destw=clRect.right-clRect.left;
    desth=destw*bmp.bmHeight/bmp.bmWidth;
    }
    }
    else {
    destw=clRect.right-clRect.left;
    desth=clRect.bottom-clRect.top;
    }
    break;
    case CLB_STRETCHH:
    if(dat->MenuBmpUse&CLBF_PROPORTIONAL) {
    destw=clRect.right-clRect.left;
    desth=destw*bmp.bmHeight/bmp.bmWidth;
    }
    else {
    destw=clRect.right-clRect.left;
    desth=bmp.bmHeight;
    }
    break;
    case CLB_STRETCHV:
    if(dat->MenuBmpUse&CLBF_PROPORTIONAL) {
    desth=clRect.bottom-clRect.top;
    destw=desth*bmp.bmWidth/bmp.bmHeight;
    }
    else {
    destw=bmp.bmWidth;
    desth=clRect.bottom-clRect.top;
    }
    break;
    default:    //clb_topleft
    destw=bmp.bmWidth;
    desth=bmp.bmHeight;					
    break;
    }
    if (desth && destw)
    for(y=clRect.top;y<maxy;y+=desth) {
    for(x=clRect.left;x<maxx;x+=destw)
    StretchBlt(hdc,x,y,destw,desth,hdcBmp,0,0,bmp.bmWidth,bmp.bmHeight,SRCCOPY);
    }
    SelectObject(hdcBmp,oldbm);
    ModernDeleteDC(hdcBmp);

    }                  

    else
    {
    hbr=CreateSolidBrush(dat->MenuBkColor);
    FillRect(hdc,&r1,hbr);
    DeleteObject(hbr);    
    }
    */
    SelectClipRgn(hdc,NULL);
    return 0;
}

int CLUI_SizingGetWindowRect(HWND hwnd,RECT * rc)
{
    if (mutex_bDuringSizing && hwnd==pcli->hwndContactList)
        *rc=rcSizingRect;
    else
        GetWindowRect(hwnd,rc);
    return 1;
}


static void CLUI_SnappingToEdge(HWND hwnd, WINDOWPOS * wp) //by ZORG
{
    if (DBGetContactSettingByte(NULL,"CLUI","SnapToEdges",0))
    {
        RECT* dr;
        MONITORINFO monInfo;
        HMONITOR curMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        monInfo.cbSize = sizeof(monInfo);
        GetMonitorInfo(curMonitor, &monInfo);

        dr = &(monInfo.rcWork);

        // Left side
        if ( wp->x < dr->left + 10 && wp->x > dr->left - 10 && g_nBehindEdgeSettings!=1)
            wp->x = dr->left;

        // Right side
        if ( dr->right - wp->x - wp->cx <10 && dr->right - wp->x - wp->cx > -10 && g_nBehindEdgeSettings!=2)
            wp->x = dr->right - wp->cx;

        // Top side
        if ( wp->y < dr->top + 10 && wp->y > dr->top - 10)
            wp->y = dr->top;

        // Bottom side
        if ( dr->bottom - wp->y - wp->cy <10 && dr->bottom - wp->y - wp->cy > -10)
            wp->y = dr->bottom - wp->cy;
    }
}

LRESULT CALLBACK CLUI__cli_ContactListWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{    
    /*
    This registers a window message with RegisterWindowMessage() and then waits for such a message,
    if it gets it, it tries to open a file mapping object and then maps it to this process space,
    it expects 256 bytes of data (incl. NULL) it will then write back the profile it is using the DB to fill in the answer.

    The caller is expected to create this mapping object and tell us the ID we need to open ours.	
    */
    if (g_bSTATE==STATE_EXITING && msg!=WM_DESTROY) 
        return 0;
    if (msg==UM_CALLSYNCRONIZED)
    {
        switch (wParam)
        {
        case SYNC_SMOOTHANIMATION:
            return CLUI_SmoothAlphaThreadTransition((HWND)lParam);
        case SYNC_GETPDNCE:
            return CListSettings_GetCopyFromCache((pdisplayNameCacheEntry)lParam);
        case SYNC_SETPDNCE:
            return CListSettings_SetToCache((pdisplayNameCacheEntry)lParam);
        }
        return 0;
    }
    if (msg==uMsgGetProfile && wParam != 0) { /* got IPC message */
        HANDLE hMap;
        char szName[MAX_PATH];
        int rc=0;
        _snprintf(szName,sizeof(szName),"Miranda::%u", wParam); // caller will tell us the ID of the map
        hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,szName);
        if (hMap != NULL) {
            void *hView=NULL;
            hView=MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, MAX_PATH);
            if (hView) {
                char szFilePath[MAX_PATH], szProfile[MAX_PATH];
                CallService(MS_DB_GETPROFILEPATH,MAX_PATH,(LPARAM)&szFilePath);
                CallService(MS_DB_GETPROFILENAME,MAX_PATH,(LPARAM)&szProfile);
                _snprintf(hView,MAX_PATH,"%s\\%s",szFilePath,szProfile);
                UnmapViewOfFile(hView);
                rc=1;
            }
            CloseHandle(hMap);
        }
        return rc;
    }

    if (g_bLayered)
    {
        LRESULT res=0;
        if (msg==WM_SIZING)
        {
            static a=0;
            RECT* wp=(RECT*)lParam;
            if (bNeedFixSizingRect && (rcCorrectSizeRect.bottom!=0||rcCorrectSizeRect.top!=0))
            {
                if(wParam!=WMSZ_BOTTOM) wp->bottom=rcCorrectSizeRect.bottom;
                if(wParam!=WMSZ_TOP) wp->top=rcCorrectSizeRect.top;       
            }
            bNeedFixSizingRect=0;
            rcSizingRect=*wp;
            mutex_bDuringSizing=1;
            return 1;
        }

        if (msg==WM_WINDOWPOSCHANGING)
        {


            WINDOWPOS * wp;
            HDWP PosBatch;
            RECT work_rect={0};
            RECT temp_rect={0};
            wp=(WINDOWPOS *)lParam;
            GetWindowRect(hwnd,&rcOldWindowRect);

            // ���������� � ����� by ZorG	
            CLUI_SnappingToEdge(hwnd, wp);

            if ((rcOldWindowRect.bottom-rcOldWindowRect.top!=wp->cy || rcOldWindowRect.right-rcOldWindowRect.left !=wp->cx)&&!(wp->flags&SWP_NOSIZE))
            {
                {         
                    if (!(wp->flags&SWP_NOMOVE)) 
                    {
                        rcNewWindowRect.left=wp->x;
                        rcNewWindowRect.top=wp->y;
                    }
                    else
                    {
                        rcNewWindowRect.left=rcOldWindowRect.left;
                        rcNewWindowRect.top=rcOldWindowRect.top;
                    }
                    rcNewWindowRect.right=rcNewWindowRect.left+wp->cx;  
                    rcNewWindowRect.bottom=rcNewWindowRect.top+wp->cy;
                    work_rect=rcNewWindowRect;
                }
                //resize frames (batch)
                {
                    PosBatch=BeginDeferWindowPos(1);
                    SizeFramesByWindowRect(&work_rect,&PosBatch,0);
                }
                //Check rect after frames resize
                {
                    GetWindowRect(hwnd,&temp_rect);
                }
                //Here work_rect should be changed to fit possible changes in cln_listsizechange
                if(bNeedFixSizingRect)
                {
                    work_rect=rcSizingRect;
                    wp->x=work_rect.left;
                    wp->y=work_rect.top;
                    wp->cx=work_rect.right-work_rect.left;
                    wp->cy=work_rect.bottom-work_rect.top;
                    wp->flags&=~(SWP_NOMOVE);
                }
                //reposition buttons and new size applying
                {
                    ModernButton_ReposButtons(hwnd,FALSE,&work_rect);
                    SkinEngine_PrepeareImageButDontUpdateIt(&work_rect);
                    g_mutex_uPreventDockMoving=0;			
                    SkinEngine_UpdateWindowImageRect(&work_rect);        
                    EndDeferWindowPos(PosBatch);
                    g_mutex_uPreventDockMoving=1;
                }       
                Sleep(0);               
                mutex_bDuringSizing=0; 
                DefWindowProc(hwnd,msg,wParam,lParam);
                return SendMessage(hwnd,WM_WINDOWPOSCHANGED,wParam,lParam);
            }
            else
            {
                SetRect(&rcCorrectSizeRect,0,0,0,0);
                // bNeedFixSizingRect=0;
            }
            return DefWindowProc(hwnd,msg,wParam,lParam); 
        }	
    }

    else if (msg==WM_WINDOWPOSCHANGING)
    {
        // Snaping if it is not in LayeredMode	
        WINDOWPOS * wp;
        wp=(WINDOWPOS *)lParam;
        CLUI_SnappingToEdge(hwnd, wp);
        return DefWindowProc(hwnd,msg,wParam,lParam); 
    }
    switch (msg) 
    {
    case WM_EXITSIZEMOVE:
        {
            int res=DefWindowProc(hwnd, msg, wParam, lParam);
            ReleaseCapture();
            TRACE("WM_EXITSIZEMOVE\n");
            SendMessage(hwnd, WM_ACTIVATE, (WPARAM)WA_ACTIVE, (LPARAM)hwnd);
            return res;
        }
    case UM_UPDATE:
        if (g_flag_bPostWasCanceled) return 0;
        return SkinEngine_ValidateFrameImageProc(NULL);               
    case WM_INITMENU:
        {
            if (ServiceExists(MS_CLIST_MENUBUILDMAIN)){CallService(MS_CLIST_MENUBUILDMAIN,0,0);};
            return(0);
        };
    case WM_NCACTIVATE:
    case WM_PRINT:  
    case WM_NCPAINT:
        {
            int r = DefWindowProc(hwnd, msg, wParam, lParam);
            if (!g_bLayered && DBGetContactSettingByte(NULL,"CLUI","ShowMainMenu",SETTING_SHOWMAINMENU_DEFAULT))
            {
                HDC hdc;
                hdc=NULL;
                if (msg==WM_PRINT)
                    hdc=(HDC)wParam;
                if (!hdc) hdc=GetWindowDC(hwnd);
                CLUI_DrawMenuBackGround(hwnd,hdc,0);
                if (msg!=WM_PRINT) ReleaseDC(hwnd,hdc);
            }
            return r;
        }
    case WM_ERASEBKGND:
        return 0;

    case WM_NCCREATE:
        {	LPCREATESTRUCT p = (LPCREATESTRUCT)lParam;
        p->style &= ~(CS_HREDRAW | CS_VREDRAW);
        }
        break;

    case WM_PAINT:
        if (!g_bLayered && IsWindowVisible(hwnd))
        {
            RECT w={0};
            RECT w2={0};
            PAINTSTRUCT ps={0};
            HDC hdc;
            HBITMAP hbmp,oldbmp;
            HDC paintDC;

            GetClientRect(hwnd,&w);
            if (!(w.right>0 && w.bottom>0)) return DefWindowProc(hwnd, msg, wParam, lParam); 
            paintDC = GetDC(hwnd);
            w2=w;
            hdc=CreateCompatibleDC(paintDC);
            hbmp=SkinEngine_CreateDIB32(w.right,w.bottom);
            oldbmp=SelectObject(hdc,hbmp);
            SkinEngine_ReCreateBackImage(FALSE,NULL);
            BitBlt(paintDC,w2.left,w2.top,w2.right-w2.left,w2.bottom-w2.top,g_pCachedWindow->hBackDC,w2.left,w2.top,SRCCOPY);
            SelectObject(hdc,oldbmp);
            DeleteObject(hbmp);
            mod_DeleteDC(hdc);
            ReleaseDC(hwnd,paintDC);
            ValidateRect(hwnd,NULL);
            ps.fErase=FALSE;
            EndPaint(hwnd,&ps); 
        }
        if (0&&(DBGetContactSettingDword(NULL,"CLUIFrames","GapBetweenFrames",1) || DBGetContactSettingDword(NULL,"CLUIFrames","GapBetweenTitleBar",1)))
        {
            if (IsWindowVisible(hwnd))
                if (g_bLayered)
                    SkinInvalidateFrame(hwnd,NULL,0);				
                else 
                {
                    RECT w={0};
                    RECT w2={0};
                    PAINTSTRUCT ps={0};
                    GetWindowRect(hwnd,&w);
                    OffsetRect(&w,-w.left,-w.top);
                    BeginPaint(hwnd,&ps);
                    if ((ps.rcPaint.bottom-ps.rcPaint.top)*(ps.rcPaint.right-ps.rcPaint.left)==0)
                        w2=w;
                    else
                        w2=ps.rcPaint;
                    SkinDrawGlyph(ps.hdc,&w,&w2,"Main,ID=Background,Opt=Non-Layered");
                    ps.fErase=FALSE;
                    EndPaint(hwnd,&ps); 
                }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);

    case WM_CREATE:
        CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)GetMenu(hwnd),0);
        DrawMenuBar(hwnd);
        g_bStatusBarShowOptions=DBGetContactSettingByte(NULL,"CLUI","SBarShow",7);		

        CLUIServices_ProtocolStatusChanged(0,0);

        {	MENUITEMINFO mii;
        ZeroMemory(&mii,sizeof(mii));
        mii.cbSize=MENUITEMINFO_V4_SIZE;
        mii.fMask=MIIM_TYPE|MIIM_DATA;
        himlMirandaIcon=ImageList_Create(GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),ILC_COLOR32|ILC_MASK,1,1);
        ImageList_AddIcon(himlMirandaIcon,LoadSkinnedIcon(SKINICON_OTHER_MIRANDA));
        mii.dwItemData=MENU_MIRANDAMENU;
        mii.fType=MFT_OWNERDRAW;
        mii.dwTypeData=NULL;
        SetMenuItemInfo(GetMenu(hwnd),0,TRUE,&mii);

        // mii.fMask=MIIM_TYPE;
        mii.fType=MFT_OWNERDRAW;
        mii.dwItemData=MENU_STATUSMENU;
        SetMenuItemInfo(GetMenu(hwnd),1,TRUE,&mii);
        }
        //PostMessage(hwnd, M_CREATECLC, 0, 0);
        //pcli->hwndContactList=hwnd;
        uMsgGetProfile=RegisterWindowMessage(TEXT("Miranda::GetProfile")); // don't localise
        //#ifndef _DEBUG
        // Miranda is starting up! Restore last status mode.
        // This is not done in debug builds because frequent
        // reconnections will get you banned from the servers.
        CLUI_RestoreMode(hwnd);
        //#endif
        bTransparentFocus=1;
        return FALSE;

    case M_SETALLEXTRAICONS:
        return 0;

    case M_CREATECLC:
        CLUI_CreateCLC(hwnd);
        if (DBGetContactSettingByte(NULL,"CList","ShowOnStart",0)) cliShowHide((WPARAM)hwnd,(LPARAM)1); 
        PostMessage(pcli->hwndContactTree,CLM_AUTOREBUILD,0,0);
        return 0;

    case  WM_LBUTTONDOWN:
        {
            POINT pt;
            int k=0;
            pt.x = LOWORD(lParam); 
            pt.y = HIWORD(lParam); 
            ClientToScreen(hwnd,&pt);

            k=CLUI_SizingOnBorder(pt,1);
            if (k)
            {
                mutex_bIgnoreActivation=1;
                return 0;
            }

        }
        break;
    case  WM_PARENTNOTIFY:      
        if (wParam==WM_LBUTTONDOWN) {   
            POINT pt;
            int k=0;
            pt.x = LOWORD(lParam); 
            pt.y = HIWORD(lParam); 
            ClientToScreen(hwnd,&pt);

            k=CLUI_SizingOnBorder(pt,1);
            wParam=0;
            lParam=0;

            if (k) {
                mutex_bIgnoreActivation=1;
                return 0;
            }
            //	CLUIFrames_ActivateSubContainers(1);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);;

    case WM_SIZING:
        return TRUE;

    case WM_MOVE:
        {
            RECT rc;
            CallWindowProc(DefWindowProc, hwnd, msg, wParam, lParam);
            mutex_bDuringSizing=0;      
            GetWindowRect(hwnd, &rc);
            CheckFramesPos(&rc);
            CLUIFrames_OnMoving(hwnd,&rc);
            if(!IsIconic(hwnd)) {
                if(!CallService(MS_CLIST_DOCKINGISDOCKED,0,0))
                { //if g_bDocked, dont remember pos (except for width)
                    DBWriteContactSettingDword(NULL,"CList","Height",(DWORD)(rc.bottom - rc.top));
                    DBWriteContactSettingDword(NULL,"CList","x",(DWORD)rc.left);
                    DBWriteContactSettingDword(NULL,"CList","y",(DWORD)rc.top);
                }
                DBWriteContactSettingDword(NULL,"CList","Width",(DWORD)(rc.right - rc.left));
            }
            return TRUE;
        }
    case WM_SIZE:    
        {   
            RECT rc;
            if (g_mutex_bSizing) return 0;
            if(wParam!=SIZE_MINIMIZED /*&& IsWindowVisible(hwnd)*/) 
            {
                if ( pcli->hwndContactList == NULL )
                    return 0;

                if (!g_bLayered)
                    SkinEngine_ReCreateBackImage(TRUE,NULL);

                GetWindowRect(hwnd, &rc);
                CheckFramesPos(&rc);
                ModernButton_ReposButtons(hwnd,FALSE,&rc);
                ModernButton_ReposButtons(hwnd,7,NULL);
                if (g_bLayered)
                    SkinEngine_Service_UpdateFrameImage((WPARAM)hwnd,0);
                if (!g_bLayered)
                {
                    g_mutex_bSizing=1;
                    CLUIFrames_OnClistResize_mod((WPARAM)hwnd,(LPARAM)0,1);
                    CLUIFrames_ApplyNewSizes(2);
                    CLUIFrames_ApplyNewSizes(1);
                    SendMessage(hwnd,CLN_LISTSIZECHANGE,0,0);				  
                    g_mutex_bSizing=0;
                }

                //       SkinEngine_RedrawCompleteWindow();
                if(!CallService(MS_CLIST_DOCKINGISDOCKED,0,0))
                { //if g_bDocked, dont remember pos (except for width)
                    DBWriteContactSettingDword(NULL,"CList","Height",(DWORD)(rc.bottom - rc.top));
                    DBWriteContactSettingDword(NULL,"CList","x",(DWORD)rc.left);
                    DBWriteContactSettingDword(NULL,"CList","y",(DWORD)rc.top);
                }
                else SetWindowRgn(hwnd,NULL,0);
                DBWriteContactSettingDword(NULL,"CList","Width",(DWORD)(rc.right - rc.left));	                               

                if (!g_bLayered)
                {
                    HRGN hRgn1;
                    RECT r;
                    int v,h;
                    int w=10;
                    GetWindowRect(hwnd,&r);
                    h=(r.right-r.left)>(w*2)?w:(r.right-r.left);
                    v=(r.bottom-r.top)>(w*2)?w:(r.bottom-r.top);
                    h=(h<v)?h:v;
                    hRgn1=CreateRoundRectRgn(0,0,(r.right-r.left+1),(r.bottom-r.top+1),h,h);
                    if ((DBGetContactSettingByte(NULL,"CLC","RoundCorners",0)) && (!CallService(MS_CLIST_DOCKINGISDOCKED,0,0)))
                        SetWindowRgn(hwnd,hRgn1,1); 
                    else
                    {
                        DeleteObject(hRgn1);
                        SetWindowRgn(hwnd,NULL,1);
                    }
                    RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_UPDATENOW|RDW_ALLCHILDREN);   
                } 			
            }
            else {
                if(DBGetContactSettingByte(NULL,"CList","Min2Tray",SETTING_MIN2TRAY_DEFAULT)) {
                    CLUI_ShowWindowMod(hwnd, SW_HIDE);
                    DBWriteContactSettingByte(NULL,"CList","State",SETTING_STATE_HIDDEN);
                }
                else DBWriteContactSettingByte(NULL,"CList","State",SETTING_STATE_MINIMIZED);
            }

            return TRUE;
        }

    case WM_SETFOCUS:
        if (hFrameContactTree) {					
            int isfloating = CallService(MS_CLIST_FRAMES_GETFRAMEOPTIONS,MAKEWPARAM(FO_FLOATING,hFrameContactTree),0);
            if ( !isfloating )
                SetFocus(pcli->hwndContactTree);
        }
        UpdateWindow(hwnd);
        return 0;

    case WM_ACTIVATE:
        {
            BOOL IsOption=FALSE;
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            if (DBGetContactSettingByte(NULL, "ModernData", "HideBehind", 0))
            {
                if(wParam==WA_INACTIVE && ((HWND)lParam!=hwnd) && GetParent((HWND)lParam)!=hwnd && !IsOption) 
                {
                    //hide
                    //CLUI_HideBehindEdge();
                    if (!g_bCalledFromShowHide) CLUI_UpdateTimer(0);
                }
                else
                {
                    //show
                    if (!g_bCalledFromShowHide) CLUI_ShowFromBehindEdge();
                }
            }

            if (!IsWindowVisible(hwnd) || mutex_bShowHideCalledFromAnimation) 
            {            
                KillTimer(hwnd,TM_AUTOALPHA);
                return 0;
            }
            if(wParam==WA_INACTIVE && ((HWND)lParam!=hwnd) && !CLUI_CheckOwnedByClui((HWND)lParam) && !IsOption) 
            {
                //if (!DBGetContactSettingByte(NULL,"CList","OnTop",SETTING_ONTOP_DEFAULT))
                //{
                //  //---+++SetWindowPos(pcli->hwndContactList, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE|SWP_NOACTIVATE);
                //  //CLUIFrames_ActivateSubContainers(0);					
                //}
                //if (IS_WM_MOUSE_DOWN_IN_TRAY) //check cursor is over trayicon
                //{
                //	IS_WM_MOUSE_DOWN_IN_TRAY=0;
                //	return DefWindowProc(hwnd,msg,wParam,lParam);
                //}
                //else
                if(g_bTransparentFlag)
                    if(bTransparentFocus)
                        SetTimer(hwnd, TM_AUTOALPHA,250,NULL);

            }
            else {
                if (!DBGetContactSettingByte(NULL,"CList","OnTop",SETTING_ONTOP_DEFAULT))
                    CLUIFrames_ActivateSubContainers(1);
                if(g_bTransparentFlag) {
                    KillTimer(hwnd,TM_AUTOALPHA);
                    CLUI_SmoothAlphaTransition(hwnd, DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT), 1);
                    bTransparentFocus=1;
                }
            }
            RedrawWindow(hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
            if(g_bTransparentFlag)
            {
                BYTE alpha;
                if (wParam!=WA_INACTIVE || CLUI_CheckOwnedByClui((HWND)lParam)|| IsOption || ((HWND)lParam==hwnd) || GetParent((HWND)lParam)==hwnd) alpha=DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT);
                else 
                    alpha=g_bTransparentFlag?DBGetContactSettingByte(NULL,"CList","AutoAlpha",SETTING_AUTOALPHA_DEFAULT):255;
                CLUI_SmoothAlphaTransition(hwnd, alpha, 1);
                if(IsOption) DefWindowProc(hwnd,msg,wParam,lParam);
                else   return 1; 	
            }
            //DefWindowProc(hwnd,msg,wParam,lParam);
            return  DefWindowProc(hwnd,msg,wParam,lParam);
        }
    case  WM_SETCURSOR:
        {
            int k=0;
            HWND gf=GetForegroundWindow();
            if (g_nBehindEdgeState>=0)  CLUI_UpdateTimer(1);
            if(g_bTransparentFlag) {
                if (!bTransparentFocus && gf!=hwnd)
                {
                    CLUI_SmoothAlphaTransition(hwnd, DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT), 1);
                    bTransparentFocus=1;
                    SetTimer(hwnd, TM_AUTOALPHA,250,NULL);
                }
            }
            k=CLUI_TestCursorOnBorders();               
            return k?k:1;//DefWindowProc(hwnd,msg,wParam,lParam);
        }
    case WM_MOUSEACTIVATE:
        {
            int r;
            if (mutex_bIgnoreActivation) 
            {   
                mutex_bIgnoreActivation=0;
                return(MA_NOACTIVATEANDEAT);                   
            }
            r=DefWindowProc(hwnd,msg,wParam,lParam);
            CLUIFrames_RepaintSubContainers();
            return r;
        }

    case WM_NCLBUTTONDOWN:
        {   
            POINT pt;
            int k=0;
            pt.x = LOWORD(lParam); 
            pt.y = HIWORD(lParam); 
            //ClientToScreen(hwnd,&pt);
            k=CLUI_SizingOnBorder(pt,1);
            //if (!k) after_syscommand=1;
            return k?k:DefWindowProc(hwnd,msg,wParam,lParam);

            break;
        }
    case WM_NCHITTEST:
        {
            LRESULT result;
            result=DefWindowProc(hwnd,WM_NCHITTEST,wParam,lParam);
            if(result==HTSIZE || result==HTTOP || result==HTTOPLEFT || result==HTTOPRIGHT ||
                result==HTBOTTOM || result==HTBOTTOMRIGHT || result==HTBOTTOMLEFT)
                if(DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)) return HTCLIENT;
            if (result==HTMENU) 
            {
                int t;
                POINT pt;
                pt.x=LOWORD(lParam);
                pt.y=HIWORD(lParam);
                t=MenuItemFromPoint(hwnd,g_hMenuMain,pt);
                if (t==-1 && (DBGetContactSettingByte(NULL,"CLUI","ClientAreaDrag",SETTING_CLIENTDRAG_DEFAULT)))
                    return HTCAPTION;
            }
            if (result==HTCLIENT)
            {
                POINT pt;
                int k;
                pt.x=LOWORD(lParam);
                pt.y=HIWORD(lParam);			
                k=CLUI_SizingOnBorder(pt,0);
                if (!k && (DBGetContactSettingByte(NULL,"CLUI","ClientAreaDrag",SETTING_CLIENTDRAG_DEFAULT)))
                    return HTCAPTION;
                else return k+9;
            }
            return result;
        }

    case WM_TIMER:
        if (MirandaExiting()) return 0;
        if ((int)wParam>=TM_STATUSBARUPDATE&&(int)wParam<=TM_STATUSBARUPDATE+64)
        {		
            int status,i;
            PROTOTICKS *pt=NULL;
            if (!pcli->hwndStatus) return 0;
            for (i=0;i<64;i++)
            {

                pt=&CycleStartTick[i];

                if (pt->szProto!=NULL&&pt->bTimerCreated==1)
                {
                    if (pt->bGlobal)
                        status=g_bMultiConnectionMode?ID_STATUS_CONNECTING:0;
                    else
                        status=CallProtoService(pt->szProto,PS_GETSTATUS,0,0);

                    if (!(status>=ID_STATUS_CONNECTING&&status<=ID_STATUS_CONNECTING+MAX_CONNECT_RETRIES))
                    {													
                        pt->nCycleStartTick=0;
                        ImageList_Destroy(pt->himlIconList);
                        pt->himlIconList=NULL;
                        KillTimer(hwnd,TM_STATUSBARUPDATE+pt->nIndex);
                        pt->bTimerCreated=0;
                    }
                }

            };

            pt=&CycleStartTick[wParam-TM_STATUSBARUPDATE];
            {
                if(IsWindowVisible(pcli->hwndStatus)) SkinInvalidateFrame(pcli->hwndStatus,NULL,0);//InvalidateRectZ(pcli->hwndStatus,NULL,TRUE);
                if (DBGetContactSettingByte(NULL,"CList","TrayIcon",SETTING_TRAYICON_DEFAULT)!=SETTING_TRAYICON_CYCLE) 
                    if (pt->bGlobal) 
                        cliTrayIconUpdateBase(g_szConnectingProto);
                    else
                        cliTrayIconUpdateBase(pt->szProto);

            }
            SkinInvalidateFrame(pcli->hwndStatus,NULL,0);
            break;
        }
        else if ((int)wParam==TM_SMOTHALPHATRANSITION)
            CLUI_SmoothAlphaTransition(hwnd, 0, 2);
        else if ((int)wParam==TM_AUTOALPHA)
        {	int inwnd;

        if (GetForegroundWindow()==hwnd)
        {
            KillTimer(hwnd,TM_AUTOALPHA);
            inwnd=1;
        }
        else {
            POINT pt;
            HWND hwndPt;
            pt.x=(short)LOWORD(GetMessagePos());
            pt.y=(short)HIWORD(GetMessagePos());
            hwndPt=WindowFromPoint(pt);

            inwnd=0;
            inwnd=CLUI_CheckOwnedByClui(hwndPt);
            /*
            {
            thwnd=hwndPt;
            do
            {
            if (thwnd==hwnd) inwnd=TRUE;
            if (!inwnd)
            thwnd=GetParent(thwnd);
            }while (thwnd && !inwnd); 
            }*/

        }
        if (inwnd!=bTransparentFocus)
        { //change
            bTransparentFocus=inwnd;
            if(bTransparentFocus) CLUI_SmoothAlphaTransition(hwnd, (BYTE)DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT), 1);
            else  
            {
                CLUI_SmoothAlphaTransition(hwnd, (BYTE)(g_bTransparentFlag?DBGetContactSettingByte(NULL,"CList","AutoAlpha",SETTING_AUTOALPHA_DEFAULT):255), 1);
            }
        }
        if(!bTransparentFocus) KillTimer(hwnd,TM_AUTOALPHA);
        }     
        else if ((int)wParam==TM_DELAYEDSIZING && mutex_bDelayedSizing)
        {
            if (!mutex_bDuringSizing)
            {
                mutex_bDelayedSizing=0;
                KillTimer(hwnd,TM_DELAYEDSIZING);
                pcli->pfnClcBroadcast( INTM_SCROLLBARCHANGED,0,0);
            }
        }
        else if ((int)wParam==TM_BRINGOUTTIMEOUT)
        {
            //hide
            POINT pt;
            HWND hAux;
            int mouse_in_window=0;
            KillTimer(hwnd,TM_BRINGINTIMEOUT);
            KillTimer(hwnd,TM_BRINGOUTTIMEOUT);
            GetCursorPos(&pt);
            hAux = WindowFromPoint(pt);
            mouse_in_window=CLUI_CheckOwnedByClui(hAux);
            if (!mouse_in_window && GetForegroundWindow()!=hwnd) CLUI_HideBehindEdge(); 
        }
        else if ((int)wParam==TM_BRINGINTIMEOUT)
        {
            //show
            POINT pt;
            HWND hAux;
            int mouse_in_window=0;
            KillTimer(hwnd,TM_BRINGINTIMEOUT);
            KillTimer(hwnd,TM_BRINGOUTTIMEOUT);
            GetCursorPos(&pt);
            hAux = WindowFromPoint(pt);
            while(hAux != NULL) 
            {
                if (hAux == hwnd) {mouse_in_window=1; break;}
                hAux = GetParent(hAux);
            }
            if (mouse_in_window)  CLUI_ShowFromBehindEdge();

        }
        else if ((int)wParam==TM_UPDATEBRINGTIMER)
        {
            CLUI_UpdateTimer(0);
        }

        return TRUE;


    case WM_SHOWWINDOW:
        {	
            BYTE gAlpha;

            if (lParam) return 0;
            if (mutex_bShowHideCalledFromAnimation) return 1; 
            {

                if (!wParam) gAlpha=0;
                else 
                    gAlpha=(DBGetContactSettingByte(NULL,"CList","Transparent",0)?DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT):255);
                if (wParam) 
                {
                    g_bCurrentAlpha=0;
                    CLUIFrames_OnShowHide(pcli->hwndContactList,1);
                    SkinEngine_RedrawCompleteWindow();
                }
                CLUI_SmoothAlphaTransition(hwnd, gAlpha, 1);
            }
            return 0;
        }
    case WM_SYSCOMMAND:
        if(wParam==SC_MAXIMIZE) return 0;

        DefWindowProc(hwnd, msg, wParam, lParam);
        if (DBGetContactSettingByte(NULL,"CList","OnDesktop",0))
            CLUIFrames_ActivateSubContainers(TRUE);
        return 0;

    case WM_KEYDOWN:
        if (wParam==VK_F5)
            SendMessage(pcli->hwndContactTree,CLM_AUTOREBUILD,0,0);
        break;

    case WM_GETMINMAXINFO:
        DefWindowProc(hwnd,msg,wParam,lParam);
        ((LPMINMAXINFO)lParam)->ptMinTrackSize.x=max(DBGetContactSettingWord(NULL,"CLUI","MinWidth",18),max(18,DBGetContactSettingByte(NULL,"CLUI","LeftClientMargin",0)+DBGetContactSettingByte(NULL,"CLUI","RightClientMargin",0)+18));
        if (nRequiredHeight==0)
        {
            ((LPMINMAXINFO)lParam)->ptMinTrackSize.y=CLUIFramesGetMinHeight();
        }
        return 0;

    case WM_MOVING:
        CallWindowProc(DefWindowProc, hwnd, msg, wParam, lParam);
        if (0) //showcontents is turned on
        {
            CLUIFrames_OnMoving(hwnd,(RECT*)lParam);
            //if (!g_bLayered) SkinEngine_UpdateWindowImage();
        }
        return TRUE;


        //MSG FROM CHILD CONTROL
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == pcli->hwndContactTree) {
            switch (((LPNMHDR)lParam)->code) {
    case CLN_NEWCONTACT:
        {
            NMCLISTCONTROL *nm=(NMCLISTCONTROL *)lParam;
            if (nm!=NULL) ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,nm->hItem );
            return 0;
        }
    case CLN_LISTREBUILT:
        ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,0);
        return(FALSE);

    case CLN_LISTSIZECHANGE:
        {
            NMCLISTCONTROL *nmc=(NMCLISTCONTROL*)lParam;
            static RECT rcWindow,rcTree,rcTree2,rcWorkArea,rcOld;
            int maxHeight,newHeight;
            int winstyle;
            if (mutex_bDisableAutoUpdate==1){return 0;};
            if (mutex_bDuringSizing)
                rcWindow=rcSizingRect;
            else					
                GetWindowRect(hwnd,&rcWindow);
            if(!DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)){return 0;}
            if(CallService(MS_CLIST_DOCKINGISDOCKED,0,0)) return 0;
            if (hFrameContactTree==0)return 0;
            maxHeight=DBGetContactSettingByte(NULL,"CLUI","MaxSizeHeight",75);
            rcOld=rcWindow;
            GetWindowRect(pcli->hwndContactTree,&rcTree);
            //
            {
                wndFrame* frm=FindFrameByItsHWND(pcli->hwndContactTree);
                if (frm) 
                    rcTree2=frm->wndSize;
                else
                    SetRect(&rcTree2,0,0,0,0);
            }
            winstyle=GetWindowLong(pcli->hwndContactTree,GWL_STYLE);

            SystemParametersInfo(SPI_GETWORKAREA,0,&rcWorkArea,FALSE);
            if (nmc->pt.y>(rcWorkArea.bottom-rcWorkArea.top)) 
            {
                nmc->pt.y=(rcWorkArea.bottom-rcWorkArea.top);
                //break;
            };
            if ((nmc->pt.y)==nLastRequiredHeight)
            {
                //	break;
            }
            nLastRequiredHeight=nmc->pt.y;
            newHeight=max(CLUIFramesGetMinHeight(),max(nmc->pt.y,3)+1+((winstyle&WS_BORDER)?2:0)+(rcWindow.bottom-rcWindow.top)-(rcTree.bottom-rcTree.top));
            if (newHeight==(rcWindow.bottom-rcWindow.top)) return 0;

            if(newHeight>(rcWorkArea.bottom-rcWorkArea.top)*maxHeight/100)
                newHeight=(rcWorkArea.bottom-rcWorkArea.top)*maxHeight/100;

            if(DBGetContactSettingByte(NULL,"CLUI","AutoSizeUpward",0)) {
                rcWindow.top=rcWindow.bottom-newHeight;
                if(rcWindow.top<rcWorkArea.top) rcWindow.top=rcWorkArea.top;
            }
            else {
                rcWindow.bottom=rcWindow.top+newHeight;
                if(rcWindow.bottom>rcWorkArea.bottom) rcWindow.bottom=rcWorkArea.bottom;
            }
            if (nRequiredHeight==1){return 0;};
            nRequiredHeight=1;					
            if (mutex_bDuringSizing)
            {
                bNeedFixSizingRect=1;           
                rcSizingRect.top=rcWindow.top;
                rcSizingRect.bottom=rcWindow.bottom;
                rcCorrectSizeRect=rcSizingRect;						
            }
            else
            {
                bNeedFixSizingRect=0;
            }
            if (!mutex_bDuringSizing)
                SetWindowPos(hwnd,0,rcWindow.left,rcWindow.top,rcWindow.right-rcWindow.left,rcWindow.bottom-rcWindow.top,SWP_NOZORDER|SWP_NOACTIVATE);
            else
            {
                SetWindowPos(hwnd,0,rcWindow.left,rcWindow.top,rcWindow.right-rcWindow.left,rcWindow.bottom-rcWindow.top,SWP_NOZORDER|SWP_NOACTIVATE);
            }
            nRequiredHeight=0;
            return 0;
        }
    case NM_CLICK:
        {	NMCLISTCONTROL *nm=(NMCLISTCONTROL*)lParam;
        DWORD hitFlags;
        HANDLE hItem = (HANDLE)SendMessage(pcli->hwndContactTree,CLM_HITTEST,(WPARAM)&hitFlags,MAKELPARAM(nm->pt.x,nm->pt.y));

        if (hitFlags&CLCHT_ONITEMEXTRA)
        {					
            int v,e,w;
            pdisplayNameCacheEntry pdnce; 
            v=ExtraImage_ExtraIDToColumnNum(EXTRA_ICON_PROTO);
            e=ExtraImage_ExtraIDToColumnNum(EXTRA_ICON_EMAIL);
            w=ExtraImage_ExtraIDToColumnNum(EXTRA_ICON_WEB);

            if (!IsHContactGroup(hItem)&&!IsHContactInfo(hItem))
            {
                pdnce=(pdisplayNameCacheEntry)pcli->pfnGetCacheEntry(nm->hItem);
                if (pdnce==NULL) return 0;

                if(nm->iColumn==v) {
                    CallService(MS_USERINFO_SHOWDIALOG,(WPARAM)nm->hItem,0);
                };
                if(nm->iColumn==e) {
                    //CallService(MS_USERINFO_SHOWDIALOG,(WPARAM)nm->hItem,0);
                    char *email,buf[4096];
                    email=DBGetStringA(nm->hItem,"UserInfo", "Mye-mail0");
                    if (!email)
                        email=DBGetStringA(nm->hItem, pdnce->szProto, "e-mail");																						
                    if (email)
                    {
                        sprintf(buf,"mailto:%s",email);
                        mir_free_and_nill(email);
                        ShellExecuteA(hwnd,"open",buf,NULL,NULL,SW_SHOW);
                    }											
                };	
                if(nm->iColumn==w) {
                    char *homepage;
                    homepage=DBGetStringA(pdnce->hContact,"UserInfo", "Homepage");
                    if (!homepage)
                        homepage=DBGetStringA(pdnce->hContact,pdnce->szProto, "Homepage");
                    if (homepage!=NULL)
                    {											
                        ShellExecuteA(hwnd,"open",homepage,NULL,NULL,SW_SHOW);
                        mir_free_and_nill(homepage);
                    }
                }
            }
        };	
        if(hItem && !(hitFlags&CLCHT_NOWHERE)) break;
        if((hitFlags&(CLCHT_NOWHERE|CLCHT_INLEFTMARGIN|CLCHT_BELOWITEMS))==0) break;
        if (DBGetContactSettingByte(NULL,"CLUI","ClientAreaDrag",SETTING_CLIENTDRAG_DEFAULT)) {
            POINT pt;
            int res;
            pt=nm->pt;
            ClientToScreen(pcli->hwndContactTree,&pt);
            res=PostMessage(hwnd, WM_SYSCOMMAND, SC_MOVE|HTCAPTION,MAKELPARAM(pt.x,pt.y));
            return res;
        }
        /*===================*/
        if (DBGetContactSettingByte(NULL,"CLUI","DragToScroll",0) && !DBGetContactSettingByte(NULL,"CLUI","ClientAreaDrag",1)) 
            return CLC_EnterDragToScroll(pcli->hwndContactTree,nm->pt.y);
        /*===================*/
        return 0;
        }	}	}
        break;

    case WM_CONTEXTMENU:
        {	RECT rc;
        POINT pt;

        pt.x=(short)LOWORD(lParam);
        pt.y=(short)HIWORD(lParam);
        // x/y might be -1 if it was generated by a kb click			
        GetWindowRect(pcli->hwndContactTree,&rc);
        if ( pt.x == -1 && pt.y == -1) {
            // all this is done in screen-coords!
            GetCursorPos(&pt);
            // the mouse isnt near the window, so put it in the middle of the window
            if (!PtInRect(&rc,pt)) {
                pt.x = rc.left + (rc.right - rc.left) / 2;
                pt.y = rc.top + (rc.bottom - rc.top) / 2; 
            }
        }
        if(PtInRect(&rc,pt)) {
            HMENU hMenu;
            hMenu=(HMENU)CallService(MS_CLIST_MENUBUILDGROUP,0,0);
            TrackPopupMenu(hMenu,TPM_TOPALIGN|TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,hwnd,NULL);
            return 0;
        }	}
        return 0;

    case WM_MEASUREITEM:
        if(((LPMEASUREITEMSTRUCT)lParam)->itemData==MENU_STATUSMENU) {
            HDC hdc;
            SIZE textSize;
            hdc=GetDC(hwnd);
            GetTextExtentPoint32A(hdc,Translate("Status"),lstrlenA(Translate("Status")),&textSize);	
            ((LPMEASUREITEMSTRUCT)lParam)->itemWidth=textSize.cx;
            //GetSystemMetrics(SM_CXSMICON)*4/3;
            ((LPMEASUREITEMSTRUCT)lParam)->itemHeight=0;
            ReleaseDC(hwnd,hdc);
            return TRUE;
        }
        break;

    case WM_DRAWITEM:
        {
            struct ClcData * dat=(struct ClcData*)GetWindowLong(pcli->hwndContactTree,0);
            LPDRAWITEMSTRUCT dis=(LPDRAWITEMSTRUCT)lParam;
            if (!dat) return 0;
            //if(dis->hwndItem==pcli->hwndStatus/*&&IsWindowVisible(pcli->hwndStatus)*/) {
            //  //DrawDataForStatusBar(dis);  //possible issue with hangin for some protocol is here
            //  SkinEngine_Service_InvalidateFrameImage((WPARAM)pcli->hwndStatus,0);
            //}

            if (dis->CtlType==ODT_MENU) {
                if (dis->itemData==MENU_MIRANDAMENU) {
                    if (!g_bLayered)
                    {	
                        char buf[255]={0};	
                        short dx=1+(dis->itemState&ODS_SELECTED?1:0)-(dis->itemState&ODS_HOTLIGHT?1:0);
                        HICON hIcon=SkinEngine_ImageList_GetIcon(himlMirandaIcon,0,ILD_NORMAL);
                        CLUI_DrawMenuBackGround(hwnd, dis->hDC, 1);
                        _snprintf(buf,sizeof(buf),"Main,ID=MainMenu,Selected=%s,Hot=%s",(dis->itemState&ODS_SELECTED)?"True":"False",(dis->itemState&ODS_HOTLIGHT)?"True":"False");
                        SkinDrawGlyph(dis->hDC,&dis->rcItem,&dis->rcItem,buf);
                        DrawState(dis->hDC,NULL,NULL,(LPARAM)hIcon,0,(dis->rcItem.right+dis->rcItem.left-GetSystemMetrics(SM_CXSMICON))/2+dx,(dis->rcItem.bottom+dis->rcItem.top-GetSystemMetrics(SM_CYSMICON))/2+dx,0,0,DST_ICON|(dis->itemState&ODS_INACTIVE&&FALSE?DSS_DISABLED:DSS_NORMAL));
                        DestroyIcon_protect(hIcon);         
                        nMirMenuState=dis->itemState;
                    } else {
                        nMirMenuState=dis->itemState;
                        SkinInvalidateFrame(hwnd,NULL,0);
                    }
                    return TRUE;
                }
                else if(dis->itemData==MENU_STATUSMENU) {
                    if (!g_bLayered)
                    {
                        char buf[255]={0};
                        RECT rc=dis->rcItem;
                        short dx=1+(dis->itemState&ODS_SELECTED?1:0)-(dis->itemState&ODS_HOTLIGHT?1:0);
                        if (dx>1){
                            rc.left+=dx;
                            rc.top+=dx;
                        }else if (dx==0){
                            rc.right-=1;
                            rc.bottom-=1;
                        }
                        CLUI_DrawMenuBackGround(hwnd, dis->hDC, 2);
                        SetBkMode(dis->hDC,TRANSPARENT);
                        _snprintf(buf,sizeof(buf),"Main,ID=StatusMenu,Selected=%s,Hot=%s",(dis->itemState&ODS_SELECTED)?"True":"False",(dis->itemState&ODS_HOTLIGHT)?"True":"False");
                        SkinDrawGlyph(dis->hDC,&dis->rcItem,&dis->rcItem,buf);
                        SetTextColor(dis->hDC, (dis->itemState&ODS_SELECTED|dis->itemState&ODS_HOTLIGHT)?dat->MenuTextHiColor:dat->MenuTextColor);
                        DrawText(dis->hDC,TranslateT("Status"), lstrlen(TranslateT("Status")),&rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                        nStatusMenuState=dis->itemState;
                    } else {
                        nStatusMenuState=dis->itemState;
                        SkinInvalidateFrame(hwnd,NULL,0);
                    }
                    return TRUE;
                }

                return CallService(MS_CLIST_MENUDRAWITEM,wParam,lParam);
            }
            return 0;
        }

    case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS * wp;
            wp=(WINDOWPOS *)lParam; 
            if (wp->flags&SWP_HIDEWINDOW && mutex_bAnimationInProgress) 
                return 0;               
            if (g_bOnDesktop)
                wp->flags|=SWP_NOACTIVATE|SWP_NOZORDER;
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    case WM_DESTROY:
        {
            int state=DBGetContactSettingByte(NULL,"CList","State",SETTING_STATE_NORMAL);
            TRACE("CLUI.c: WM_DESTROY\n");
            g_bSTATE=STATE_EXITING;
            CLUI_DisconnectAll();
            TRACE("CLUI.c: WM_DESTROY - WaitThreadsCompletion \n");
            if (CLUI_WaitThreadsCompletion(hwnd)) return 0; //stop all my threads                
            TRACE("CLUI.c: WM_DESTROY - WaitThreadsCompletion DONE\n");
            {
                int i=0;
                for(i=0; i<64; i++)
                    if(CycleStartTick[i].szProto) mir_free_and_nill(CycleStartTick[i].szProto);
            }

            if (state==SETTING_STATE_NORMAL){CLUI_ShowWindowMod(hwnd,SW_HIDE);};
            UnLoadContactListModule();
            if(hSettingChangedHook!=0){UnhookEvent(hSettingChangedHook);};
            if(hAvatarChanged!=0){UnhookEvent(hAvatarChanged);};
            if(hSmileyAddOptionsChangedHook!=0){UnhookEvent(hSmileyAddOptionsChangedHook);};
            if(hIconChangedHook!=0){UnhookEvent(hIconChangedHook);};

            CListTray_TrayIconDestroy(hwnd);	
            mutex_bAnimationInProgress=0;  		
            CallService(MS_CLIST_FRAMES_REMOVEFRAME,(WPARAM)hFrameContactTree,(LPARAM)0);		
            TRACE("CLUI.c: WM_DESTROY - hFrameContactTree removed\n");
            pcli->hwndContactTree=NULL;
            pcli->hwndStatus=NULL;
            {
                if(DBGetContactSettingByte(NULL,"CLUI","AutoSize",0))
                {
                    RECT r;
                    GetWindowRect(pcli->hwndContactList,&r);
                    if(DBGetContactSettingByte(NULL,"CLUI","AutoSizeUpward",0))
                        r.top=r.bottom-CLUIFrames_GetTotalHeight();
                    else 
                        r.bottom=r.top+CLUIFrames_GetTotalHeight();
                    DBWriteContactSettingDword(NULL,"CList","y",r.top);
                    DBWriteContactSettingDword(NULL,"CList","Height",r.bottom-r.top);
                }
            }
            UnLoadCLUIFramesModule();
            TRACE("CLUI.c: WM_DESTROY - UnLoadCLUIFramesModule DONE\n");
            ImageList_Destroy(himlMirandaIcon);
            DBWriteContactSettingByte(NULL,"CList","State",(BYTE)state);
            SkinEngine_UnloadSkin(&g_SkinObjectList);
            FreeLibrary(hUserDll);
            TRACE("CLUI.c: WM_DESTROY - hUserDll freed\n");
            pcli->hwndContactList=NULL;
            pcli->hwndStatus=NULL;
            PostQuitMessage(0);
            TRACE("CLUI.c: WM_DESTROY - PostQuitMessage posted\n");
            TRACE("CLUI.c: WM_DESTROY - ALL DONE\n");
            return 0;
        }	}

    return saveContactListWndProc( hwnd, msg, wParam, lParam );
}

int CLUI_IconsChanged(WPARAM wParam,LPARAM lParam)
{
    if (MirandaExiting()) return 0;
    ImageList_ReplaceIcon(himlMirandaIcon,0,LoadSkinnedIcon(SKINICON_OTHER_MIRANDA));
    DrawMenuBar(pcli->hwndContactList);
    ExtraImage_ReloadExtraIcons();
    ExtraImage_SetAllExtraIcons(pcli->hwndContactTree,0);
    SkinEngine_RedrawCompleteWindow();
    //	pcli->pfnClcBroadcast( INTM_INVALIDATE,0,0);
    return 0;
}


static int CLUI_MenuItem_PreBuild(WPARAM wParam, LPARAM lParam) 
{
    TCHAR cls[128];
    HANDLE hItem;
    HWND hwndClist = GetFocus();
    CLISTMENUITEM mi;
    if (MirandaExiting()) return 0;
    ZeroMemory(&mi,sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.flags = CMIM_FLAGS;
    GetClassName(hwndClist,cls,sizeof(cls));
    hwndClist = (!lstrcmp(CLISTCONTROL_CLASS,cls))?hwndClist:pcli->hwndContactList;
    hItem = (HANDLE)SendMessage(hwndClist,CLM_GETSELECTION,0,0);
    if (!hItem) {
        mi.flags = CMIM_FLAGS | CMIF_HIDDEN;
    }
    CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hRenameMenuItem, (LPARAM)&mi);

    if (!hItem || !IsHContactContact(hItem) || !DBGetContactSettingByte(NULL,"CList","ShowAvatars",0)) 
    {
        mi.flags = CMIM_FLAGS | CMIF_HIDDEN;
        CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hShowAvatarMenuItem, (LPARAM)&mi);
        CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hHideAvatarMenuItem, (LPARAM)&mi);
    }
    else
    {
        int has_avatar;

        if (ServiceExists(MS_AV_GETAVATARBITMAP))
        {
            has_avatar = CallService(MS_AV_GETAVATARBITMAP, (WPARAM)hItem, 0);
        }
        else
        {
            DBVARIANT dbv={0};
            if (DBGetContactSetting(hItem, "ContactPhoto", "File", &dbv))
            {
                has_avatar = 0;
            }
            else
            {
                has_avatar = 1;
                DBFreeVariant(&dbv);
            }
        }

        if (DBGetContactSettingByte(hItem, "CList", "HideContactAvatar", 0))
        {
            mi.flags = CMIM_FLAGS | (has_avatar ? 0 : CMIF_GRAYED);
            CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hShowAvatarMenuItem, (LPARAM)&mi);
            mi.flags = CMIM_FLAGS | CMIF_HIDDEN;
            CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hHideAvatarMenuItem, (LPARAM)&mi);
        }
        else
        {
            mi.flags = CMIM_FLAGS | CMIF_HIDDEN;
            CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hShowAvatarMenuItem, (LPARAM)&mi);
            mi.flags = CMIM_FLAGS | (has_avatar ? 0 : CMIF_GRAYED);
            CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)hHideAvatarMenuItem, (LPARAM)&mi);
        }
    }

    return 0;
}

static int CLUI_MenuItem_ShowContactAvatar(WPARAM wParam,LPARAM lParam)
{
    HANDLE hContact = (HANDLE) wParam;

    DBWriteContactSettingByte(hContact, "CList", "HideContactAvatar", 0);

    pcli->pfnClcBroadcast( INTM_AVATARCHANGED,wParam,0);
    return 0;
}

static int CLUI_MenuItem_HideContactAvatar(WPARAM wParam,LPARAM lParam)
{
    HANDLE hContact = (HANDLE) wParam;

    DBWriteContactSettingByte(hContact, "CList", "HideContactAvatar", 1);

    pcli->pfnClcBroadcast( INTM_AVATARCHANGED,wParam,0);
    return 0;
}

static int CLUI_ShowMainMenu (WPARAM w,LPARAM l)
{
    HMENU hMenu;
    POINT pt;
    hMenu=(HMENU)CallService(MS_CLIST_MENUGETMAIN,0,0);
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu,TPM_TOPALIGN|TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,pcli->hwndContactList,NULL);				
    return 0;
}
static int CLUI_ShowStatusMenu(WPARAM w,LPARAM l)
{
    HMENU hMenu;
    POINT pt;
    hMenu=(HMENU)CallService(MS_CLIST_MENUGETSTATUS,0,0);
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu,TPM_TOPALIGN|TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,pcli->hwndContactList,NULL);				
    return 0;
}


void CLUI_cli_LoadCluiGlobalOpts()
{
    BOOL tLayeredFlag=FALSE;
    tLayeredFlag=IsWinVer2000Plus();
    tLayeredFlag&=DBGetContactSettingByte(NULL, "ModernData", "EnableLayering", tLayeredFlag);

    if(tLayeredFlag)
    {
        if (DBGetContactSettingByte(NULL,"CList","WindowShadow",0)==1)
            DBWriteContactSettingByte(NULL,"CList","WindowShadow",2);    
    }
    else
    {
        if (DBGetContactSettingByte(NULL,"CList","WindowShadow",0)==2)
            DBWriteContactSettingByte(NULL,"CList","WindowShadow",1); 
    }
    saveLoadCluiGlobalOpts();
}

void CLUI_cliOnCreateClc(void)
{
    hFrameContactTree=0;
    CreateServiceFunction(MS_CLIST_GETSTATUSMODE,CListTray_GetGlobalStatus);
    hUserDll = LoadLibrary(TEXT("user32.dll"));
    if (hUserDll)
    {
        g_proc_UpdateLayeredWindow = (BOOL (WINAPI *)(HWND,HDC,POINT*,SIZE*,HDC,POINT*,COLORREF,BLENDFUNCTION*,DWORD))GetProcAddress(hUserDll, "UpdateLayeredWindow");
        g_bLayered=(g_proc_UpdateLayeredWindow!=NULL);
        g_bSmoothAnimation=IsWinVer2000Plus()&&DBGetContactSettingByte(NULL, "CLUI", "FadeInOut", 1);
        g_bLayered=g_bLayered*DBGetContactSettingByte(NULL, "ModernData", "EnableLayering", g_bLayered);
        g_proc_SetLayeredWindowAttributesNew = (BOOL (WINAPI *)(HWND,COLORREF,BYTE,DWORD))GetProcAddress(hUserDll, "SetLayeredWindowAttributes");
        g_proc_AnimateWindow=(BOOL (WINAPI*)(HWND,DWORD,DWORD))GetProcAddress(hUserDll,"AnimateWindow");
    }
    uMsgProcessProfile=RegisterWindowMessage(TEXT("Miranda::ProcessProfile"));

    // Call InitGroup menus before
    GroupMenus_Init();

    HookEvent(ME_SYSTEM_MODULESLOADED,CLUI_ModulesLoaded);
    HookEvent(ME_SKIN_ICONSCHANGED,CLUI_IconsChanged);
    HookEvent(ME_OPT_INITIALISE,CLUIOpt_Init);

    hContactDraggingEvent=CreateHookableEvent(ME_CLUI_CONTACTDRAGGING);
    hContactDroppedEvent=CreateHookableEvent(ME_CLUI_CONTACTDROPPED);
    hContactDragStopEvent=CreateHookableEvent(ME_CLUI_CONTACTDRAGSTOP);
    CLUIServices_LoadModule();
    //TODO Add Row template loading here.

    RowHeight_InitModernRow();

    //CreateServiceFunction("CLUI/GetConnectingIconForProtocol",CLUI_GetConnectingIconService);

    bOldHideOffline=DBGetContactSettingByte(NULL,"CList","HideOffline",SETTING_HIDEOFFLINE_DEFAULT);

    CLUI_PreCreateCLC(pcli->hwndContactList);

    LoadCLUIFramesModule();
    ExtraImage_LoadModule();	

    g_hMenuMain=GetMenu(pcli->hwndContactList);
    CLUI_ChangeWindowMode();

    {	CLISTMENUITEM mi;
    ZeroMemory(&mi,sizeof(mi));
    mi.cbSize=sizeof(mi);
    mi.flags=0;
    mi.pszContactOwner=NULL;    //on every contact
    CreateServiceFunction("CList/ShowContactAvatar",CLUI_MenuItem_ShowContactAvatar);
    mi.position=2000150000;
    mi.hIcon=LoadSmallIcon(g_hInst,MAKEINTRESOURCE(IDI_SHOW_AVATAR));
    mi.pszName=Translate("Show Contact &Avatar");
    mi.pszService="CList/ShowContactAvatar";
    hShowAvatarMenuItem = (HANDLE)CallService(MS_CLIST_ADDCONTACTMENUITEM,0,(LPARAM)&mi);
    DestroyIcon_protect(mi.hIcon);

    CreateServiceFunction("CList/HideContactAvatar",CLUI_MenuItem_HideContactAvatar);
    mi.position=2000150001;
    mi.hIcon=LoadSmallIcon(g_hInst,MAKEINTRESOURCE(IDI_HIDE_AVATAR));
    mi.pszName=Translate("Hide Contact &Avatar");
    mi.pszService="CList/HideContactAvatar";
    hHideAvatarMenuItem = (HANDLE)CallService(MS_CLIST_ADDCONTACTMENUITEM,0,(LPARAM)&mi);
    DestroyIcon_protect(mi.hIcon);
    HookEvent(ME_CLIST_PREBUILDCONTACTMENU, CLUI_MenuItem_PreBuild);
    }

    ProtocolOrder_LoadModule();
    nLastRequiredHeight=0;
    //CreateServiceFunction("TestService",TestService);
    CreateServiceFunction(MS_CLUI_SHOWMAINMENU,CLUI_ShowMainMenu);
    CreateServiceFunction(MS_CLUI_SHOWSTATUSMENU,CLUI_ShowStatusMenu);

    CLUI_ReloadCLUIOptions();
    EventArea_Create(pcli->hwndContactList);
    pcli->hwndStatus=(HWND)StatusBar_Create(pcli->hwndContactList);    
}

int CLUI_TestCursorOnBorders()
{
    HWND hwnd=pcli->hwndContactList;
    HCURSOR hCurs1=NULL;
    RECT r;  
    POINT pt;
    int k=0, t=0, fx,fy;
    HWND hAux;
    BOOL mouse_in_window=0;
    HWND gf=GetForegroundWindow();
    GetCursorPos(&pt);
    hAux = WindowFromPoint(pt);	
    if (CLUI_CheckOwnedByClui(hAux))
    {
        if(g_bTransparentFlag) {
            if (!bTransparentFocus && gf!=hwnd) {
                CLUI_SmoothAlphaTransition(hwnd, DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT), 1);
                //g_proc_SetLayeredWindowAttributes(hwnd, RGB(0,0,0), (BYTE)DBGetContactSettingByte(NULL,"CList","Alpha",SETTING_ALPHA_DEFAULT), LWA_ALPHA);
                bTransparentFocus=1;
                SetTimer(hwnd, TM_AUTOALPHA,250,NULL);
            }
        }
    }


    mutex_bIgnoreActivation=0;
    GetWindowRect(hwnd,&r);
    /*
    *  Size borders offset (contract)
    */
    r.top+=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Top",0);
    r.bottom-=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Bottom",0);
    r.left+=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Left",0);
    r.right-=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Right",0);

    if (r.right<r.left) r.right=r.left;
    if (r.bottom<r.top) r.bottom=r.top;

    /*
    *  End of size borders offset (contract)
    */

    hAux = WindowFromPoint(pt);
    while(hAux != NULL) 
    {
        if (hAux == hwnd) {mouse_in_window=1; break;}
        hAux = GetParent(hAux);
    }
    fx=GetSystemMetrics(SM_CXFULLSCREEN);
    fy=GetSystemMetrics(SM_CYFULLSCREEN);
    if (g_bDocked || g_nBehindEdgeState==0)
        //if (g_bDocked) || ((pt.x<fx-1) && (pt.y<fy-1) && pt.x>1 && pt.y>1)) // workarounds for behind the edge.
    {
        //ScreenToClient(hwnd,&pt);
        //GetClientRect(hwnd,&r);
        if (pt.y<=r.bottom && pt.y>=r.bottom-SIZING_MARGIN && !DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)) k=6;
        else if (pt.y>=r.top && pt.y<=r.top+SIZING_MARGIN && !DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)) k=3;
        if (pt.x<=r.right && pt.x>=r.right-SIZING_MARGIN && g_nBehindEdgeSettings!=2) k+=2;
        else if (pt.x>=r.left && pt.x<=r.left+SIZING_MARGIN && g_nBehindEdgeSettings!=1) k+=1;
        if (!(pt.x>=r.left && pt.x<=r.right && pt.y>=r.top && pt.y<=r.bottom)) k=0;
        k*=mouse_in_window;
        hCurs1 = LoadCursor(NULL, IDC_ARROW);
        if(g_nBehindEdgeState<=0 && (!(DBGetContactSettingByte(NULL,"CLUI","LockSize",0))))
            switch(k)
        {
            case 1: 
            case 2:
                if(!g_bDocked||(g_bDocked==2 && k==1)||(g_bDocked==1 && k==2)){hCurs1 = LoadCursor(NULL, IDC_SIZEWE); break;}
            case 3: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENS); break;}
            case 4: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENWSE); break;}
            case 5: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENESW); break;}
            case 6: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENS); break;}
            case 7: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENESW); break;}
            case 8: if(!g_bDocked) {hCurs1 = LoadCursor(NULL, IDC_SIZENWSE); break;}
        }
        if (hCurs1) SetCursor(hCurs1);
        return k;
    }

    return 0;
}

int CLUI_SizingOnBorder(POINT pt, int PerformSize)
{
    if (!(DBGetContactSettingByte(NULL,"CLUI","LockSize",0)))
    {
        RECT r;
        HWND hwnd=pcli->hwndContactList;
        int k=0;
        GetWindowRect(hwnd,&r);
        /*
        *  Size borders offset (contract)
        */
        r.top+=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Top",0);
        r.bottom-=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Bottom",0);
        r.left+=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Left",0);
        r.right-=DBGetContactSettingDword(NULL,"ModernSkin","SizeMarginOffset_Right",0);

        if (r.right<r.left) r.right=r.left;
        if (r.bottom<r.top) r.bottom=r.top;

        /*
        *  End of size borders offset (contract)
        */
        //ScreenToClient(hwnd,&pt);
        if (pt.y<=r.bottom && pt.y>=r.bottom-SIZING_MARGIN && !DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)) k=6;
        else if (pt.y>=r.top && pt.y<=r.top+SIZING_MARGIN && !DBGetContactSettingByte(NULL,"CLUI","AutoSize",0)) k=3;
        if (pt.x<=r.right && pt.x>=r.right-SIZING_MARGIN) k+=2;
        else if (pt.x>=r.left && pt.x<=r.left+SIZING_MARGIN)k+=1;
        if (!(pt.x>=r.left && pt.x<=r.right && pt.y>=r.top && pt.y<=r.bottom)) k=0;
        //ClientToScreen(hwnd,&pt);
        if (k && PerformSize) 
        { 
            ReleaseCapture();
            SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE+k,MAKELPARAM(pt.x,pt.y));					
            return k;
        }
        else return k;
    }
    return 0;
}

static void CLUI_SmoothAnimationThreadProc(HWND hwnd)
{
    //  return;
    if (!mutex_bAnimationInProgress) 
    {
        g_hSmoothAnimationThreadID=0;
        return;  /// Should be some locked to avoid painting against contact deletion.
    }
    do
    {
        if (!g_mutex_bLockUpdating)
        {
			if (!MirandaExiting())
				SendMessage(hwnd,UM_CALLSYNCRONIZED, (WPARAM)SYNC_SMOOTHANIMATION,(LPARAM) hwnd);       
            SleepEx(20,TRUE);
            if (MirandaExiting()) 
            {
                g_hSmoothAnimationThreadID=0;
                return;
            }
        }
        else SleepEx(0,TRUE);

    } while (mutex_bAnimationInProgress);
    g_hSmoothAnimationThreadID=0;
    return;
}


static int CLUI_SmoothAlphaThreadTransition(HWND hwnd)
{
    int step;
    int a;

    step=(g_bCurrentAlpha>bAlphaEnd)?-1*ANIMATION_STEP:ANIMATION_STEP;
    a=g_bCurrentAlpha+step;
    if ((step>=0 && a>=bAlphaEnd) || (step<=0 && a<=bAlphaEnd))
    {
        mutex_bAnimationInProgress=0;
        g_bCurrentAlpha=bAlphaEnd;
        if (g_bCurrentAlpha==0)
        {			
            g_bCurrentAlpha=1;
            SkinEngine_JustUpdateWindowImage();
            mutex_bShowHideCalledFromAnimation=1;             
            CLUI_ShowWindowMod(pcli->hwndContactList,0);
            CLUIFrames_OnShowHide(hwnd,0);
            mutex_bShowHideCalledFromAnimation=0;
            g_bCurrentAlpha=0;
            if (!g_bLayered) RedrawWindow(pcli->hwndContactList,NULL,NULL,RDW_ERASE|RDW_FRAME);
            return 0;			
        }
    }
    else   g_bCurrentAlpha=a;   
    SkinEngine_JustUpdateWindowImage();     
    return 1;
}

int CLUI_SmoothAlphaTransition(HWND hwnd, BYTE GoalAlpha, BOOL wParam)
{ 

    if ((!g_bLayered
        && (!g_bSmoothAnimation && !g_bTransparentFlag))||!g_proc_SetLayeredWindowAttributesNew)
    {
        if (GoalAlpha>0 && wParam!=2)
        {
            if (!IsWindowVisible(hwnd))
            {
                mutex_bShowHideCalledFromAnimation=1;
                CLUI_ShowWindowMod(pcli->hwndContactList,SW_RESTORE);
                CLUIFrames_OnShowHide(hwnd,1);
                mutex_bShowHideCalledFromAnimation=0;
                g_bCurrentAlpha=GoalAlpha;
                SkinEngine_UpdateWindowImage();

            }
        }
        else if (GoalAlpha==0 && wParam!=2)
        {
            if (IsWindowVisible(hwnd))
            {
                mutex_bShowHideCalledFromAnimation=1;
                CLUI_ShowWindowMod(pcli->hwndContactList,0);
                CLUIFrames_OnShowHide(hwnd,0);
                g_bCurrentAlpha=GoalAlpha;
                mutex_bShowHideCalledFromAnimation=0;

            }
        }
        return 0;
    }
    if (g_bCurrentAlpha==GoalAlpha &&0)
    {
        if (mutex_bAnimationInProgress)
        {
            KillTimer(hwnd,TM_SMOTHALPHATRANSITION);
            mutex_bAnimationInProgress=0;
        }
        return 0;
    }
    if (mutex_bShowHideCalledFromAnimation) return 0;
    if (wParam!=2)  //not from timer
    {   
        bAlphaEnd=GoalAlpha;
        if (!mutex_bAnimationInProgress)
        {
            if ((!IsWindowVisible(hwnd)||g_bCurrentAlpha==0) && bAlphaEnd>0 )
            {
                mutex_bShowHideCalledFromAnimation=1;            
                CLUI_ShowWindowMod(pcli->hwndContactList,SW_SHOWNA);			 
                CLUIFrames_OnShowHide(hwnd,SW_SHOW);
                mutex_bShowHideCalledFromAnimation=0;
                g_bCurrentAlpha=1;
                SkinEngine_UpdateWindowImage();
            }
            if (IsWindowVisible(hwnd) && !g_hSmoothAnimationThreadID)
            {
                mutex_bAnimationInProgress=1;
                if (g_bSmoothAnimation)
                    g_hSmoothAnimationThreadID=(DWORD)forkthread(CLUI_SmoothAnimationThreadProc,0,pcli->hwndContactList);	

            }
        }
    }

    {
        int step;
        int a;
        step=(g_bCurrentAlpha>bAlphaEnd)?-1*ANIMATION_STEP:ANIMATION_STEP;
        a=g_bCurrentAlpha+step;
        if ((step>=0 && a>=bAlphaEnd) || (step<=0 && a<=bAlphaEnd) || g_bCurrentAlpha==bAlphaEnd || !g_bSmoothAnimation) //stop animation;
        {
            KillTimer(hwnd,TM_SMOTHALPHATRANSITION);
            mutex_bAnimationInProgress=0;
            if (bAlphaEnd==0) 
            {
                g_bCurrentAlpha=1;
                SkinEngine_UpdateWindowImage();
                mutex_bShowHideCalledFromAnimation=1;             
                CLUI_ShowWindowMod(pcli->hwndContactList,0);
                CLUIFrames_OnShowHide(pcli->hwndContactList,0);
                mutex_bShowHideCalledFromAnimation=0;
                g_bCurrentAlpha=0;
            }
            else
            {
                g_bCurrentAlpha=bAlphaEnd;
                SkinEngine_UpdateWindowImage();
            }
        }
        else
        {
            g_bCurrentAlpha=a;
            SkinEngine_UpdateWindowImage();
        }
    }

    return 0;
}

BOOL CLUI__cliInvalidateRect(HWND hWnd, CONST RECT* lpRect,BOOL bErase )
{
    if (g_mutex_bSetAllExtraIconsCycle)
        return FALSE;
    if (CLUI_IsInMainWindow(hWnd) && g_bLayered)// && IsWindowVisible(hWnd))
    {
        if (IsWindowVisible(hWnd))
            return SkinInvalidateFrame(hWnd,lpRect,bErase);
        else
        {
            g_flag_bFullRepaint=1;
            return 0;
        }
    }
    else return InvalidateRect(hWnd,lpRect,bErase);
    return 1;
}

