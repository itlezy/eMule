//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "TitledMenu.h"
#include "emule.h"
#include "preferences.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MP_TITLE	0xFFFEu
#define ICONSIZE	16

CTitledMenu::CTitledMenu()
	: m_clRight(::GetSysColor(COLOR_GRADIENTACTIVECAPTION))
	, m_clLeft(::GetSysColor(COLOR_ACTIVECAPTION))
	, m_clText(::GetSysColor(COLOR_CAPTIONTEXT))
	, m_uEdgeFlags(BDR_SUNKENINNER)
	, m_bDrawEdge()
	, m_bIconMenu()
{
	m_mapMenuIdToIconIdx.InitHashTable(29);
}

CTitledMenu::~CTitledMenu()
{
	DeleteIcons();
}

void CTitledMenu::DeleteIcons()
{
	m_ImageList.DeleteImageList();
	m_mapIconNameToIconIdx.RemoveAll();
	m_mapMenuIdToIconIdx.RemoveAll();

	CString strKey;
	for (POSITION pos = m_mapIconNameToBitmap.GetStartPosition(); pos != NULL;) {
		void *pvBmp;
		m_mapIconNameToBitmap.GetNextAssoc(pos, strKey, pvBmp);
		VERIFY(::DeleteObject((HBITMAP)pvBmp));
	}
	m_mapIconNameToBitmap.RemoveAll();
}

BOOL CTitledMenu::CreateMenu()
{
	ASSERT(m_mapIconNameToIconIdx.IsEmpty());
	ASSERT(m_mapMenuIdToIconIdx.IsEmpty());
	ASSERT(m_ImageList.m_hImageList == NULL || m_ImageList.GetImageCount() == 0);
	ASSERT(m_mapIconNameToBitmap.IsEmpty());
	return __super::CreateMenu();
}

BOOL CTitledMenu::DestroyMenu()
{
	BOOL bResult = __super::DestroyMenu();
	DeleteIcons();
	return bResult;
}

void CTitledMenu::AddMenuTitle(LPCTSTR lpszTitle, bool bIsIconMenu)
{
	// insert an empty owner-draw item at top to serve as the title
	// note: item is not selectable (disabled) but not grayed
	//
	// Vista: Adding at least one MF_OWNERDRAW item would render the entire menu in owner drawn mode,
	// and it would be quite expensive to get the native Vista menu styles back. We would need to draw
	// the entire menu with the Vista theme API -- no way. Thus, there is no title for context menus
	// under Vista - the title doesn't fit to the native Vista menu style anyway.
	if (lpszTitle != NULL && !theApp.IsModernThemedControlsActive()) {
		m_strTitle = lpszTitle;
		m_strTitle.Remove(_T('&'));
		CMenu::InsertMenu(0, MF_BYPOSITION | MF_OWNERDRAW | MF_STRING | MF_DISABLED, MP_TITLE);
	}
	if (bIsIconMenu)
		EnableIcons();
}

void CTitledMenu::EnableIcons()
{
	m_bIconMenu = true;
	m_ImageList.DeleteImageList();
	m_ImageList.Create(ICONSIZE, ICONSIZE, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);

	MENUINFO mi;
	mi.cbSize = (DWORD)sizeof mi;
	mi.fMask = MIM_STYLE;
	GetMenuInfo(&mi);
	mi.dwStyle |= MNS_CHECKORBMP;
	SetMenuInfo(&mi);
}

// NOTE: This function is no longer used for Vista!
void CTitledMenu::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	if (lpMIS->itemID == MP_TITLE) {
		CDC dc;
		dc.Attach(::GetDC(HWND_DESKTOP));
		HFONT hfontOld = (HFONT)::SelectObject(dc.m_hDC, (HFONT)theApp.m_fontDefaultBold);
		CSize size = dc.GetTextExtent(m_strTitle);
		::SelectObject(dc.m_hDC, hfontOld);
		size.cx += ::GetSystemMetrics(SM_CXMENUCHECK) + 8;
		::ReleaseDC(NULL, dc.Detach());

		static const int nBorderSize = 2;
		lpMIS->itemWidth = size.cx + nBorderSize;
		lpMIS->itemHeight = size.cy + nBorderSize;
	} else {
		CMenu::MeasureItem(lpMIS);
		if (m_bIconMenu) {
			lpMIS->itemHeight = max(lpMIS->itemHeight, 16);
			lpMIS->itemWidth += 18;
		}
	}
}

