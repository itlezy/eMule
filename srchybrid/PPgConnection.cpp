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
#include "PPgConnection.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "Opcodes.h"
#include "StatisticsDlg.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "HelpIDs.h"
#include "Statistics.h"
#include "ListenSocket.h"
#include "ClientUDPSocket.h"
#include "PreferencesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	static DWORD_PTR const s_dwAnyInterfaceItemData = static_cast<DWORD_PTR>(-1);
	static DWORD_PTR const s_dwMissingInterfaceItemData = static_cast<DWORD_PTR>(-2);
	static int const s_iBandwidthSliderMax = 300000;

	/**
	 * @brief Parse a signed bandwidth value from a limit edit box.
	 *
	 * The limits page now accepts `0` and negative numbers to mean unlimited.
	 */
	static LONGLONG GetBandwidthEditValue(const CWnd *pWnd, int iControlId)
	{
		CString strValue;
		pWnd->GetDlgItemText(iControlId, strValue);
		return _tcstoi64(strValue, NULL, 10);
	}

	/**
	 * @brief Convert typed bandwidth input into the internal unlimited-or-positive format.
	 */
	static uint32 NormalizeBandwidthLimitInput(const LONGLONG nConfiguredLimit)
	{
		if (nConfiguredLimit <= 0)
			return UNLIMITED;
		return (nConfiguredLimit >= static_cast<LONGLONG>(UNLIMITED)) ? (UNLIMITED - 1) : static_cast<uint32>(nConfiguredLimit);
	}

	/**
	 * @brief Keep the slider bounded while the textbox remains free-form.
	 */
	static int GetBandwidthSliderPos(const uint32 uConfiguredLimit)
	{
		if (uConfiguredLimit == UNLIMITED)
			return 0;
		return min(s_iBandwidthSliderMax, static_cast<int>(uConfiguredLimit));
	}

	/**
	 * @brief Round-trip unlimited limits through the edit boxes as `0`.
	 */
	static void SetBandwidthEditValue(CWnd *pWnd, int iControlId, const uint32 uConfiguredLimit)
	{
		CString strValue;
		strValue.Format(_T("%u"), (uConfiguredLimit == UNLIMITED) ? 0u : uConfiguredLimit);
		pWnd->SetDlgItemText(iControlId, strValue);
	}
}


IMPLEMENT_DYNAMIC(CPPgConnection, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgConnection, CPropertyPage)
	ON_BN_CLICKED(IDC_STARTTEST, OnStartPortTest)
	ON_EN_CHANGE(IDC_DOWNLOAD_CAP, OnSettingsChange)
	ON_EN_CHANGE(IDC_UPLOAD_CAP, OnSettingsChange)
	ON_BN_CLICKED(IDC_UDPDISABLE, OnEnChangeUDPDisable)
	ON_EN_CHANGE(IDC_UDPPORT, OnSettingsChange)
	ON_EN_CHANGE(IDC_PORT, OnSettingsChange)
	ON_EN_KILLFOCUS(IDC_UDPPORT, OnEnKillFocusUDP)
	ON_EN_KILLFOCUS(IDC_PORT, OnEnKillFocusTCP)
	ON_EN_CHANGE(IDC_MAXCON, OnSettingsChange)
	ON_EN_CHANGE(IDC_MAXSOURCEPERFILE, OnSettingsChange)
	ON_BN_CLICKED(IDC_AUTOCONNECT, OnSettingsChange)
	ON_BN_CLICKED(IDC_RECONN, OnSettingsChange)
	ON_BN_CLICKED(IDC_RANDOMIZEPORTSONSTARTUP, OnSettingsChange)
	ON_BN_CLICKED(IDC_STARTUP_BIND_BLOCK, OnSettingsChange)
	ON_BN_CLICKED(IDC_NETWORK_ED2K, OnSettingsChange)
	ON_BN_CLICKED(IDC_SHOWOVERHEAD, OnSettingsChange)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_NETWORK_KADEMLIA, OnSettingsChange)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_PREF_UPNPONSTART, OnSettingsChange)
	ON_CBN_SELCHANGE(IDC_BIND_INTERFACE, OnCbnSelChangeBindInterface)
	ON_CBN_SELCHANGE(IDC_BIND_ADDRESS, OnSettingsChange)
