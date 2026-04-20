#pragma once
#include "PreferenceToolTipHelper.h"

class CPPgConnection : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgConnection)

	enum
	{
		IDD = IDD_PPG_CONNECTION
	};
	uint16 m_lastudp;
	void ChangePorts(uint8 iWhat); //0 - UDP, 1 - TCP, 2 - enable/disable "Test ports"
	bool ChangeUDP();

public:
	CPPgConnection();

	void Localize();
	void LoadSettings();

	static bool CheckUp(uint32 mUp, uint32 &mDown);
	static bool CheckDown(uint32 &mUp, uint32 mDown);
protected:
	CPreferenceToolTipHelper m_toolTip;

	void ShowLimitValues();
	void UpdateToolTips();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSettingsChange();
	afx_msg void OnEnChangeUDPDisable();
//	afx_msg void OnBnClickedNetworkKademlia();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnStartPortTest();
	afx_msg void OnEnKillFocusTCP();
	afx_msg void OnEnKillFocusUDP();
};
