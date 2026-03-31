#pragma once

#include <initializer_list>

/**
 * @brief Describes one hover-help binding for a preferences control.
 */
struct CPreferencesPageToolTip
{
	UINT uControlId;
	LPCTSTR pszText;
};

/**
 * @brief Adds common layout helpers and detailed hover tooltips to preferences pages.
 */
class CPreferencesPage : public CPropertyPage
{
protected:
	explicit CPreferencesPage(UINT uTemplateId)
		: CPropertyPage(uTemplateId)
	{
	}

	void InitializePageToolTips(std::initializer_list<CPreferencesPageToolTip> toolTips)
	{
		if (!m_wndToolTips.GetSafeHwnd()) {
			VERIFY(m_wndToolTips.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX));
			m_wndToolTips.SetMaxTipWidth(420);
			m_wndToolTips.SetDelayTime(TTDT_INITIAL, 350);
			m_wndToolTips.SetDelayTime(TTDT_AUTOPOP, 60000);
		}

		for (const CPreferencesPageToolTip &toolTip : toolTips)
			AddPageToolTip(toolTip.uControlId, toolTip.pszText);
	}

	/**
	 * @brief Expand wide-friendly controls and keep selected buttons pinned to the right edge.
	 */
	void ApplyWidePageLayout(std::initializer_list<UINT> rightAnchoredControls = {}, std::initializer_list<UINT> skipControls = {})
	{
		for (CWnd *pWnd = GetWindow(GW_CHILD); pWnd != NULL; pWnd = pWnd->GetNextWindow()) {
			if (!::IsWindow(pWnd->GetSafeHwnd()))
				continue;

			const UINT uControlId = static_cast<UINT>(pWnd->GetDlgCtrlID());
			if (ContainsControlId(skipControls, uControlId))
				continue;

			if (ContainsControlId(rightAnchoredControls, uControlId)) {
				MoveControlToRight(uControlId, 12);
				continue;
			}

			TCHAR szClassName[32];
			szClassName[0] = _T('\0');
			::GetClassName(pWnd->GetSafeHwnd(), szClassName, _countof(szClassName));
			if (ShouldStretchControl(szClassName, pWnd->GetStyle()))
				StretchControlToRight(uControlId, 12);
		}
	}

	void AddPageToolTip(UINT uControlId, LPCTSTR pszText)
	{
		ASSERT(pszText != NULL);
		CWnd *pWnd = GetDlgItem(uControlId);
		if (pWnd == NULL || !::IsWindow(pWnd->GetSafeHwnd()))
			return;

		if (m_wndToolTips.GetToolCount() != 0)
			m_wndToolTips.DelTool(pWnd);
		VERIFY(m_wndToolTips.AddTool(pWnd, pszText));
	}

	void StretchControlToRight(UINT uControlId, int iRightMargin)
	{
		CRect rectControl;
		if (!GetPageControlRect(uControlId, rectControl))
			return;

		rectControl.right = max(rectControl.left + 4, GetPageClientRight() - iRightMargin);
		MoveControlRect(uControlId, rectControl);
	}

	void MoveControlToRight(UINT uControlId, int iRightMargin)
	{
		CRect rectControl;
		if (!GetPageControlRect(uControlId, rectControl))
			return;

		const int iWidth = rectControl.Width();
		rectControl.right = GetPageClientRight() - iRightMargin;
		rectControl.left = rectControl.right - iWidth;
		MoveControlRect(uControlId, rectControl);
	}

	void MoveControlLeftOf(UINT uControlId, UINT uAnchorControlId, int iGap)
	{
		CRect rectControl;
		CRect rectAnchor;
		if (!GetPageControlRect(uControlId, rectControl) || !GetPageControlRect(uAnchorControlId, rectAnchor))
			return;

		const int iWidth = rectControl.Width();
		rectControl.right = rectAnchor.left - iGap;
		rectControl.left = rectControl.right - iWidth;
		MoveControlRect(uControlId, rectControl);
	}

	void StretchControlToLeftOf(UINT uControlId, UINT uAnchorControlId, int iGap)
	{
		CRect rectControl;
		CRect rectAnchor;
		if (!GetPageControlRect(uControlId, rectControl) || !GetPageControlRect(uAnchorControlId, rectAnchor))
			return;

		rectControl.right = max(rectControl.left + 4, rectAnchor.left - iGap);
		MoveControlRect(uControlId, rectControl);
	}

	void SetControlRect(UINT uControlId, int iLeft, int iTop, int iWidth, int iHeight)
	{
		CRect rectControl(iLeft, iTop, iLeft + iWidth, iTop + iHeight);
		MoveControlRect(uControlId, rectControl);
	}

	int GetPageClientRight() const
	{
		CRect rectClient;
		GetClientRect(&rectClient);
		return rectClient.right;
	}

	bool GetPageControlRect(UINT uControlId, CRect &rectControl) const
	{
		return GetControlRect(uControlId, rectControl);
	}

	BOOL PreTranslateMessage(MSG *pMsg) override
	{
		if (m_wndToolTips.GetSafeHwnd())
			m_wndToolTips.RelayEvent(pMsg);
		return __super::PreTranslateMessage(pMsg);
	}