END_MESSAGE_MAP()

CPPgConnection::CPPgConnection()
	: CPropertyPage(CPPgConnection::IDD)
	, m_lastudp()
{
}

void CPPgConnection::LoadBindableInterfaces()
{
	// Refresh the live adapter list so the bind selectors reflect current interface availability.
	m_bindInterfaces = CBindAddressResolver::GetBindableInterfaces();
}

void CPPgConnection::FillBindInterfaceCombo()
{
	m_bindInterface.ResetContent();
	m_strMissingBindInterfaceId.Empty();
	m_strMissingBindInterfaceName.Empty();

	int iItem = m_bindInterface.AddString(GetResString(IDS_BIND_ANY_INTERFACE));
	m_bindInterface.SetItemData(iItem, s_dwAnyInterfaceItemData);
	m_bindInterface.SetCurSel(iItem);

	const CString &strConfiguredInterface = thePrefs.GetBindInterface();
	int iSelectedItem = iItem;

	for (size_t i = 0; i < m_bindInterfaces.size(); ++i) {
		iItem = m_bindInterface.AddString(m_bindInterfaces[i].strDisplayName);
		m_bindInterface.SetItemData(iItem, static_cast<DWORD_PTR>(i));
		if (!m_bindInterfaces[i].strId.CompareNoCase(strConfiguredInterface))
			iSelectedItem = iItem;
	}

	if (!strConfiguredInterface.IsEmpty() && iSelectedItem == 0) {
		m_strMissingBindInterfaceId = strConfiguredInterface;
		m_strMissingBindInterfaceName = thePrefs.GetBindInterfaceName();
		const CString strMissingLabel = m_strMissingBindInterfaceName.IsEmpty() ? m_strMissingBindInterfaceId : m_strMissingBindInterfaceName;
		iItem = m_bindInterface.AddString(strMissingLabel);
		m_bindInterface.SetItemData(iItem, s_dwMissingInterfaceItemData);
		iSelectedItem = iItem;
	}

	m_bindInterface.SetCurSel(iSelectedItem);
}

void CPPgConnection::FillBindAddressCombo(const CString &strPreferredAddress)
{
	m_bindAddress.ResetContent();
	int iItem = m_bindAddress.AddString(GetResString(IDS_BIND_ALL_ADDRESSES));
	m_bindAddress.SetCurSel(iItem);

	CStringArray astrAddresses;
	const CString strSelectedInterfaceId = GetSelectedBindInterfaceId();
	if (strSelectedInterfaceId.IsEmpty()) {
		for (size_t i = 0; i < m_bindInterfaces.size(); ++i) {
			for (size_t j = 0; j < m_bindInterfaces[i].addresses.size(); ++j) {
				bool bDuplicate = false;
				for (INT_PTR k = 0; k < astrAddresses.GetCount(); ++k) {
					if (!astrAddresses[k].CompareNoCase(m_bindInterfaces[i].addresses[j])) {
						bDuplicate = true;
						break;
					}
				}
				if (!bDuplicate)
					astrAddresses.Add(m_bindInterfaces[i].addresses[j]);
			}
		}
	} else {
		for (size_t i = 0; i < m_bindInterfaces.size(); ++i) {
			if (m_bindInterfaces[i].strId.CompareNoCase(strSelectedInterfaceId))
				continue;

			for (size_t j = 0; j < m_bindInterfaces[i].addresses.size(); ++j)
				astrAddresses.Add(m_bindInterfaces[i].addresses[j]);
			break;
		}
	}

	int iSelectedItem = 0;
	for (INT_PTR i = 0; i < astrAddresses.GetCount(); ++i) {
		iItem = m_bindAddress.AddString(astrAddresses[i]);
		if (!astrAddresses[i].CompareNoCase(strPreferredAddress))
			iSelectedItem = iItem;
	}

	if (!strPreferredAddress.IsEmpty() && iSelectedItem == 0) {
		iItem = m_bindAddress.AddString(strPreferredAddress);
		iSelectedItem = iItem;
	}

	m_bindAddress.SetCurSel(iSelectedItem);
}

