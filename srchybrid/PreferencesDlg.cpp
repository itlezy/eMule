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
#include "emule.h"
#include "PreferencesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPreferencesDlg, CTreePropSheet)

BEGIN_MESSAGE_MAP(CPreferencesDlg, CTreePropSheet)
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_HELPINFO()
	ON_MESSAGE(PSM_SETCURSEL, OnSetCurSel)
	ON_MESSAGE(PSM_SETCURSELID, OnSetCurSelId)
END_MESSAGE_MAP()

namespace
{
	static const int kExtendedExtraWidth = 144;
	static const int kExtendedExtraHeight = 132;

	static void MoveChildWindow(CWnd *pWnd, const CRect &rect)
	{
		if (pWnd != NULL && ::IsWindow(pWnd->GetSafeHwnd()))
			pWnd->MoveWindow(rect);
	}
}

void CPreferencesDlg::RemoveHelpButtons()
{
	static const UINT aHelpIds[] = { ID_HELP, IDHELP };
	for (int i = 0; i < _countof(aHelpIds); ++i) {
		if (CWnd *pHelpButton = GetDlgItem(aHelpIds[i]))
			pHelpButton->DestroyWindow();
	}
}

CPreferencesDlg::CPreferencesDlg()
	: m_bNormalLayoutCaptured(false)
	, m_szExtendedGrowth(kExtendedExtraWidth, kExtendedExtraHeight)
{
	m_psh.dwFlags &= ~PSH_HASHELP;
	m_wndGeneral.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndDisplay.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndConnection.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndServer.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndDirectories.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndFiles.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndStats.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndIRC.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndWebServer.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndTweaks.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndSecurity.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndScheduler.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndProxy.m_psp.dwFlags &= ~PSH_HASHELP;
	m_wndMessages.m_psp.dwFlags &= ~PSH_HASHELP;
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	m_wndDebug.m_psp.dwFlags &= ~PSH_HASHELP;
#endif

	CTreePropSheet::SetPageIcon(&m_wndGeneral, _T("Preferences"));
	CTreePropSheet::SetPageIcon(&m_wndDisplay, _T("DISPLAY"));
	CTreePropSheet::SetPageIcon(&m_wndConnection, _T("CONNECTION"));
	CTreePropSheet::SetPageIcon(&m_wndProxy, _T("PROXY"));
	CTreePropSheet::SetPageIcon(&m_wndServer, _T("SERVER"));
	CTreePropSheet::SetPageIcon(&m_wndDirectories, _T("FOLDERS"));
	CTreePropSheet::SetPageIcon(&m_wndFiles, _T("Transfer"));
	CTreePropSheet::SetPageIcon(&m_wndNotify, _T("NOTIFICATIONS"));
	CTreePropSheet::SetPageIcon(&m_wndStats, _T("STATISTICS"));
	CTreePropSheet::SetPageIcon(&m_wndIRC, _T("IRC"));
	CTreePropSheet::SetPageIcon(&m_wndSecurity, _T("SECURITY"));
	CTreePropSheet::SetPageIcon(&m_wndScheduler, _T("SCHEDULER"));
	CTreePropSheet::SetPageIcon(&m_wndWebServer, _T("WEB"));
	CTreePropSheet::SetPageIcon(&m_wndTweaks, _T("TWEAK"));
	CTreePropSheet::SetPageIcon(&m_wndMessages, _T("MESSAGES"));
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	CTreePropSheet::SetPageIcon(&m_wndDebug, _T("Preferences"));
#endif

	AddPage(&m_wndGeneral);
	AddPage(&m_wndDisplay);
	AddPage(&m_wndConnection);
	AddPage(&m_wndProxy);
	AddPage(&m_wndServer);
	AddPage(&m_wndDirectories);
	AddPage(&m_wndFiles);
	AddPage(&m_wndNotify);
	AddPage(&m_wndStats);
	AddPage(&m_wndIRC);
	AddPage(&m_wndMessages);
	AddPage(&m_wndSecurity);
	AddPage(&m_wndScheduler);
	AddPage(&m_wndWebServer);
	AddPage(&m_wndTweaks);
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	AddPage(&m_wndDebug);
#endif

	// The height of the option dialog is already too large for 640x480. To show as much as
	// possible we do not show a page caption (which is a decorative element only anyway).
	SetTreeViewMode(TRUE, ::GetSystemMetrics(SM_CYSCREEN) >= 600, TRUE);
	SetTreeWidth(170);

	m_pPshStartPage = NULL;
	m_bSaveIniFile = false;
}

