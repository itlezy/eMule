// ------------------------------------------------------------
//  CDialogMinTrayBtn template class
//  MFC CDialog with minimize to system tray button (0.04)
//  Supports WinXP styles (thanks to David Yuheng Zhao for CVisualStylesXP - yuheng_zhao@yahoo.com)
// ------------------------------------------------------------
//  DialogMinTrayBtn.hpp
//  zegzav - 2002,2003 - eMule project (https://www.emule-project.net)
// ------------------------------------------------------------
#include "stdafx.h"
#include <uxtheme.h>
#include "DialogMinTrayBtn.h"
#include "ResizableDialog.h"
#include "OtherFunctions.h"
#include "MenuCmds.h"
#undef SCHEMADEF_H
#undef SCHEMA_VERIFY_VSSYM32
#include "SchemaDef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// define this to instantiate functions for class 'BASE' right in this CPP module
#if _MSC_VER >= 1310
#define	TEMPLATE	template <>
#else
#define	TEMPLATE
#endif
#define BASE		CResizableDialog

// ------------------------------
//  constants
// ------------------------------

#define CAPTION_BUTTONSPACE      (2)
#define CAPTION_MINHEIGHT        (8)

#define TIMERMINTRAYBTN_ID       0x76617a67
#define TIMERMINTRAYBTN_PERIOD   300    // ms

#define WP_TRAYBUTTON WP_MINBUTTON

BEGIN_TM_PART_STATES(TRAYBUTTON)
	TM_STATE(1, TRAYBS, NORMAL)
	TM_STATE(2, TRAYBS, HOT)
	TM_STATE(3, TRAYBS, PUSHED)
	TM_STATE(4, TRAYBS, DISABLED)
	// Inactive
	TM_STATE(5, TRAYBS, INORMAL)
	TM_STATE(6, TRAYBS, IHOT)
	TM_STATE(7, TRAYBS, IPUSHED)
	TM_STATE(8, TRAYBS, IDISABLED)
END_TM_PART_STATES()

#define BMP_TRAYBTN_WIDTH		(21)
#define BMP_TRAYBTN_HEIGHT		(21)
#define BMP_TRAYBTN_BLUE		_T("TRAYBTN_BLUE")
#define BMP_TRAYBTN_METALLIC	_T("TRAYBTN_METALLIC")
#define BMP_TRAYBTN_HOMESTEAD	_T("TRAYBTN_HOMESTEAD")
#define BMP_TRAYBTN_TRANSCOLOR	(RGB(255,0,255))

TEMPLATE const TCHAR *CDialogMinTrayBtn<BASE>::m_pszMinTrayBtnBmpName[] = { BMP_TRAYBTN_BLUE, BMP_TRAYBTN_METALLIC, BMP_TRAYBTN_HOMESTEAD };

#define VISUALSTYLESXP_DEFAULTFILE		L"LUNA.MSSTYLES"
#define VISUALSTYLESXP_BLUE				0
#define VISUALSTYLESXP_METALLIC			1
#define VISUALSTYLESXP_HOMESTEAD		2
#define VISUALSTYLESXP_NAMEBLUE			L"NORMALCOLOR"
#define VISUALSTYLESXP_NAMEMETALLIC		L"METALLIC"
#define VISUALSTYLESXP_NAMEHOMESTEAD	L"HOMESTEAD"

#define _ON_WM_THEMECHANGED() \
	{ WM_THEMECHANGED, 0, 0, 0, AfxSig_l, \
		(AFX_PMSG)(AFX_PMSGW) \
		(static_cast<LRESULT (AFX_MSG_CALL CWnd::*)(void)>(&ThisClass::_OnThemeChanged)) },

BEGIN_TEMPLATE_MESSAGE_MAP(CDialogMinTrayBtn, BASE, BASE)
	ON_WM_NCPAINT()
	ON_WM_NCACTIVATE()
	ON_WM_NCHITTEST()
	ON_WM_NCLBUTTONDOWN()
	ON_WM_NCRBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_TIMER()
	_ON_WM_THEMECHANGED()
END_MESSAGE_MAP()


TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn()
	: BASE()
{
	MinTrayBtnInit();
}

TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn(LPCTSTR lpszTemplateName, CWnd *pParentWnd)
	: BASE(lpszTemplateName, pParentWnd)
{
	MinTrayBtnInit();
}