CString CPPgConnection::GetSelectedBindInterfaceId() const
{
	const int iSel = m_bindInterface.GetCurSel();
	if (iSel == CB_ERR)
		return CString();

	const DWORD_PTR dwItemData = m_bindInterface.GetItemData(iSel);
	if (dwItemData == s_dwAnyInterfaceItemData)
		return CString();
	if (dwItemData == s_dwMissingInterfaceItemData)
		return m_strMissingBindInterfaceId;
	if (dwItemData >= m_bindInterfaces.size())
		return CString();
	return m_bindInterfaces[dwItemData].strId;
}

CString CPPgConnection::GetSelectedBindInterfaceName() const
{
	const int iSel = m_bindInterface.GetCurSel();
	if (iSel == CB_ERR)
		return CString();

	const DWORD_PTR dwItemData = m_bindInterface.GetItemData(iSel);
	if (dwItemData == s_dwAnyInterfaceItemData)
		return CString();
	if (dwItemData == s_dwMissingInterfaceItemData)
		return m_strMissingBindInterfaceName;
	if (dwItemData >= m_bindInterfaces.size())
		return CString();
	return m_bindInterfaces[dwItemData].strName;
}

CString CPPgConnection::GetSelectedBindAddress() const
{
	const int iSel = m_bindAddress.GetCurSel();
	if (iSel <= 0)
		return CString();

	CString strAddress;
	m_bindAddress.GetLBText(iSel, strAddress);
	return strAddress;
}

void CPPgConnection::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MAXDOWN_SLIDER, m_ctlMaxDown);
	DDX_Control(pDX, IDC_MAXUP_SLIDER, m_ctlMaxUp);
	DDX_Control(pDX, IDC_BIND_INTERFACE, m_bindInterface);
	DDX_Control(pDX, IDC_BIND_ADDRESS, m_bindAddress);
}

void CPPgConnection::OnEnKillFocusTCP()
{
	ChangePorts(1);
}

void CPPgConnection::OnEnKillFocusUDP()
{
	ChangePorts(0);
}

void CPPgConnection::ChangePorts(uint8 iWhat)
{
	UINT tcp = GetDlgItemInt(IDC_PORT, NULL, FALSE);
	UINT udp = GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);

	GetDlgItem(IDC_STARTTEST)->EnableWindow(
		GetDlgItemInt(IDC_PORT, NULL, FALSE) == theApp.listensocket->GetConnectedPort()
		&& GetDlgItemInt(IDC_UDPPORT, NULL, FALSE) == theApp.clientudp->GetConnectedPort()
	);

	if (iWhat == 0) //UDP
		ChangeUDP();
	else if (iWhat == 1) //TCP
		if (tcp != thePrefs.port || udp != thePrefs.udpport)
			OnSettingsChange();
	//else if (iWhat == 2) "Test ports" button enable/disable - done already
}

bool CPPgConnection::ChangeUDP()
{
	bool bDisabled = IsDlgButtonChecked(IDC_UDPDISABLE) != 0;
	GetDlgItem(IDC_UDPPORT)->EnableWindow(!bDisabled);

	uint16 newVal, oldVal = (uint16)GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);
	if (oldVal)
		m_lastudp = oldVal;
	if (bDisabled)
		newVal = 0;
	else
		newVal = m_lastudp ? m_lastudp : (10ui16 + thePrefs.port);
	if (newVal != oldVal)
		SetDlgItemInt(IDC_UDPPORT, newVal, FALSE);
	return bDisabled;
}

void CPPgConnection::OnEnChangeUDPDisable()
{
	SetModified();
	bool bDisabled = ChangeUDP();
	CheckDlgButton(IDC_NETWORK_KADEMLIA, static_cast<UINT>(thePrefs.networkkademlia && !bDisabled)); // don't use GetNetworkKademlia here
	GetDlgItem(IDC_NETWORK_KADEMLIA)->EnableWindow(!bDisabled);
}

