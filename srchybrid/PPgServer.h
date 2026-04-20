#pragma once
#include "PreferenceToolTipHelper.h"

class CPPgServer : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgServer)

	enum
	{
		IDD = IDD_PPG_SERVER
	};

public:
	CPPgServer();

	void Localize();

protected:
	CPreferenceToolTipHelper m_toolTip;
	void LoadSettings();
	void UpdateToolTips();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSettingsChange()					{ SetModified(); }
	afx_msg void OnBnClickedEditadr();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};