TEMPLATE CDialogMinTrayBtn<BASE>::CDialogMinTrayBtn(UINT nIDTemplate, CWnd *pParentWnd)
	: BASE(nIDTemplate, pParentWnd)
{
	MinTrayBtnInit();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnInit()
{
	m_nMinTrayBtnTimerId = 0;
	m_bMinTrayBtnActive = false;
	m_bMinTrayBtnCapture = false;
	m_bMinTrayBtnEnabled = true;
	m_bMinTrayBtnHitTest = false;
	m_bMinTrayBtnUp = true;
	m_bMinTrayBtnVisible = true;
	MinTrayBtnInitBitmap();
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::OnInitDialog()
{
	BOOL bReturn = BASE::OnInitDialog();
	InitWindowStyles(this);
	return bReturn;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcPaint()
{
	BASE::OnNcPaint();
	MinTrayBtnUpdatePosAndSize();
	MinTrayBtnDraw();
}

TEMPLATE BOOL CDialogMinTrayBtn<BASE>::OnNcActivate(BOOL bActive)
{
	MinTrayBtnUpdatePosAndSize();
	BOOL bResult = BASE::OnNcActivate(bActive);
	m_bMinTrayBtnActive = bActive;
	MinTrayBtnDraw();
	return bResult;
}

TEMPLATE LRESULT CDialogMinTrayBtn<BASE>::OnNcHitTest(CPoint point)
{
	bool bPreviousHitTest = m_bMinTrayBtnHitTest;
	m_bMinTrayBtnHitTest = MinTrayBtnHitTest(point);
	if (!IsWindowsClassicStyle() && m_bMinTrayBtnHitTest != bPreviousHitTest)
		MinTrayBtnDraw(); // Windows XP Style (hot button)
	return m_bMinTrayBtnHitTest ? HTMINTRAYBUTTON : BASE::OnNcHitTest(point);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !MinTrayBtnIsEnabled() || !MinTrayBtnIsVisible() || !MinTrayBtnHitTest(point)) {
		BASE::OnNcLButtonDown(nHitTest, point);
		return;
	}
	SetCapture();
	m_bMinTrayBtnCapture = TRUE;
	MinTrayBtnSetDown();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnNcRButtonDown(UINT nHitTest, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !MinTrayBtnIsVisible() || !MinTrayBtnHitTest(point))
		BASE::OnNcRButtonDown(nHitTest, point);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !m_bMinTrayBtnCapture) {
		BASE::OnMouseMove(nFlags, point);
		return;
	}

	ClientToScreen(&point);
	m_bMinTrayBtnHitTest = MinTrayBtnHitTest(point);
	if (m_bMinTrayBtnHitTest) {
		if (m_bMinTrayBtnUp)
			MinTrayBtnSetDown();
	} else if (!m_bMinTrayBtnUp)
		MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ((GetStyle() & WS_DISABLED) || !m_bMinTrayBtnCapture) {
		BASE::OnLButtonUp(nFlags, point);
		return;
	}

	::ReleaseCapture();
	m_bMinTrayBtnCapture = FALSE;
	MinTrayBtnSetUp();

	ClientToScreen(&point);
	if (MinTrayBtnHitTest(point))
		SendMessage(WM_SYSCOMMAND, MP_MINIMIZETOTRAY, MAKELONG(point.x, point.y));
}

TEMPLATE void CDialogMinTrayBtn<BASE>::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_nMinTrayBtnTimerId) {
		// Visual XP Style (hot button)
		POINT point;
		if (::GetCursorPos(&point)) {
			bool bHitTest = MinTrayBtnHitTest(point);
			if (m_bMinTrayBtnHitTest != bHitTest) {
				m_bMinTrayBtnHitTest = bHitTest;
				MinTrayBtnDraw();
			}
		}
	}
}