BOOL CPPgConnection::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	static_cast<CEdit*>(GetDlgItem(IDC_DOWNLOAD_CAP))->SetLimitText(11);
	static_cast<CEdit*>(GetDlgItem(IDC_UPLOAD_CAP))->SetLimitText(11);
	static_cast<CEdit*>(GetDlgItem(IDC_PORT))->SetLimitText(5);
	static_cast<CEdit*>(GetDlgItem(IDC_UDPPORT))->SetLimitText(5);

	LoadBindableInterfaces();
	LoadSettings();
	Localize();

	ChangePorts(2); //"Test ports" button enable/disable

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgConnection::LoadSettings()
{
	if (m_hWnd) {
		LoadBindableInterfaces();
		m_lastudp = thePrefs.udpport;
		CheckDlgButton(IDC_UDPDISABLE, !m_lastudp); //before the port number!
		SetDlgItemInt(IDC_UDPPORT, m_lastudp, FALSE);
		SetBandwidthEditValue(this, IDC_DOWNLOAD_CAP, thePrefs.m_maxdownload);
		m_ctlMaxDown.SetRange(0, s_iBandwidthSliderMax);
		SetRateSliderTicks(m_ctlMaxDown);
		SetBandwidthEditValue(this, IDC_UPLOAD_CAP, thePrefs.m_maxupload);
		m_ctlMaxUp.SetRange(0, s_iBandwidthSliderMax);
		SetRateSliderTicks(m_ctlMaxUp);
		m_ctlMaxDown.SetPos(GetBandwidthSliderPos(thePrefs.m_maxdownload));
		m_ctlMaxUp.SetPos(GetBandwidthSliderPos(thePrefs.m_maxupload));

		SetDlgItemInt(IDC_PORT, thePrefs.port, FALSE);
		FillBindInterfaceCombo();
		FillBindAddressCombo(thePrefs.GetConfiguredBindAddr());
		SetDlgItemInt(IDC_MAXCON, thePrefs.maxconnections);
		SetDlgItemInt(IDC_MAXSOURCEPERFILE, (thePrefs.maxsourceperfile == 0xffff ? 0 : thePrefs.maxsourceperfile));

		CheckDlgButton(IDC_RECONN, static_cast<UINT>(thePrefs.reconnect));
		CheckDlgButton(IDC_SHOWOVERHEAD, static_cast<UINT>(thePrefs.m_bshowoverhead));
		CheckDlgButton(IDC_AUTOCONNECT, static_cast<UINT>(thePrefs.autoconnect));
		CheckDlgButton(IDC_RANDOMIZEPORTSONSTARTUP, static_cast<UINT>(thePrefs.IsRandomizePortsOnStartupEnabled()));
		CheckDlgButton(IDC_STARTUP_BIND_BLOCK, static_cast<UINT>(thePrefs.IsStartupBindBlockEnabled()));
		CheckDlgButton(IDC_NETWORK_KADEMLIA, static_cast<UINT>(thePrefs.GetNetworkKademlia()));
		GetDlgItem(IDC_NETWORK_KADEMLIA)->EnableWindow(thePrefs.GetUDPPort() > 0);
		CheckDlgButton(IDC_NETWORK_ED2K, static_cast<UINT>(thePrefs.networked2k));

		GetDlgItem(IDC_PREF_UPNPONSTART)->EnableWindow(TRUE);

		CheckDlgButton(IDC_PREF_UPNPONSTART, static_cast<UINT>(thePrefs.IsUPnPEnabled()));
		ShowLimitValues();
	}
}