// NOTE: This function is no longer used for Vista!
void CTitledMenu::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	if (lpDIS->itemID == MP_TITLE) {
		COLORREF crOldBk = ::SetBkColor(lpDIS->hDC, m_clLeft);

		if (!g_bLowColorDesktop &&/* m_pfnGradientFill &&*/ m_clLeft != m_clRight) {
			TRIVERTEX rcVertex[2];
			--lpDIS->rcItem.right; // exclude this point, like FillRect does
			--lpDIS->rcItem.bottom;
			rcVertex[0].x = lpDIS->rcItem.left;
			rcVertex[0].y = lpDIS->rcItem.top;
			rcVertex[0].Red = GetRValue(m_clLeft) << 8;	// color values from 0x0000 to 0xff00 !!!!
			rcVertex[0].Green = GetGValue(m_clLeft) << 8;
			rcVertex[0].Blue = GetBValue(m_clLeft) << 8;
			rcVertex[0].Alpha = 0;
			rcVertex[1].x = lpDIS->rcItem.right;
			rcVertex[1].y = lpDIS->rcItem.bottom;
			rcVertex[1].Red = GetRValue(m_clRight) << 8;
			rcVertex[1].Green = GetGValue(m_clRight) << 8;
			rcVertex[1].Blue = GetBValue(m_clRight) << 8;
			rcVertex[1].Alpha = 0;
			static const GRADIENT_RECT rect{0, 1};
			::GradientFill(lpDIS->hDC, rcVertex, 2, (PVOID)&rect, 1, GRADIENT_FILL_RECT_H);
		} else
			::ExtTextOut(lpDIS->hDC, 0, 0, ETO_OPAQUE, &lpDIS->rcItem, NULL, 0, NULL);

		if (m_bDrawEdge)
			::DrawEdge(lpDIS->hDC, &lpDIS->rcItem, m_uEdgeFlags, BF_RECT);

		int iOldBkMode = ::SetBkMode(lpDIS->hDC, TRANSPARENT);
		COLORREF crOld = ::SetTextColor(lpDIS->hDC, m_clText);
		HFONT hfontOld = (HFONT)::SelectObject(lpDIS->hDC, (HFONT)theApp.m_fontDefaultBold);
		lpDIS->rcItem.left += ::GetSystemMetrics(SM_CXMENUCHECK) + 8;
		::DrawText(lpDIS->hDC, m_strTitle, -1, &lpDIS->rcItem, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
		::SelectObject(lpDIS->hDC, hfontOld);
		::SetTextColor(lpDIS->hDC, crOld);
		::SetBkMode(lpDIS->hDC, iOldBkMode);
		::SetBkColor(lpDIS->hDC, crOldBk);
	} else {
		int nIconPos;
		if (m_mapMenuIdToIconIdx.Lookup(lpDIS->itemID, nIconPos)) {
			int posY = lpDIS->rcItem.top + ((lpDIS->rcItem.bottom - lpDIS->rcItem.top) - ICONSIZE) / 2;
			CDC *dc = CDC::FromHandle(lpDIS->hDC);
			HICON hIcon = (lpDIS->itemState & ODS_GRAYED) ? ReplaceIconGreyedInImageList(m_ImageList, nIconPos) : 0;
			// Draw the bitmap on the menu.
			m_ImageList.Draw(dc, nIconPos, CPoint(lpDIS->rcItem.left, posY), ILD_TRANSPARENT);
			if (hIcon) { //restore coloured icon
				m_ImageList.Replace(nIconPos, hIcon);
				::DestroyIcon(hIcon);
			}
		}
	}
}