TEMPLATE LRESULT CDialogMinTrayBtn<BASE>::_OnThemeChanged()
{
	// BASE::OnThemeChanged();
	MinTrayBtnInitBitmap();
	if (!IsWindowsClassicStyle() && !m_nMinTrayBtnTimerId)
		m_nMinTrayBtnTimerId = SetTimer(TIMERMINTRAYBTN_ID, TIMERMINTRAYBTN_PERIOD, NULL);
	return 0;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnUpdatePosAndSize()
{
	DWORD dwExStyle = GetExStyle();
	int cyCaption = (((dwExStyle & WS_EX_TOOLWINDOW) == 0) ? ::GetSystemMetrics(SM_CYCAPTION) : ::GetSystemMetrics(SM_CYSMCAPTION)) - 1;
	if (cyCaption < CAPTION_MINHEIGHT)
		cyCaption = CAPTION_MINHEIGHT;

	CRect rcWnd;
	GetWindowRect(&rcWnd);

	// get Windows' frame window button width/height (this may not always be a square!)
	CSize szBtn;
	szBtn.cx = ::GetSystemMetrics(SM_CXSIZE) - (IsWindowsClassicStyle() ? 2 : 4);
	szBtn.cy = cyCaption - (CAPTION_BUTTONSPACE * 2);

	// set our frame window button width/height...
	if (IsWindowsClassicStyle())
		m_MinTrayBtnSize = szBtn; //the same as Windows' buttons for non WinXP
	else
		m_MinTrayBtnSize.cx = m_MinTrayBtnSize.cy = szBtn.cy; //a square for WinXP

	m_MinTrayBtnPos.x = rcWnd.Width() - (CAPTION_BUTTONSPACE + m_MinTrayBtnSize.cx + CAPTION_BUTTONSPACE + szBtn.cx);
	m_MinTrayBtnPos.y = CAPTION_BUTTONSPACE;

	DWORD dwStyle = GetStyle();
	if ((dwStyle & WS_THICKFRAME) != 0)
		// resizable window
		m_MinTrayBtnPos += CSize(-::GetSystemMetrics(SM_CXSIZEFRAME), ::GetSystemMetrics(SM_CYSIZEFRAME));
	else
		// fixed window
		m_MinTrayBtnPos += CSize(-::GetSystemMetrics(SM_CXFIXEDFRAME), ::GetSystemMetrics(SM_CYFIXEDFRAME));

	if (((dwExStyle & WS_EX_TOOLWINDOW) == 0) && (((dwStyle & WS_MINIMIZEBOX) != 0) || ((dwStyle & WS_MAXIMIZEBOX) != 0)))
		if (IsWindowsClassicStyle())
			m_MinTrayBtnPos.x -= (szBtn.cx * 2) + CAPTION_BUTTONSPACE;
		else
			m_MinTrayBtnPos.x -= (szBtn.cx + CAPTION_BUTTONSPACE) * 2;
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnShow()
{
	if (MinTrayBtnIsVisible())
		return;

	if (!IsWindowsClassicStyle() && !m_nMinTrayBtnTimerId)
		m_nMinTrayBtnTimerId = SetTimer(TIMERMINTRAYBTN_ID, TIMERMINTRAYBTN_PERIOD, NULL);
	m_bMinTrayBtnVisible = TRUE;
	if (IsWindowVisible())
		RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnHide()
{
	if (!MinTrayBtnIsVisible())
		return;

	m_bMinTrayBtnVisible = FALSE;
	if (m_nMinTrayBtnTimerId) {
		KillTimer(m_nMinTrayBtnTimerId);
		m_nMinTrayBtnTimerId = 0;
	}

	if (IsWindowVisible())
		RedrawWindow(NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnEnable()
{
	if (MinTrayBtnIsEnabled())
		return;

	m_bMinTrayBtnEnabled = TRUE;
	MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnDisable()
{
	if (!MinTrayBtnIsEnabled())
		return;

	m_bMinTrayBtnEnabled = FALSE;
	if (m_bMinTrayBtnCapture) {
		::ReleaseCapture();
		m_bMinTrayBtnCapture = FALSE;
	}
	MinTrayBtnSetUp();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnDraw()
{
	if (!MinTrayBtnIsVisible())
		return;

	CDC *pDC = GetWindowDC();
	if (!pDC)
		return; // panic!

	if (IsWindowsClassicStyle()) {
		// button
		pDC->DrawFrameControl(MinTrayBtnGetRect(), DFC_BUTTON
							, m_bMinTrayBtnUp ? DFCS_BUTTONPUSH : DFCS_BUTTONPUSH | DFCS_PUSHED);

		 // dot
		CRect btn(MinTrayBtnGetRect());
		btn.DeflateRect(2, 2);
		UINT caption = MinTrayBtnGetSize().cy + (CAPTION_BUTTONSPACE * 2);
		UINT pixratio = (caption >= 14) ? ((caption >= 20) ? 2 + ((caption - 20) / 8) : 2) : 1;
		UINT pixratio2 = (caption >= 12) ? 1 + (caption - 12) / 8 : 0;
		UINT dotwidth = (1 + pixratio * 3) >> 1;
		UINT dotheight = pixratio;
		CRect dot(0, 0, dotwidth, dotheight);
		CSize spc((1 + pixratio2 * 3) >> 1, pixratio2);
		dot -= dot.Size();
		dot += btn.BottomRight();
		dot -= spc;
		if (!m_bMinTrayBtnUp)
			dot += SIZE{1, 1};
		if (m_bMinTrayBtnEnabled)
			pDC->FillSolidRect(dot, ::GetSysColor(COLOR_BTNTEXT)); //black
		else {
			pDC->FillSolidRect(dot + SIZE{1, 1}, ::GetSysColor(COLOR_BTNHILIGHT)); //grey
			pDC->FillSolidRect(dot, ::GetSysColor(COLOR_GRAYTEXT));	//grey
		}
	} else {
		// VisualStylesXP
		CRect btn(MinTrayBtnGetRect());
		int iState;
		if (!m_bMinTrayBtnEnabled)
			iState = TRAYBS_DISABLED;
		else if (GetStyle() & WS_DISABLED)
			iState = MINBS_NORMAL;
		else if (m_bMinTrayBtnHitTest)
			iState = (m_bMinTrayBtnCapture) ? MINBS_PUSHED : MINBS_HOT;
		else
			iState = MINBS_NORMAL;
		// inactive
		if (!m_bMinTrayBtnActive)
			iState += 4; // inactive state TRAYBS_Ixxx

		if (m_bmMinTrayBtnBitmap.m_hObject) {
			// known theme (bitmap)
			CBitmap *pBmpOld;
			CDC dcMem;
			if (dcMem.CreateCompatibleDC(pDC) && (pBmpOld = dcMem.SelectObject(&m_bmMinTrayBtnBitmap)) != NULL) {
				::TransparentBlt(pDC->m_hDC, btn.left, btn.top, btn.Width(), btn.Height()
					, dcMem.m_hDC, 0, BMP_TRAYBTN_HEIGHT * (iState - 1)
					, BMP_TRAYBTN_WIDTH, BMP_TRAYBTN_HEIGHT, BMP_TRAYBTN_TRANSCOLOR);
				dcMem.SelectObject(pBmpOld);
			}
		} else {
			// unknown theme (ThemeData)
			HTHEME hTheme = ::OpenThemeData(m_hWnd, L"Window");
			if (hTheme) {
				btn.top += btn.Height() / 8;
				::DrawThemeBackground(hTheme, pDC->m_hDC, WP_TRAYBUTTON, iState, &btn, NULL);
				::CloseThemeData(hTheme);
			}
		}
	}

	ReleaseDC(pDC);
}

TEMPLATE bool CDialogMinTrayBtn<BASE>::MinTrayBtnHitTest(CPoint ptScreen) const
{
	CRect rcWnd;
	GetWindowRect(&rcWnd);
	// adjust 'ptScreen' with possible RTL window layout
	if (::MapWindowPoints(HWND_DESKTOP, m_hWnd, &rcWnd.TopLeft(), 2) == 0
		|| ::MapWindowPoints(HWND_DESKTOP, m_hWnd, &ptScreen, 1) == 0)
	{
		// several bug reports about not working on NT SP6 (?). in case of any problems with
		// 'MapWindowPoints' we fall back to old code (does not work for RTL window layout though)
		CRect rcWndOrg(rcWnd);
		CPoint ptScreenOrg(ptScreen);
		rcWnd = rcWndOrg;
		ptScreen = ptScreenOrg;
	}
	ptScreen.Offset(-rcWnd.TopLeft());

	CRect rcBtn(MinTrayBtnGetRect());
	rcBtn.InflateRect(0, CAPTION_BUTTONSPACE);
	return rcBtn.PtInRect(ptScreen);
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnSetUp()
{
	m_bMinTrayBtnUp = TRUE;
	MinTrayBtnDraw();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::MinTrayBtnSetDown()
{
	m_bMinTrayBtnUp = FALSE;
	MinTrayBtnDraw();
}

TEMPLATE void CDialogMinTrayBtn<BASE>::SetWindowText(LPCTSTR lpszString)
{
	BASE::SetWindowText(lpszString);
	MinTrayBtnDraw();
}

TEMPLATE INT CDialogMinTrayBtn<BASE>::GetVisualStylesXPColor() const
{
	if (IsWindowsClassicStyle())
		return -1;

	WCHAR szwThemeFile[MAX_PATH];
	WCHAR szwThemeColor[256];
	if (::GetCurrentThemeName(szwThemeFile, MAX_PATH, szwThemeColor, _countof(szwThemeColor), NULL, 0) != S_OK)
		return -1;
	WCHAR *p = wcsrchr(szwThemeFile, _T('\\'));
	if (p == NULL)
		return -1;
	if (_wcsicmp(&p[1], VISUALSTYLESXP_DEFAULTFILE) == 0) {
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEBLUE) == 0)
			return VISUALSTYLESXP_BLUE;
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEMETALLIC) == 0)
			return VISUALSTYLESXP_METALLIC;
		if (_wcsicmp(szwThemeColor, VISUALSTYLESXP_NAMEHOMESTEAD) == 0)
			return VISUALSTYLESXP_HOMESTEAD;
	}
	return -1;
}

TEMPLATE bool CDialogMinTrayBtn<BASE>::MinTrayBtnInitBitmap()
{
	m_bMinTrayBtnWindowsClassicStyle = !(::IsThemeActive() && ::IsAppThemed());

	m_bmMinTrayBtnBitmap.DeleteObject();
	INT iColor = GetVisualStylesXPColor();
	return (iColor >= 0) && m_bmMinTrayBtnBitmap.LoadBitmap(m_pszMinTrayBtnBmpName[iColor]);
}