BOOL CPPgConnection::OnApply()
{
	uint32 u = NormalizeBandwidthLimitInput(GetBandwidthEditValue(this, IDC_UPLOAD_CAP));
	uint32 v = NormalizeBandwidthLimitInput(GetBandwidthEditValue(this, IDC_DOWNLOAD_CAP));
	uint32 adjustedUp = (u == UNLIMITED) ? 0u : u;
	uint32 adjustedDown = (v == UNLIMITED) ? 0u : v;
	if (CheckUp(adjustedUp, adjustedDown))
		CheckDown(adjustedUp, adjustedDown);
	else if (CheckDown(adjustedUp, adjustedDown))
		CheckUp(adjustedUp, adjustedDown);
	u = (adjustedUp == 0) ? UNLIMITED : adjustedUp;
	v = (adjustedDown == 0) ? UNLIMITED : adjustedDown;

	const uint32 lastmaxgu = thePrefs.GetMaxGraphUploadRate(true);
	const uint32 lastmaxgd = thePrefs.GetMaxGraphDownloadRate();
	thePrefs.SetMaxUpload(u);
	thePrefs.SetMaxDownload(v);

	u = GetDlgItemInt(IDC_MAXSOURCEPERFILE, NULL, FALSE);
	thePrefs.maxsourceperfile = (u > INT_MAX ? 1 : u);

	bool bRestartApp = false;
	bool bBindRestartRequired = false;

	u = GetDlgItemInt(IDC_PORT, NULL, FALSE);
	uint16 nNewPort = (uint16)(u > _UI16_MAX ? 0 : u);
	if (nNewPort && nNewPort != thePrefs.port) {
		thePrefs.port = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.listensocket->Rebind();
		else
			bRestartApp = true;
	}

	u = GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);
	nNewPort = (uint16)(u > _UI16_MAX ? 0 : u);
	if (nNewPort != thePrefs.udpport) {
		thePrefs.udpport = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.clientudp->Rebind();
		else
			bRestartApp = true;
	}

	const CString strBindInterfaceId = GetSelectedBindInterfaceId();
	const CString strBindInterfaceName = GetSelectedBindInterfaceName();
	const CString strBindAddress = GetSelectedBindAddress();
	if (thePrefs.GetBindInterface().CompareNoCase(strBindInterfaceId)
		|| thePrefs.GetBindInterfaceName().CompareNoCase(strBindInterfaceName)
		|| thePrefs.GetConfiguredBindAddr().CompareNoCase(strBindAddress)) {
		// The P2P sockets use the resolved bind address during startup, so interface changes take effect after restart.
		thePrefs.SetBindNetworkSelection(strBindInterfaceId, strBindInterfaceName, strBindAddress);
		bBindRestartRequired = true;
	}
	if (thePrefs.IsStartupBindBlockEnabled() != (IsDlgButtonChecked(IDC_STARTUP_BIND_BLOCK) != 0)) {
		thePrefs.m_bBlockNetworkWhenBindUnavailableAtStartup = (IsDlgButtonChecked(IDC_STARTUP_BIND_BLOCK) != 0);
		bBindRestartRequired = true;
	}

	if (thePrefs.m_bshowoverhead != (IsDlgButtonChecked(IDC_SHOWOVERHEAD) != 0)) {
		thePrefs.m_bshowoverhead = !thePrefs.m_bshowoverhead;
		// free memory and reset overhead data counters
		theStats.ResetDownDatarateOverhead();
		theStats.ResetUpDatarateOverhead();
	}

	thePrefs.SetNetworkKademlia(IsDlgButtonChecked(IDC_NETWORK_KADEMLIA) != 0);

	thePrefs.SetNetworkED2K(IsDlgButtonChecked(IDC_NETWORK_ED2K) != 0);

	GetDlgItem(IDC_UDPPORT)->EnableWindow(!IsDlgButtonChecked(IDC_UDPDISABLE));

	thePrefs.autoconnect = IsDlgButtonChecked(IDC_AUTOCONNECT) != 0;
	thePrefs.reconnect = IsDlgButtonChecked(IDC_RECONN) != 0;
	thePrefs.m_bRandomizePortsOnStartup = (IsDlgButtonChecked(IDC_RANDOMIZEPORTSONSTARTUP) != 0);

	if (lastmaxgu != thePrefs.GetMaxGraphUploadRate(true))
		theApp.emuledlg->statisticswnd->SetARange(false, thePrefs.GetMaxGraphUploadRate(true));
	if (lastmaxgd != thePrefs.GetMaxGraphDownloadRate())
		theApp.emuledlg->statisticswnd->SetARange(true, thePrefs.GetMaxGraphDownloadRate());

	UINT tempcon;
	u = GetDlgItemInt(IDC_MAXCON, NULL, FALSE);
	if (u <= 0)
		tempcon = thePrefs.maxconnections;
	else
		tempcon = (u >= INT_MAX ? CPreferences::GetRecommendedMaxConnections() : u);
	thePrefs.maxconnections = tempcon;

	if (thePrefs.IsUPnPEnabled() != (IsDlgButtonChecked(IDC_PREF_UPNPONSTART) != 0)) {
		thePrefs.m_bEnableUPnP = !thePrefs.m_bEnableUPnP;
		if (thePrefs.m_bEnableUPnP)
			theApp.emuledlg->StartUPnP();
	}

	SetModified(FALSE);
	LoadSettings();

	theApp.emuledlg->ShowConnectionState();

	if (bRestartApp)
		LocMessageBox(IDS_NOPORTCHANGEPOSSIBLE, MB_OK, 0);
	else if (bBindRestartRequired)
		LocMessageBox(IDS_SETTINGCHANGED_RESTART, MB_OK, 0);

	ChangePorts(2);	//"Test ports" button enable/disable

	return CPropertyPage::OnApply();
}

