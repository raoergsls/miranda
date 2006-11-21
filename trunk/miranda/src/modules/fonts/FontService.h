/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2006 Miranda ICQ/IM project, Scott Ellis (mail@scottellis.com.au)
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

#include "m_fontservice.h"

// settings to be used for the value of 'deffontsettings' in the FontID structure below - i.e. defaults
typedef struct TFontSettings_tag
{
    COLORREF colour;
    char     size;
    BYTE     style;					// see the DBFONTF_* flags above
    BYTE     charset;
    TCHAR    szFace[LF_FACESIZE];
}
	TFontSettings;

// a font identifier structure - used for registering a font, and getting one out again
typedef struct TFontID_tag {
	int   cbSize;
	TCHAR group[64];               // group the font belongs to - this is the 'Font Group' list in the options page
	TCHAR name[64];                // this is the name of the font setting - e.g. 'contacts' in the 'contact list' group
	char  dbSettingsGroup[32];     // the 'module' in the database where the font data is stored
	char  prefix[32];              // this is prepended to the settings used to store this font's data in the db
	DWORD flags;                   // bitwise OR of the FIDF_* flags above
	TFontSettings deffontsettings; // defaults, valid if flags & FIDF_DEFAULTVALID
	int   order;                   // controls the order in the font group in which the fonts are listed in the UI (if order fields are equal,
                                  // they will be ordered alphabetically by name)
	TFontSettings value;
}
	TFontID;

typedef struct TColourID_tag {
	int      cbSize;
	TCHAR    group[64];
	TCHAR    name[64];
	char     dbSettingsGroup[32];
	char     setting[32];
	DWORD    flags;
	COLORREF defcolour;
	int      order;

	COLORREF value;
}
	TColourID;

/////////////////////////////////////////////////////////////////////////////////////////
// global data & functions

typedef struct
{
	TFontID**   items;
	int count,  limit, increment;
	FSortFunc	sortFunc;
}
	FontIdList;

typedef struct
{
	TColourID** items;
	int count,  limit, increment;
	FSortFunc	sortFunc;
}
	ColourIdList;

extern FontIdList   font_id_list;
extern ColourIdList colour_id_list;

extern int code_page;
extern HANDLE hFontReloadEvent, hColourReloadEvent;

int  CreateFromFontSettings(TFontSettings *fs, LOGFONT *lf, DWORD flags);
void DestroyList( SortedList* list );