void CPreferencesDlg::OnDestroy()
{
	CTreePropSheet::OnDestroy();
	if (m_bSaveIniFile) {
		thePrefs.Save();
		m_bSaveIniFile = false;
	}
	m_pPshStartPage = GetPage(GetActiveIndex())->m_psp.pszTemplate;
}

BOOL CPreferencesDlg::OnInitDialog()
{
	ASSERT(!m_bSaveIniFile);
	BOOL bResult = CTreePropSheet::OnInitDialog();
	InitWindowStyles(this);

	RemoveHelpButtons();

	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == m_pPshStartPage) {
			SetActivePage(i);
			break;
		}

	Localize();
	CaptureNormalLayout();
	ApplyExtendedLayout();
	return bResult;
}

void CPreferencesDlg::CaptureNormalLayout()
{
	if (m_bNormalLayoutCaptured)
		return;

	CRect rectWindow;
	GetWindowRect(&rectWindow);
	m_szNormalWindow = rectWindow.Size();

	CTreeCtrl *pTree = GetPageTreeControl();
	if (pTree != NULL) {
		pTree->GetWindowRect(&m_rcNormalTree);
		ScreenToClient(&m_rcNormalTree);
	}

	CWnd *pFrame = GetDlgItem(0xFFFF);
	if (pFrame != NULL) {
		pFrame->GetWindowRect(&m_rcNormalFrame);
		ScreenToClient(&m_rcNormalFrame);
	}

	HWND hActivePage = PropSheet_GetCurrentPageHwnd(m_hWnd);
	if (hActivePage != NULL) {
		::GetWindowRect(hActivePage, &m_rcNormalPage);
		ScreenToClient(&m_rcNormalPage);
	}

	static const UINT aButtonIds[] = { IDOK, IDCANCEL, ID_APPLY_NOW };
	CRect *apRects[] = { &m_rcNormalOk, &m_rcNormalCancel, &m_rcNormalApply };
	for (int i = 0; i < _countof(aButtonIds); ++i) {
		CWnd *pButton = GetDlgItem(aButtonIds[i]);
		if (pButton != NULL) {
			pButton->GetWindowRect(apRects[i]);
			ScreenToClient(apRects[i]);
		} else
			apRects[i]->SetRectEmpty();
	}
	m_rcNormalHelp.SetRectEmpty();

	m_bNormalLayoutCaptured = true;
}