void CPPgConnection::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_CONNECTION));
		SetDlgItemText(IDC_CAPACITIES_FRM, GetResString(IDS_SPEED_LIMITS));
		SetDlgItemText(IDC_DCAP_LBL, GetResString(IDS_PW_CON_DOWNLBL));
		SetDlgItemText(IDC_UCAP_LBL, GetResString(IDS_PW_CON_UPLBL));
		SetDlgItemText(IDC_LIMITS_FRM, GetResString(IDS_PW_CON_LIMITFRM));
		SetDlgItemText(IDC_DLIMIT_LBL, GetResString(IDS_PW_CON_DOWNLBL));
		SetDlgItemText(IDC_ULIMIT_LBL, GetResString(IDS_PW_CON_UPLBL));
		SetDlgItemText(IDC_CONNECTION_NETWORK, GetResString(IDS_NETWORK));
		SetDlgItemText(IDC_KBS2, GetResString(IDS_KBYTESPERSEC));
		SetDlgItemText(IDC_KBS3, GetResString(IDS_KBYTESPERSEC));
		SetDlgItemText(IDC_MAXCONN_FRM, GetResString(IDS_PW_CONLIMITS));
		SetDlgItemText(IDC_MAXCONLABEL, GetResString(IDS_PW_MAXC));
		SetDlgItemText(IDC_SHOWOVERHEAD, GetResString(IDS_SHOWOVERHEAD));
		SetDlgItemText(IDC_CLIENTPORT_FRM, GetResString(IDS_PW_CLIENTPORT));
		SetDlgItemText(IDC_MAXSRC_FRM, GetResString(IDS_PW_MAXSOURCES));
		SetDlgItemText(IDC_AUTOCONNECT, GetResString(IDS_PW_AUTOCON));
		SetDlgItemText(IDC_RECONN, GetResString(IDS_PW_RECON));
		SetDlgItemText(IDC_MAXSRCHARD_LBL, GetResString(IDS_HARDLIMIT));
		SetDlgItemText(IDC_UDPDISABLE, GetResString(IDS_UDPDISABLED));
		SetDlgItemText(IDC_RANDOMIZEPORTSONSTARTUP, GetResString(IDS_RANDOMIZEPORTSONSTARTUP));
		SetDlgItemText(IDC_STARTTEST, GetResString(IDS_STARTTEST));
		SetDlgItemText(IDC_PREF_UPNPONSTART, GetResString(IDS_UPNPSTART));
		SetDlgItemText(IDC_BIND_INTERFACE_LABEL, GetResString(IDS_BIND_INTERFACE) + _T(':'));
		SetDlgItemText(IDC_BIND_ADDRESS_LABEL, GetResString(IDS_BIND_ADDRESS) + _T(':'));
		SetDlgItemText(IDC_STARTUP_BIND_BLOCK, _T("Keep networking offline if the bind target is unavailable at startup"));
		ShowLimitValues();
	}
}

bool CPPgConnection::CheckUp(uint32 mUp, uint32 &mDown)
{
	if (mUp == 0 || mDown == 0)
		return false;
	uint32 uDown = mDown;
	if (mUp < 4 && mDown > mUp * 3)
		mDown = mUp * 3;
	else if (mUp < 10 && mDown > mUp * 4)
		mDown = mUp * 4;
	else if (mUp < 20 && mDown > mUp * 5)
		mDown = mUp * 5;
	return uDown != mDown;
}