BOOL CTitledMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	BOOL bResult = CMenu::AppendMenu(nFlags, nIDNewItem, lpszNewItem);
	if (bResult)
		SetMenuBitmap(nFlags, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

BOOL CTitledMenu::InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	BOOL bResult = CMenu::InsertMenu(nPosition, nFlags, nIDNewItem, lpszNewItem);
	if (bResult)
		SetMenuBitmap(nFlags, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

BOOL CTitledMenu::RenameMenu(UINT_PTR nIDNewItem, UINT nFlags, LPCTSTR lpszNewItem, LPCTSTR lpszIconName)
{
	MENUITEMINFO mi = {};
	mi.cbSize = (UINT)sizeof(MENUITEMINFO);
	mi.fMask = MIIM_TYPE;
	mi.fType = MFT_STRING;
	mi.dwTypeData = const_cast<LPTSTR>(lpszNewItem);
	BOOL bResult = SetMenuItemInfo((UINT)nIDNewItem, &mi, nFlags == MF_BYPOSITION);
	if (bResult)
		SetMenuBitmap(0, (UINT)nIDNewItem, lpszNewItem, lpszIconName);
	return bResult;
}

static HBITMAP Create32BitHBITMAP(HDC hdc, int cx, int cy, void **ppvBits = NULL)
{
	HBITMAP hBmp = NULL;
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = cx;
	bmi.bmiHeader.biHeight = cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC hdcUsed = hdc ? hdc : GetDC(NULL);
	if (hdcUsed) {
		hBmp = ::CreateDIBSection(hdcUsed, &bmi, DIB_RGB_COLORS, ppvBits, NULL, 0);
		if (hdc != hdcUsed)
			::ReleaseDC(NULL, hdcUsed);
	}
	return hBmp;
}

static HBITMAP IconToBitmap32(HICON hIcon, int cx, int cy)
{
	HBITMAP hBmp = NULL;
	HDC hdcDest = ::CreateCompatibleDC(NULL);
	if (hdcDest) {
		hBmp = ::Create32BitHBITMAP(hdcDest, cx, cy);
		if (hBmp) {
			HBITMAP hbmpOld = (HBITMAP)::SelectObject(hdcDest, hBmp);
			if (hbmpOld) {
				// "DrawIconEx" works well only for icons which do also have an XP version specified.
				// For 256 color icons the icons drawn by "DrawIconEx" are way too "bright"?
				//::DrawIconEx(hdcDest, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

				// Not as efficient as "DrawIconEx", but using an image list works for XP icons
				// as well as for 256 color icons.
				HIMAGELIST himl = ::ImageList_Create(cx, cy, theApp.m_iDfltImageListColorFlags | ILC_MASK, 1, 0);
				if (himl) {
					::ImageList_AddIcon(himl, hIcon);
					::ImageList_Draw(himl, 0, hdcDest, 0, 0, ILD_NORMAL);
					::ImageList_Destroy(himl);
				}

				::SelectObject(hdcDest, hbmpOld);
			}
		}
		::DeleteDC(hdcDest);
	}
	return hBmp;
}

void CTitledMenu::SetMenuBitmap(UINT nFlags, UINT nIDNewItem, LPCTSTR /*lpszNewItem*/, LPCTSTR lpszIconName)
{
	if (!m_bIconMenu || (nFlags & MF_SEPARATOR) != 0) {
		if (m_bIconMenu && lpszIconName != NULL)
			ASSERT(0);
		return;
	}

	// Those MFC warnings which are thrown when one opens certain context menus
	// are because of sub menu items. All the IDs shown in the warnings are sub
	// menu handles! Seems to be a bug in MFC. Look at '_AfxFindPopupMenuFromID'.
	// ---
	// Warning: unknown WM_MEASUREITEM for menu item 0x530601.
	// Warning: unknown WM_MEASUREITEM for menu item 0x4305E7.
	// ---
	//if (nFlags & MF_POPUP)
	//	TRACE(_T("TitledMenu: adding popup menu item id=%x  str=%s\n"), nIDNewItem, lpszNewItem);

	CString strIconLower(lpszIconName);
	if (strIconLower.MakeLower().IsEmpty())
		return;
	// Use the Windows built-in feature for 32-bit menu item bitmaps.
	// 'MeasureItem' and 'DrawItem' no longer get called for this path.
	HBITMAP hBmp;
	void *pvBmp;
	if (m_mapIconNameToBitmap.Lookup(strIconLower, pvBmp))
		hBmp = (HBITMAP)pvBmp;
	else {
		HICON hIcon = theApp.LoadIcon(strIconLower);
		if (hIcon) {
			hBmp = IconToBitmap32(hIcon, ICONSIZE, ICONSIZE);
			VERIFY(::DestroyIcon(hIcon));
		} else
			hBmp = NULL;
	}

	if (hBmp) {
		MENUITEMINFO info = {};
		info.cbSize = (UINT)sizeof info;
		info.fMask = MIIM_BITMAP;
		info.hbmpItem = hBmp;
		VERIFY(SetMenuItemInfo(nIDNewItem, &info, FALSE));
		m_mapIconNameToBitmap[strIconLower] = hBmp;
	}
}

bool CTitledMenu::HasEnabledItems() const
{
	for (int i = GetMenuItemCount(); --i >= 0;)
		if ((GetMenuState((UINT)i, MF_BYPOSITION) & (MF_DISABLED | MF_SEPARATOR | MF_GRAYED)) == 0)
			return true;
	return false;
}
