#include "clist.h"
#include "CLUIFRAMES\cluiframes.h"

#define SKIN                "ModernSkin"


//Global variables
extern BOOL gl_b_GDIPlusFail;
extern BOOL g_mutex_bOnEdgeSizing;

extern HANDLE g_hSkinLoadedEvent;         


extern wndFrame *Frames;
extern int nFramescount;
//extern struct LISTMODERNMASK *MainModernMaskList;
extern RECT g_rcEdgeSizingRect;
extern int g_nBehindEdgeState;
extern int g_nBehindEdgeSettings;
extern int sortBy[3];
extern tPaintCallbackProc CLC_PaintCallbackProc(HWND hWnd, HDC hDC, RECT * rcPaint, HRGN rgn, DWORD dFlags, void * CallBackData);
extern CLIST_INTERFACE * pcli;
extern BOOL g_mutex_bLockImageUpdating;
extern ClcProtoStatus *clcProto;
extern HIMAGELIST himlCListClc;
extern HIMAGELIST hCListImages;

//External functions 

int SkinEngine_BltBackImage (HWND destHWND, HDC destDC, RECT * BltClientRect);
BOOL CLUI__cliInvalidateRect(HWND hWnd, CONST RECT* lpRect,BOOL bErase );
int GetProtocolVisibility(char * ProtoName);
int CLUI_GetConnectingIconService(WPARAM wParam,LPARAM lParam);
BOOL SkinEngine_TextOut(HDC hdc, int x, int y, LPCTSTR lpString, int nCount);
BOOL SkinEngine_TextOutA(HDC hdc, int x, int y, char * lpString, int nCount);
BOOL SkinEngine_DrawTextA(HDC hdc, char * lpString, int nCount, RECT * lpRect, UINT format);
BOOL SkinEngine_DrawText(HDC hdc, LPCTSTR lpString, int nCount, RECT * lpRect, UINT format);
int SkinEngine_Service_InvalidateFrameImage(WPARAM wParam, LPARAM lParam);       // Post request for updating
int CLUI_UpdateTimer(BYTE BringIn);
int CLUI_TestCursorOnBorders();
int CLUIServices_ProtocolStatusChanged(WPARAM wParam,LPARAM lParam);
void DrawAvatarImageWithGDIp(HDC hDestDC,int x, int y, DWORD width, DWORD height, HBITMAP hbmp, int x1, int y1, DWORD width1, DWORD height1,DWORD flag,BYTE alpha);
void TextOutWithGDIp(HDC hDestDC, int x, int y, LPCTSTR lpString, int nCount);
void InitGdiPlus(void);
void ShutdownGdiPlus(void);
BOOL _inline wildcmp(char * name, char * mask, BYTE option);
BOOL wildcmpi(char * name, char * mask);

/* Global procedures */

HBITMAP SkinEngine_CreateDIB32(int cx, int cy);
HBITMAP SkinEngine_CreateDIB32Point(int cx, int cy, void ** bits);
BOOL    SkinEngine_DrawIconEx(HDC hdc,int xLeft,int yTop,HICON hIcon,int cxWidth,int cyWidth, UINT istepIfAniCur, HBRUSH hbrFlickerFreeDraw, UINT diFlags);
int     SkinEngine_DrawNonFramedObjects(BOOL Erase,RECT *r);
int     SkinEngine_JustUpdateWindowImageRect(RECT * rty);
HBITMAP SkinEngine_LoadGlyphImage(char * szFileName);
int     SkinEngine_LoadModule();
int     SkinEngine_ReCreateBackImage(BOOL Erase,RECT *w);
BOOL    SkinEngine_SetRectOpaque(HDC memdc,RECT *fr);
int     SkinEngine_UnloadGlyphImage(HBITMAP hbmp);
int     SkinEngine_UnloadModule();
int     SkinEngine_UpdateWindowImage();
int     SkinEngine_UpdateWindowImageRect(RECT * lpRect);
int     SkinEngine_ValidateFrameImageProc(RECT * r);

int     SkinEngine_Service_InvalidateFrameImage(WPARAM wParam, LPARAM lParam);
int     SkinEngine_Service_UpdateFrameImage(WPARAM wParam, LPARAM lParam);