bool CPPgConnection::CheckDown(uint32 &mUp, uint32 mDown)
{
	if (mUp == 0 || mDown == 0)
		return false;
	uint32 uUp = mUp;
	if (mDown < 13 && mUp * 3 < mDown)
		mUp = (mDown + 2) / 3;
	else if (mDown < 41 && mUp * 4 < mDown)
		mUp = (mDown + 3) / 4;
	else if (mUp < 20 && mUp * 5 < mDown)
		mUp = (mDown + 4) / 5;
	return uUp != mUp;
}

void CPPgConnection::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	SetModified(TRUE);

	uint32 maxup = m_ctlMaxUp.GetPos();
	uint32 maxdown = m_ctlMaxDown.GetPos();

	if (pScrollBar->GetSafeHwnd() == m_ctlMaxUp.m_hWnd) {
		if (CheckUp(maxup, maxdown)) {
			if (CheckDown(maxup, maxdown))
				m_ctlMaxUp.SetPos(maxup);
			m_ctlMaxDown.SetPos(maxdown);
		}
	} else { /*if (hWnd == m_ctlMaxDown.m_hWnd) { */
		if (CheckDown(maxup, maxdown)) {
			if (CheckUp(maxup, maxdown))
				m_ctlMaxDown.SetPos(maxdown);
			m_ctlMaxUp.SetPos(maxup);
		}
	}

	SetBandwidthEditValue(this, IDC_UPLOAD_CAP, maxup == 0 ? UNLIMITED : maxup);
	SetBandwidthEditValue(this, IDC_DOWNLOAD_CAP, maxdown == 0 ? UNLIMITED : maxdown);
	ShowLimitValues();
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPPgConnection::ShowLimitValues()
{
	CString buffer;
	const LONGLONG nUploadLimit = GetBandwidthEditValue(this, IDC_UPLOAD_CAP);
	if (nUploadLimit <= 0)
		buffer = GetResString(IDS_PW_UNLIMITED);
	else
		buffer.Format(_T("%I64d %s"), nUploadLimit, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
	SetDlgItemText(IDC_KBS4, buffer);

	const LONGLONG nDownloadLimit = GetBandwidthEditValue(this, IDC_DOWNLOAD_CAP);
	if (nDownloadLimit <= 0)
		buffer = GetResString(IDS_PW_UNLIMITED);
	else
		buffer.Format(_T("%I64d %s"), nDownloadLimit, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
	SetDlgItemText(IDC_KBS1, buffer);
}

void CPPgConnection::OnSettingsChange()
{
	SetModified(TRUE);
	ShowLimitValues();
}

void CPPgConnection::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Connection);
}

BOOL CPPgConnection::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgConnection::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgConnection::OnCbnSelChangeBindInterface()
{
	FillBindAddressCombo(GetSelectedBindAddress());
	SetModified();
}

void CPPgConnection::OnStartPortTest()
{
	uint16 tcp = (uint16)GetDlgItemInt(IDC_PORT, NULL, FALSE);
	uint16 udp = (uint16)GetDlgItemInt(IDC_UDPPORT, NULL, FALSE);

	TriggerPortTest(tcp, udp);
}

void CPPgConnection::SetRateSliderTicks(CSliderCtrl &rRate)
{
	rRate.ClearTics();
	int iMin, iMax;
	rRate.GetRange(iMin, iMax);
	int iDiff = iMax - iMin;
	if (iDiff > 0) {
		CRect rc;
		rRate.GetWindowRect(&rc);
		if (rc.Width() > 0) {
			int iTic;
			int iPixels = rc.Width() / iDiff;
			if (iPixels >= 6)
				iTic = 1;
			else {
				iTic = 10;
				while (rc.Width() / (iDiff / iTic) < 8)
					iTic *= 10;
			}
			if (iTic)
				for (int i = ((iMin + (iTic - 1)) / iTic) * iTic; i < iMax; i += iTic)
					rRate.SetTic(i);
			rRate.SetPageSize(iTic);
		}
	}
}
