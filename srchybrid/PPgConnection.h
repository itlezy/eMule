#pragma once

#include <vector>
#include "BindAddressResolver.h"

class CPPgConnection : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgConnection)

	enum
	{
		IDD = IDD_PPG_CONNECTION
	};
	uint16 m_lastudp;
	std::vector<BindableNetworkInterface> m_bindInterfaces;
	CString m_strMissingBindInterfaceId;
	CString m_strMissingBindInterfaceName;
	void ChangePorts(uint8 iWhat); //0 - UDP, 1 - TCP, 2 - enable/disable "Test ports"
	bool ChangeUDP();
	void LoadBindableInterfaces();
	void FillBindInterfaceCombo();
	void FillBindAddressCombo(const CString &strPreferredAddress);
	CString GetSelectedBindInterfaceId() const;
	CString GetSelectedBindInterfaceName() const;
	CString GetSelectedBindAddress() const;
	void SyncStartupBindBlockCheck();

public:
	CPPgConnection();

	void Localize();
	void LoadSettings();

	static bool CheckUp(uint32 mUp, uint32 &mDown);
	static bool CheckDown(uint32 &mUp, uint32 mDown);
protected:
	CSliderCtrl m_ctlMaxDown;
	CSliderCtrl m_ctlMaxUp;
	CComboBox m_bindInterface;
	CComboBox m_bindAddress;

	void ShowLimitValues();
	void SetRateSliderTicks(CSliderCtrl &rRate);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnSettingsChange();
	afx_msg void OnEnChangeUDPDisable();
//	afx_msg void OnBnClickedNetworkKademlia();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnStartPortTest();
	afx_msg void OnEnKillFocusTCP();
	afx_msg void OnEnKillFocusUDP();
	afx_msg void OnCbnSelChangeBindInterface();
	afx_msg void OnCbnSelChangeBindAddress();
};
