// ------------------------------------------------------------
//  CDialogMinTrayBtn template class
//  MFC CDialog with minimize to system tray button (0.04)
//  Supports WinXP styles (thanks to David Yuheng Zhao for CVisualStylesXP - yuheng_zhao@yahoo.com)
// ------------------------------------------------------------
//  DialogMinTrayBtn.h
//  zegzav - 2002,2003 - eMule project (https://www.emule-project.net)
// ------------------------------------------------------------
#pragma once
#define HTMINTRAYBUTTON	65

template <class BASE = CDialog> class CDialogMinTrayBtn : public BASE
{
public:
	// constructor
	CDialogMinTrayBtn();
	explicit CDialogMinTrayBtn(LPCTSTR lpszTemplateName, CWnd *pParentWnd = NULL);
	explicit CDialogMinTrayBtn(UINT nIDTemplate, CWnd *pParentWnd = NULL);

	// methods
	void MinTrayBtnShow();
	void MinTrayBtnHide();
	bool MinTrayBtnIsVisible() const			{ return m_bMinTrayBtnVisible; }

	void MinTrayBtnEnable();
	void MinTrayBtnDisable();
	bool MinTrayBtnIsEnabled() const			{ return m_bMinTrayBtnEnabled; }

	void SetWindowText(LPCTSTR lpszString);

protected:
	// messages
	virtual BOOL OnInitDialog();
	afx_msg void OnNcPaint();
	afx_msg BOOL OnNcActivate(BOOL bActive);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint point);
	afx_msg void OnNcRButtonDown(UINT nHitTest, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT _OnThemeChanged();
	DECLARE_MESSAGE_MAP()

private:
	// internal methods
	void MinTrayBtnInit();
	void MinTrayBtnDraw();
	bool MinTrayBtnHitTest(CPoint ptScreen) const;
	void MinTrayBtnUpdatePosAndSize();

	void MinTrayBtnSetUp();
	void MinTrayBtnSetDown();

	const CPoint &MinTrayBtnGetPos() const	{ return m_MinTrayBtnPos; }
	const CSize &MinTrayBtnGetSize() const	{ return m_MinTrayBtnSize; }
	CRect MinTrayBtnGetRect() const			{ return CRect(MinTrayBtnGetPos(), MinTrayBtnGetSize()); }

	bool IsWindowsClassicStyle() const		{ return m_bMinTrayBtnWindowsClassicStyle; }
	INT GetVisualStylesXPColor() const;

	bool MinTrayBtnInitBitmap();

	// data members
	CPoint	m_MinTrayBtnPos;
	CSize	m_MinTrayBtnSize;
	CBitmap	m_bmMinTrayBtnBitmap;
	UINT_PTR m_nMinTrayBtnTimerId;
	static LPCTSTR m_pszMinTrayBtnBmpName[];
	bool	m_bMinTrayBtnActive;
	bool	m_bMinTrayBtnCapture;
	bool	m_bMinTrayBtnEnabled;
	bool	m_bMinTrayBtnHitTest;
	bool	m_bMinTrayBtnUp;
	bool	m_bMinTrayBtnVisible;
	bool	m_bMinTrayBtnWindowsClassicStyle;
};