private:
	static bool ContainsControlId(std::initializer_list<UINT> controlIds, UINT uControlId)
	{
		for (UINT uCurrentId : controlIds) {
			if (uCurrentId == uControlId)
				return true;
		}
		return false;
	}

	static bool ShouldStretchControl(LPCTSTR pszClassName, DWORD dwStyle)
	{
		if (_tcsicmp(pszClassName, _T("Button")) == 0)
			return (dwStyle & BS_TYPEMASK) == BS_GROUPBOX;

		if (_tcsicmp(pszClassName, _T("Edit")) == 0
			|| _tcsicmp(pszClassName, _T("ComboBox")) == 0
			|| _tcsicmp(pszClassName, _T("ComboBoxEx32")) == 0
			|| _tcsicmp(pszClassName, _T("ListBox")) == 0
			|| _tcsicmp(pszClassName, _T("SysListView32")) == 0
			|| _tcsicmp(pszClassName, _T("SysTreeView32")) == 0
			|| _tcsicmp(pszClassName, _T("RichEdit20W")) == 0
			|| _tcsicmp(pszClassName, _T("RICHEDIT50W")) == 0
			|| _tcsicmp(pszClassName, _T("msctls_trackbar32")) == 0)
		{
			return true;
		}

		if (_tcsicmp(pszClassName, _T("Static")) != 0)
			return false;

		switch (dwStyle & SS_TYPEMASK) {
		case SS_BLACKFRAME:
		case SS_GRAYFRAME:
		case SS_WHITEFRAME:
		case SS_ETCHEDHORZ:
		case SS_ETCHEDVERT:
		case SS_ETCHEDFRAME:
		case SS_BLACKRECT:
		case SS_GRAYRECT:
		case SS_WHITERECT:
			return true;
		default:
			return false;
		}
	}

	bool GetControlRect(UINT uControlId, CRect &rectControl) const
	{
		const CWnd *pWnd = GetDlgItem(uControlId);
		if (pWnd == NULL || !::IsWindow(pWnd->GetSafeHwnd()))
			return false;

		pWnd->GetWindowRect(&rectControl);
		const_cast<CPreferencesPage*>(this)->ScreenToClient(&rectControl);
		return true;
	}

	void MoveControlRect(UINT uControlId, const CRect &rectControl)
	{
		CWnd *pWnd = GetDlgItem(uControlId);
		if (pWnd == NULL || !::IsWindow(pWnd->GetSafeHwnd()))
			return;

		pWnd->MoveWindow(rectControl);
	}

	CToolTipCtrl m_wndToolTips;
};