BOOL MatchMask(char * name, char * mask);
void SkinEngine_ApplyTransluency();
int ImageList_ReplaceIcon_FixAlpha(HIMAGELIST himl, int i, HICON hicon);
int CLUI_SizingGetWindowRect(HWND hwnd,RECT * rc);
int SkinEngine_UpdateWindowImageRect(RECT * rty);
char * GetParamN(char * string, char * buf, int buflen, BYTE paramN, char Delim, BOOL SkipSpaces);
int SkinEngine_GetSkinFolder(char * szFileName, char * t2);
BOOL SkinEngine_AlphaBlend(HDC hdcDest,int nXOriginDest,int nYOriginDest,int nWidthDest,int nHeightDest,HDC hdcSrc,int nXOriginSrc,int nYOriginSrc,int nWidthSrc,int nHeightSrc,BLENDFUNCTION blendFunction);
int GetProtoIndexByPos(PROTOCOLDESCRIPTOR ** proto, int protoCnt, int Pos);
int CLUI_ShowFromBehindEdge();
int CLUI_UpdateTimer(BYTE BringIn);
int ClcProtoAck(WPARAM wParam,LPARAM lParam);
BOOL SkinEngine_SetRectOpaque(HDC memdc,RECT *fr);
int cliShowHide(WPARAM wParam,LPARAM lParam);
int CLUIFrames_OnShowHide(HWND hwnd, int mode);
int CLUI_ShowFromBehindEdge();
int CLUI_HideBehindEdge();
int LoadStatusBarData();

int CLUI_ReloadCLUIOptions();
wndFrame * FindFrameByItsHWND(HWND FrameHwnd);
//int CallTest(HDC hdc, int x, int y, char * Text);
BOOL SkinEngine_DrawIconEx(HDC hdc,int xLeft,int yTop,HICON hIcon,int cxWidth,int cyWidth, UINT istepIfAniCur, HBRUSH hbrFlickerFreeDraw, UINT diFlags);
int StartGDIPlus();
int TerminateGDIPlus();
BOOL SkinEngine_SetRectOpaque(HDC memdc,RECT *fr);
int DrawTitleBar(HDC hdcMem2,RECT rect,int Frameid);
int SkinEngine_Service_UpdateFrameImage(WPARAM wParam, LPARAM lParam);
int CLUI_OnSkinLoad(WPARAM wParam, LPARAM lParam);
_inline DWORD mod_CalcHash(const char * a);
int  QueueAllFramesUpdating (BYTE);
int  SetAlpha(BYTE);
int  DeleteButtons();
int RegisterButtonByParce(char * ObjectName, char * Params);
int RedrawButtons(HDC hdc);
int SizeFramesByWindowRect(RECT *r, HDWP * PosBatch, int mode);
int SkinEngine_PrepeareImageButDontUpdateIt(RECT * r);
int SkinEngine_UpdateWindowImageRect(RECT * r);
int CheckFramesPos(RECT *wr);
BOOL SkinEngine_SetRgnOpaque(HDC memdc,HRGN hrgn);
int ModernButton_ReposButtons(HWND parent, BOOL draw, RECT * r);
char *DBGetStringA(HANDLE hContact,const char *szModule,const char *szSetting);
BOOL SkinEngine_ImageList_DrawEx( HIMAGELIST himl,int i,HDC hdcDst,int x,int y,int dx,int dy,COLORREF rgbBk,COLORREF rgbFg,UINT fStyle);
int CLUIFrames_OnMoving(HWND hwnd,RECT *lParam);
int CLUIFrames_OnClistResize_mod(WPARAM wParam,LPARAM lParam, int mode);
BOOL CLUI__cliInvalidateRect(HWND hWnd, CONST RECT* lpRect,BOOL bErase );
int BgStatusBarChange(WPARAM wParam,LPARAM lParam);
int CLUI_IsInMainWindow(HWND hwnd);
int BgClcChange(WPARAM wParam,LPARAM lParam);
int BgMenuChange(WPARAM wParam,LPARAM lParam);
int OnFrameTitleBarBackgroundChange(WPARAM wParam,LPARAM lParam);
int SkinEngine_Service_UpdateFrameImage(WPARAM /*hWnd*/, LPARAM/*sPaintRequest*/);
int SkinEngine_Service_InvalidateFrameImage(WPARAM wParam, LPARAM lParam);
//void CacheContactAvatar(struct ClcData *dat, struct ClcContact *contact, BOOL changed);
int GetStatusForContact(HANDLE hContact,char *szProto);
int GetContactIconC(pdisplayNameCacheEntry cacheEntry);
int GetContactIcon(WPARAM wParam,LPARAM lParam);
int CLUI_TestCursorOnBorders();
int CLUI_SizingOnBorder(POINT ,int);