void CPreferencesDlg::ApplyExtendedLayout()
{
	if (!m_bNormalLayoutCaptured || m_hWnd == NULL)
		return;

	const bool bExtendedActive = (GetActivePage() == &m_wndTweaks);
	const int dx = bExtendedActive ? m_szExtendedGrowth.cx : 0;
	const int dy = bExtendedActive ? m_szExtendedGrowth.cy : 0;
	CWnd *const pFrameWnd = GetDlgItem(0xFFFF);

	SetRedraw(FALSE);

	CRect rectWindow;
	GetWindowRect(&rectWindow);
	SetWindowPos(NULL, rectWindow.left, rectWindow.top, m_szNormalWindow.cx + dx, m_szNormalWindow.cy + dy,
		SWP_NOZORDER | SWP_NOACTIVATE);

	CRect rectTree(m_rcNormalTree);
	rectTree.bottom += dy;
	MoveChildWindow(GetPageTreeControl(), rectTree);

	CRect rectFrame(m_rcNormalFrame);
	rectFrame.right += dx;
	rectFrame.bottom += dy;
	MoveChildWindow(pFrameWnd, rectFrame);

	HWND hActivePage = PropSheet_GetCurrentPageHwnd(m_hWnd);
	if (hActivePage != NULL) {
		CRect rectPage(m_rcNormalPage);
		rectPage.right += dx;
		rectPage.bottom += dy;
		::MoveWindow(hActivePage, rectPage.left, rectPage.top, rectPage.Width(), rectPage.Height(), TRUE);
	}

	const CPoint offset(dx, dy);
	const struct
	{
		UINT id;
		const CRect *normalRect;
	} aButtons[] =
	{
		{ IDOK, &m_rcNormalOk },
		{ IDCANCEL, &m_rcNormalCancel },
		{ ID_APPLY_NOW, &m_rcNormalApply }
	};

	for (int i = 0; i < _countof(aButtons); ++i) {
		if (!aButtons[i].normalRect->IsRectEmpty()) {
			CRect rect(*aButtons[i].normalRect);
			rect.OffsetRect(offset);
			MoveChildWindow(GetDlgItem(aButtons[i].id), rect);
		}
	}

	RemoveHelpButtons();
	SetRedraw(TRUE);
	if (pFrameWnd != NULL)
		pFrameWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
	if (hActivePage != NULL)
		::RedrawWindow(hActivePage, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
}

BOOL CPreferencesDlg::OnEraseBkgnd(CDC *pDC)
{
	if (pDC != NULL) {
		CRect rectClient;
		GetClientRect(&rectClient);
		pDC->FillSolidRect(&rectClient, ::GetSysColor(COLOR_3DFACE));
		return TRUE;
	}

	return __super::OnEraseBkgnd(pDC);
}

void CPreferencesDlg::LocalizeItemText(int i, UINT strid)
{
	GetPageTreeControl()->SetItemText(GetPageTreeItem(i), GetResNoAmp(strid));
}

void CPreferencesDlg::Localize()
{
	SetTitle(GetResNoAmp(IDS_EM_PREFS));

	m_wndGeneral.Localize();
	m_wndDisplay.Localize();
	m_wndConnection.Localize();
	m_wndServer.Localize();
	m_wndDirectories.Localize();
	m_wndFiles.Localize();
	m_wndStats.Localize();
	m_wndNotify.Localize();
	m_wndIRC.Localize();
	m_wndSecurity.Localize();
	m_wndTweaks.Localize();
	m_wndWebServer.Localize();
	m_wndScheduler.Localize();
	m_wndProxy.Localize();
	m_wndMessages.Localize();

	if (GetPageTreeControl()) {
		static const UINT uids[15] =
		{
			IDS_PW_GENERAL, IDS_PW_DISPLAY, IDS_CONNECTION, IDS_PW_PROXY, IDS_PW_SERVER,
			IDS_PW_DIR, IDS_PW_FILES, IDS_PW_EKDEV_OPTIONS, IDS_STATSSETUPINFO, IDS_IRC,
			IDS_MESSAGESCOMMENTS, IDS_SECURITY, IDS_SCHEDULER, IDS_PW_WS, IDS_PW_TWEAK
		};

		int c;
		for (c = 0; c < (int)_countof(uids); ++c)
			LocalizeItemText(c, uids[c]);
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
		GetPageTreeControl()->SetItemText(GetPageTreeItem(c), _T("Debug"));
#endif
	}

	UpdateCaption();
}

void CPreferencesDlg::OnHelp()
{
	int iCurSel = GetActiveIndex();
	if (iCurSel >= 0) {
		CPropertyPage *pPage = GetPage(iCurSel);
		if (pPage) {
			HELPINFO hi = {};
			hi.cbSize = (UINT)sizeof hi;
			hi.iContextType = HELPINFO_WINDOW;
			//hi.iCtrlId = 0;
			hi.hItemHandle = pPage->m_hWnd;
			//hi.dwContextId = 0;
			pPage->SendMessage(WM_HELP, 0, (LPARAM)&hi);
			return;
		}
	}

	theApp.ShowHelp(0, HELP_CONTENTS);
}

BOOL CPreferencesDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case ID_HELP:
	case IDHELP:
		return OnHelpInfo(NULL);
	case IDOK:
	case ID_APPLY_NOW:
		m_bSaveIniFile = true;
	}
	return __super::OnCommand(wParam, lParam);
}

BOOL CPreferencesDlg::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

LRESULT CPreferencesDlg::OnSetCurSel(WPARAM wParam, LPARAM lParam)
{
	const LRESULT lResult = CTreePropSheet::OnSetCurSel(wParam, lParam);
	ApplyExtendedLayout();
	return lResult;
}

LRESULT CPreferencesDlg::OnSetCurSelId(WPARAM wParam, LPARAM lParam)
{
	const LRESULT lResult = CTreePropSheet::OnSetCurSelId(wParam, lParam);
	ApplyExtendedLayout();
	return lResult;
}
