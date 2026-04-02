#pragma once

enum EStatusBarPane
{
	SBarLog = 0,
	SBarIP,
	SBarUsers,
	SBarUpDown,
	SBarConnected,
	SBarChatMsg
};

class CMuleStatusBarCtrl : public CStatusBarCtrl
{
	DECLARE_DYNAMIC(CMuleStatusBarCtrl)

public:
	CMuleStatusBarCtrl() = default;

	void Init();

protected:
	int GetPaneAtPosition(CPoint &point) const;
	/** Builds the single-line diagnostic tooltip for the hovered status-bar pane. */
	CString GetPaneToolTipText(EStatusBarPane iPane) const;

	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO *pTI) const;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
};