void InvalidateDNCEbyPointer(HANDLE hContact,pdisplayNameCacheEntry pdnce,int SettingType);
int InitCListEvents(void);
void UninitCListEvents(void);
int CLUI_IsInMainWindow(HWND hwnd);
//external functions

//INTERFACES
BOOL	CLUI__cliInvalidateRect(HWND hWnd, CONST RECT* lpRect,BOOL bErase );
int		cliCompareContacts(const struct ClcContact *contact1,const struct ClcContact *contact2);
int		cliCListTrayNotify(MIRANDASYSTRAYNOTIFY *msn);
int		cliFindItem(HWND hwnd,struct ClcData *dat,HANDLE hItem,struct ClcContact **contact,struct ClcGroup **subgroup,int *isVisible);
void	cliTrayIconUpdateBase(char *szChangedProto);
void	cliTrayIconSetToBase(char *szPreferredProto);
void	cliTrayIconIconsChanged(void);
void	cliCluiProtocolStatusChanged(int status,const unsigned char * proto);
HMENU	cliBuildGroupPopupMenu(struct ClcGroup *group);
void	cliInvalidateDisplayNameCacheEntry(HANDLE hContact);
void	cliCheckCacheItem(pdisplayNameCacheEntry pdnce);
ClcCacheEntryBase* cliGetCacheEntry(HANDLE hContact);
void	cli_SaveStateAndRebuildList(HWND hwnd, struct ClcData *dat);
void	CLUI_cli_LoadCluiGlobalOpts(void);
int		cli_TrayIconProcessMessage(WPARAM wParam,LPARAM lParam);
struct ClcContact* cliCreateClcContact( void );
ClcCacheEntryBase* cliCreateCacheItem(HANDLE hContact);

extern struct ClcGroup* ( *saveAddGroup )(HWND hwnd,struct ClcData *dat,const TCHAR *szName,DWORD flags,int groupId,int calcTotalMembers);
extern int ( *saveAddItemToGroup )( struct ClcGroup *group, int iAboveItem );
extern int ( *saveAddInfoItemToGroup )(struct ClcGroup *group,int flags,const TCHAR *pszText);
extern void ( *saveDeleteItemFromTree )(HWND hwnd, HANDLE hItem);
extern void ( *saveFreeContact )( struct ClcContact* );
extern void ( *saveFreeGroup )( struct ClcGroup* );
extern LRESULT ( CALLBACK *saveContactListControlWndProc )(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern char* (*saveGetGroupCountsText)(struct ClcData *dat, struct ClcContact *contact);
extern int  (*saveIconFromStatusMode)(const char *szProto,int nStatus, HANDLE hContact);

int LoadMoveToGroup();
int GetContactIcon(WPARAM wParam,LPARAM lParam);
TCHAR *SkinEngine_ParseText(TCHAR *stzText);
int  ModernButton_LoadModule();
void UnloadAvatarOverlayIcon();
void FreeRowCell ();
void UninitCustomMenus(void);
extern HICON SkinEngine_ImageList_GetIcon(HIMAGELIST himl, int i, UINT fStyle);
int CreateTabPage(char *Group, char * Title, WPARAM wParam, DLGPROC DlgProcOpts);
BOOL CALLBACK DlgProcTabbedOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int LoadAdvancedIcons(int ID);
int GetTrasportStatusIconIndex(int ID, int Status);
HICON CLUI_LoadIconFromExternalFile(char *filename,int i,boolean UseLibrary,boolean registerit,char *IconName,char *SectName,char *Description,int internalidx, BOOL * needFree);
int GetTransportProtoIDFromHCONTACT(HANDLE hContact, char * protocol);
#ifndef _COMMONPROTOTYPES
#define _COMMONPROTOTYPES
typedef struct _TabItemOptionConf 
{ 
	TCHAR *name;			// Tab name
	int id;					// Dialog id
	DLGPROC wnd_proc;		// Dialog function
} TabItemOptionConf;
#endif

