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
#include "Scheduler.h"
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
#include "PPgWebServer.h"
#include "BindStartupPolicy.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	bool IsValidIPv4Literal(const CString &strAddress)
	{
		IN_ADDR addr = {};
		return InetPton(AF_INET, strAddress, &addr) == 1;
	}

	CString FormatRuntimeBindTarget(const CString &strInterfaceName, const CString &strBindAddress)
	{
		if (strInterfaceName.IsEmpty() && strBindAddress.IsEmpty())
			return _T("Any interface");
		if (strInterfaceName.IsEmpty())
			return strBindAddress;
		if (strBindAddress.IsEmpty())
			return strInterfaceName;

		CString strTarget(strInterfaceName);
		strTarget.AppendFormat(_T(" / %s"), (LPCTSTR)strBindAddress);
		return strTarget;
	}

	CString FormatFallbackBindStatus(const CString &strInterfaceName, const CString &strConfiguredAddress, EBindAddressResolveResult eResult)
	{
		CString strTarget = BindStartupPolicy::FormatConfiguredBindTarget(strInterfaceName, strInterfaceName, strConfiguredAddress);
		if (strTarget.IsEmpty())
			strTarget = _T("Any interface");

		switch (eResult) {
		case BARR_InterfaceNotFound:
			return _T("Active P2P bind: Any interface. The saved interface is unavailable: ") + strTarget;
		case BARR_InterfaceNameAmbiguous:
			return _T("Active P2P bind: Any interface. The saved interface name is ambiguous: ") + strTarget;
		case BARR_InterfaceHasNoAddress:
			return _T("Active P2P bind: Any interface. The saved interface has no usable IPv4 address: ") + strTarget;
		case BARR_AddressNotFoundOnInterface:
			return _T("Active P2P bind: Any interface. The saved bind IP is missing on the selected interface: ") + strTarget;
		case BARR_AddressNotFound:
			return _T("Active P2P bind: Any interface. The saved bind IP is missing on all live interfaces: ") + strTarget;
		case BARR_Default:
		case BARR_Resolved:
		default:
			return _T("Active P2P bind: Any interface.");
		}
	}

	bool TryGetPortValue(CWnd* pWnd, int nCtrlId, bool bAllowZero, uint16& outPort)
	{
		CString strValue;
		pWnd->GetDlgItemText(nCtrlId, strValue);
		strValue.Trim();
		if (strValue.IsEmpty())
			return false;

		LPTSTR pEnd = NULL;
		const unsigned long value = _tcstoul(strValue, &pEnd, 10);
		if (pEnd == strValue || *pEnd != _T('\0') || value > _UI16_MAX || (!bAllowZero && value == 0))
			return false;

		outPort = static_cast<uint16>(value);
		return true;
	}

	bool ValidatePortControl(CWnd* pWnd, int nCtrlId, bool bAllowZero, uint16& outPort)
	{
		if (TryGetPortValue(pWnd, nCtrlId, bAllowZero, outPort))
			return true;

		AfxMessageBox(GetResString(IDS_ERR_BADPORT), MB_ICONWARNING | MB_OK);
		CWnd* pEdit = pWnd->GetDlgItem(nCtrlId);
		if (pEdit != NULL) {
			pEdit->SetFocus();
			if (pEdit->IsKindOf(RUNTIME_CLASS(CEdit)))
				static_cast<CEdit*>(pEdit)->SetSel(0, -1);
		}
		return false;
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
	ON_BN_CLICKED(IDC_NETWORK_ED2K, OnSettingsChange)
	ON_BN_CLICKED(IDC_SHOWOVERHEAD, OnSettingsChange)
	ON_BN_CLICKED(IDC_NETWORK_KADEMLIA, OnSettingsChange)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_PREF_UPNPONSTART, OnSettingsChange)
	ON_BN_CLICKED(IDC_RANDOMIZE_PORTS_ON_STARTUP, OnSettingsChange)
	ON_BN_CLICKED(IDC_STARTUP_BIND_BLOCK, OnSettingsChange)
	ON_BN_CLICKED(IDC_EXIT_ON_BIND_LOSS, OnSettingsChange)
	ON_CBN_SELCHANGE(IDC_BIND_INTERFACE, OnCbnSelChangeBindInterface)
	ON_CBN_EDITCHANGE(IDC_BIND_INTERFACE, OnSettingsChange)
	ON_EN_CHANGE(IDC_BIND_ADDRESS, OnSettingsChange)
END_MESSAGE_MAP()

CPPgConnection::CPPgConnection()
	: CPropertyPage(CPPgConnection::IDD)
	, m_lastudp()
{
}

void CPPgConnection::LoadBindableInterfaces()
{
	m_bindInterfaces = CBindAddressResolver::GetBindableInterfaces();
}

void CPPgConnection::FillBindInterfaceCombo()
{
	if (!m_bindInterface.m_hWnd)
		return;

	m_bindInterface.ResetContent();
	for (size_t i = 0; i < m_bindInterfaces.size(); ++i) {
		const int iItem = m_bindInterface.AddString(m_bindInterfaces[i].strDisplayName);
		m_bindInterface.SetItemData(iItem, static_cast<DWORD_PTR>(i));
	}

	const CString strConfiguredInterface = thePrefs.GetBindInterface();
	if (strConfiguredInterface.IsEmpty())
		m_bindInterface.SetWindowText(CString());
	else {
		for (size_t i = 0; i < m_bindInterfaces.size(); ++i) {
			if (!m_bindInterfaces[i].strName.CompareNoCase(strConfiguredInterface)) {
				m_bindInterface.SetCurSel(static_cast<int>(i));
				SyncBindInterfaceEditTextFromSelection();
				return;
			}
		}
		m_bindInterface.SetWindowText(strConfiguredInterface);
	}
}

CString CPPgConnection::GetBindInterfaceText() const
{
	CString strInterface;
	m_bindInterface.GetWindowText(strInterface);
	strInterface.Trim();

	const int iSel = m_bindInterface.GetCurSel();
	if (iSel != CB_ERR) {
		const DWORD_PTR dwItemData = m_bindInterface.GetItemData(iSel);
		if (dwItemData < m_bindInterfaces.size()) {
			CString strDisplay;
			m_bindInterface.GetLBText(iSel, strDisplay);
			strDisplay.Trim();
			if (!strInterface.CompareNoCase(strDisplay))
				return m_bindInterfaces[dwItemData].strName;
		}
	}

	return strInterface;
}

CString CPPgConnection::GetBindAddressText() const
{
	CString strAddress;
	GetDlgItemText(IDC_BIND_ADDRESS, strAddress);
	strAddress.Trim();
	return strAddress;
}

void CPPgConnection::UpdateBindStatus()
{
	CString strStatus;
	if (theApp.IsStartupBindBlocked())
		strStatus = theApp.GetStartupBindBlockReason();
	else if (thePrefs.GetActiveBindAddressResolveResult() == BARR_Default)
		strStatus = _T("Active P2P bind: Any interface.");
	else if (thePrefs.GetActiveBindAddressResolveResult() == BARR_Resolved) {
		CString strRuntimeBindAddress;
		if (thePrefs.GetBindAddr() != NULL)
			strRuntimeBindAddress = thePrefs.GetBindAddr();
		strStatus = _T("Active P2P bind: ") + FormatRuntimeBindTarget(thePrefs.GetActiveBindInterfaceName(), strRuntimeBindAddress);
	}
	else
		strStatus = FormatFallbackBindStatus(thePrefs.GetActiveBindInterfaceName(), thePrefs.GetActiveConfiguredBindAddr(), thePrefs.GetActiveBindAddressResolveResult());

	SetDlgItemText(IDC_BIND_STATUS, strStatus);
}

void CPPgConnection::UpdateBindProtectionControls()
{
	const bool bHasSpecificInterface = !GetBindInterfaceText().IsEmpty();
	if (!bHasSpecificInterface) {
		CheckDlgButton(IDC_STARTUP_BIND_BLOCK, 0);
		CheckDlgButton(IDC_EXIT_ON_BIND_LOSS, 0);
	}
	GetDlgItem(IDC_STARTUP_BIND_BLOCK)->EnableWindow(bHasSpecificInterface);
	GetDlgItem(IDC_EXIT_ON_BIND_LOSS)->EnableWindow(bHasSpecificInterface);
}

void CPPgConnection::UpdateRestartRequiredNotice()
{
	CString strRestartNote;
	const CString strPendingInterface = GetBindInterfaceText();
	const CString strPendingAddress = GetBindAddressText();
	const bool bPendingStartupBlock = !strPendingInterface.IsEmpty() && (IsDlgButtonChecked(IDC_STARTUP_BIND_BLOCK) != 0);
	if (thePrefs.GetActiveBindInterface().CompareNoCase(strPendingInterface)
		|| thePrefs.GetActiveConfiguredBindAddr().CompareNoCase(strPendingAddress)
		|| thePrefs.IsActiveStartupBindBlockEnabled() != bPendingStartupBlock) {
		CString strResolvedAddress;
		CString strResolvedInterfaceName;
		const EBindAddressResolveResult ePendingResult = CBindAddressResolver::ResolveBindAddress(strPendingInterface
			, strPendingAddress, strResolvedAddress, &strResolvedInterfaceName);
		strRestartNote = _T("Restart required: the new bind settings take effect only after restarting eMule.");
		if (BindStartupPolicy::ShouldBlockSessionNetworking(bPendingStartupBlock, strPendingInterface, strPendingAddress, ePendingResult)) {
			CString strReason = BindStartupPolicy::FormatStartupBlockReason(strResolvedInterfaceName.IsEmpty() ? strPendingInterface : strResolvedInterfaceName
				, strPendingInterface, strPendingAddress, ePendingResult);
			if (!strReason.IsEmpty())
				strRestartNote.AppendFormat(_T(" %s"), (LPCTSTR)strReason);
		} else if (ePendingResult != BARR_Default && ePendingResult != BARR_Resolved) {
			strRestartNote.Append(_T(" Restart will fall back to the default interface because the saved bind target is unresolved."));
		}
	}

	SetDlgItemText(IDC_BIND_RESTART_NOTE, strRestartNote);
	if (CWnd *pRestartNote = GetDlgItem(IDC_BIND_RESTART_NOTE))
		pRestartNote->ShowWindow(strRestartNote.IsEmpty() ? SW_HIDE : SW_SHOW);
}

void CPPgConnection::SyncBindInterfaceEditTextFromSelection()
{
	const int iSel = m_bindInterface.GetCurSel();
	if (iSel == CB_ERR)
		return;

	const DWORD_PTR dwItemData = m_bindInterface.GetItemData(iSel);
	if (dwItemData < m_bindInterfaces.size())
		m_bindInterface.SetWindowText(m_bindInterfaces[dwItemData].strName);
}

void CPPgConnection::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BIND_INTERFACE, m_bindInterface);
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
	uint16 tcp = 0;
	const bool bValidTcp = TryGetPortValue(this, IDC_PORT, false, tcp);
	uint16 udp = 0;
	const bool bAllowZeroUdp = IsDlgButtonChecked(IDC_UDPDISABLE) != 0;
	const bool bValidUdp = TryGetPortValue(this, IDC_UDPPORT, bAllowZeroUdp, udp);

	GetDlgItem(IDC_STARTTEST)->EnableWindow(
		bValidTcp
		&& bValidUdp
		&& tcp == theApp.listensocket->GetConnectedPort()
		&& udp == theApp.clientudp->GetConnectedPort()
	);

	if (iWhat == 0) { //UDP
		if (bValidUdp && udp != 0)
			m_lastudp = udp;
		if (bValidTcp && bValidUdp && (tcp != thePrefs.port || udp != thePrefs.udpport))
			OnSettingsChange();
	}
	else if (iWhat == 1) //TCP
		if (bValidTcp && bValidUdp && (tcp != thePrefs.port || udp != thePrefs.udpport))
			OnSettingsChange();
	//else if (iWhat == 2) "Test ports" button enable/disable - done already
}

bool CPPgConnection::ChangeUDP()
{
	bool bDisabled = IsDlgButtonChecked(IDC_UDPDISABLE) != 0;
	GetDlgItem(IDC_UDPPORT)->EnableWindow(!bDisabled);

	uint16 newVal;
	uint16 oldVal = 0;
	const bool bValidOldVal = TryGetPortValue(this, IDC_UDPPORT, bDisabled, oldVal);
	if (bValidOldVal && oldVal)
		m_lastudp = oldVal;
	if (bDisabled)
		newVal = 0;
	else
		newVal = m_lastudp ? m_lastudp : (10ui16 + thePrefs.port);
	if (!bValidOldVal || newVal != oldVal)
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

	static_cast<CEdit*>(GetDlgItem(IDC_PORT))->SetLimitText(5);
	static_cast<CEdit*>(GetDlgItem(IDC_UDPPORT))->SetLimitText(5);
	static_cast<CEdit*>(GetDlgItem(IDC_BIND_ADDRESS))->SetLimitText(15);

	LoadBindableInterfaces();
	LoadSettings();
	Localize();
	UpdateToolTips();
	UpdateBindStatus();
	UpdateRestartRequiredNotice();
	SetModified(FALSE);

	ChangePorts(2); //"Test ports" button enable/disable

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgConnection::UpdateToolTips()
{
	if (!m_toolTip.Init(this))
		return;

	m_toolTip.SetTool(this, IDC_AUTOCONNECT,
		_T("Automatically starts the enabled networks when eMule launches.\r\n\r\n")
		_T("Recommended: enabled for normal unattended use. Disable it only if you want to connect manually every session."));
	m_toolTip.SetTool(this, IDC_RECONN,
		_T("Retries server connections after disconnects or failed attempts.\r\n\r\n")
		_T("Recommended: enabled if you use the eD2K server network. It has no effect when server networking is disabled."));
	m_toolTip.SetTool(this, IDC_SHOWOVERHEAD,
		_T("Shows protocol overhead separately in the transfer rates and statistics.\r\n\r\n")
		_T("Enable it if you want a more honest view of wire traffic. Leave it off if you prefer simpler payload-only numbers."));
	m_toolTip.SetTool(this, IDC_PORT,
		_T("Main TCP port used for incoming client connections.\r\n\r\n")
		_T("Change it only when you intentionally reconfigure firewall or router rules. After changing it, retest connectivity."));
	m_toolTip.SetTool(this, IDC_UDPPORT,
		_T("UDP port used for Kad and other UDP-assisted features.\r\n\r\n")
		_T("Recommended: keep it enabled and reachable unless you intentionally want to disable UDP-based networking."));
	m_toolTip.SetTool(this, IDC_MAXSOURCEPERFILE,
		_T("Hard limit for how many remote sources eMule keeps per download.\r\n\r\n")
		_T("Higher values can improve availability for rare files but cost more memory and source-management traffic. Use moderate values unless you have a specific reason to raise it."));
	m_toolTip.SetTool(this, IDC_MAXCON,
		_T("Upper limit for simultaneous connections eMule may keep open.\r\n\r\n")
		_T("Higher values are not automatically better. Leave this near the tuned default unless you are diagnosing a specific network limitation."));
	m_toolTip.SetTool(this, IDC_DOWNLOAD_CAP,
		_T("Your configured downstream capacity in KiB/s. eMule uses this as an upper reference for sliders and bandwidth decisions.\r\n\r\n")
		_T("Set it close to your real usable line rate, not the marketing maximum."));
	m_toolTip.SetTool(this, IDC_UPLOAD_CAP,
		_T("Your configured upstream capacity in KiB/s. eMule uses this as an upper reference for upload control.\r\n\r\n")
		_T("Set it close to the real usable sustained upload rate of your line."));
	m_toolTip.SetTool(this, IDC_NETWORK_ED2K,
		_T("Enables the classic eD2K server network.\r\n\r\n")
		_T("Leave it enabled if you still use server-based source discovery. Disable it only if you intentionally want Kad-only operation."));
	m_toolTip.SetTool(this, IDC_NETWORK_KADEMLIA,
		_T("Enables the Kad distributed network.\r\n\r\n")
		_T("Recommended: enabled for modern use. It requires the UDP port to stay enabled."));
	m_toolTip.SetTool(this, IDC_PREF_UPNPONSTART,
		_T("Attempts automatic NAT port mapping on startup for the configured TCP and UDP ports.\r\n\r\n")
		_T("Recommended: enabled when your router supports UPnP or PCP/NAT-PMP. Disable it if you forward ports manually."));
	m_toolTip.SetTool(this, IDC_RANDOMIZE_PORTS_ON_STARTUP,
		_T("Chooses new TCP and UDP listen ports at startup before automatic port mapping runs.\r\n\r\n")
		_T("The option uses eMule's existing random port range and leaves UDP disabled when the UDP port is disabled."));
	m_toolTip.SetTool(this, IDC_UDPDISABLE,
		_T("Disables eMule's UDP port and therefore disables UDP-dependent features such as Kad networking.\r\n\r\n")
		_T("Recommended: leave this off unless you must run TCP-only for a very specific network environment."));
	m_toolTip.SetTool(this, IDC_STARTTEST,
		_T("Launches the external connectivity test for the currently configured TCP and UDP ports.\r\n\r\n")
		_T("Use it after changing ports, bind settings, or router mapping rules."));
	m_toolTip.SetTool(this, IDC_BIND_INTERFACE,
		_T("Optional Windows network interface name used for P2P socket binding.\r\n\r\n")
		_T("Match is by friendly interface name, not adapter number. Changes take effect after restart."));
	m_toolTip.SetTool(this, IDC_BIND_ADDRESS,
		_T("Optional IPv4 address used for P2P socket binding.\r\n\r\n")
		_T("Leave it empty to use the first IPv4 on the selected interface, or the default routing choice when no interface is selected."));
	m_toolTip.SetTool(this, IDC_STARTUP_BIND_BLOCK,
		_T("Keeps P2P networking offline for the session if the configured bind target cannot be resolved at startup.\r\n\r\n")
		_T("Available only when a bind interface is selected. Recommended when you explicitly bind to a named interface and do not want silent fallback."));
	m_toolTip.SetTool(this, IDC_EXIT_ON_BIND_LOSS,
		_T("Exits eMule if the selected bind interface disappears after startup.\r\n\r\n")
		_T("Available only when a bind interface is selected. Use this as VPN protection when P2P networking is bound to a VPN adapter."));
}

void CPPgConnection::LoadSettings()
{
	if (m_hWnd) {
		m_lastudp = thePrefs.udpport;
		CheckDlgButton(IDC_UDPDISABLE, !m_lastudp); //before the port number!
		SetDlgItemInt(IDC_UDPPORT, m_lastudp, FALSE);

		const uint32 downloadLimit = thePrefs.GetMaxDownload();
		const uint32 uploadLimit = thePrefs.GetMaxUpload();

		SetDlgItemInt(IDC_DOWNLOAD_CAP, downloadLimit);
		SetDlgItemInt(IDC_UPLOAD_CAP, uploadLimit);

		SetDlgItemInt(IDC_PORT, thePrefs.port, FALSE);
		SetDlgItemInt(IDC_MAXCON, thePrefs.maxconnections);
		SetDlgItemInt(IDC_MAXSOURCEPERFILE, (thePrefs.maxsourceperfile == 0xffff ? 0 : thePrefs.maxsourceperfile));

		CheckDlgButton(IDC_RECONN, static_cast<UINT>(thePrefs.reconnect));
		CheckDlgButton(IDC_SHOWOVERHEAD, static_cast<UINT>(thePrefs.m_bshowoverhead));
		CheckDlgButton(IDC_AUTOCONNECT, static_cast<UINT>(thePrefs.autoconnect));
		CheckDlgButton(IDC_NETWORK_KADEMLIA, static_cast<UINT>(thePrefs.GetNetworkKademlia()));
		GetDlgItem(IDC_NETWORK_KADEMLIA)->EnableWindow(thePrefs.GetUDPPort() > 0);
		CheckDlgButton(IDC_NETWORK_ED2K, static_cast<UINT>(thePrefs.networked2k));

		GetDlgItem(IDC_PREF_UPNPONSTART)->EnableWindow(TRUE);

		CheckDlgButton(IDC_PREF_UPNPONSTART, static_cast<UINT>(thePrefs.IsUPnPEnabled()));
		CheckDlgButton(IDC_RANDOMIZE_PORTS_ON_STARTUP, static_cast<UINT>(thePrefs.IsPortRandomizationOnStartupEnabled()));

		FillBindInterfaceCombo();
		SetDlgItemText(IDC_BIND_ADDRESS, thePrefs.GetConfiguredBindAddr());
		CheckDlgButton(IDC_STARTUP_BIND_BLOCK, static_cast<UINT>(thePrefs.IsStartupBindBlockEnabled()));
		CheckDlgButton(IDC_EXIT_ON_BIND_LOSS, static_cast<UINT>(thePrefs.IsExitOnBindInterfaceLossEnabled()));
		UpdateBindProtectionControls();

		ShowLimitValues();
	}
}

BOOL CPPgConnection::OnApply()
{
	UINT v = GetDlgItemInt(IDC_DOWNLOAD_CAP, NULL, FALSE);
	if (v == 0 || v >= UNLIMITED) {
		GetDlgItem(IDC_DOWNLOAD_CAP)->SetFocus();
		return FALSE;
	}
	UINT u = GetDlgItemInt(IDC_UPLOAD_CAP, NULL, FALSE);
	if (u == 0 || u >= UNLIMITED) {
		GetDlgItem(IDC_UPLOAD_CAP)->SetFocus();
		return FALSE;
	}

	uint32 lastmaxgu = thePrefs.GetMaxUpload(); // save the values
	uint32 lastmaxgd = thePrefs.maxGraphDownloadRate;
	bool bBindRestartRequired = false;

	thePrefs.SetMaxDownload(v);
	SetDlgItemInt(IDC_DOWNLOAD_CAP, thePrefs.GetMaxDownload(), FALSE);

	thePrefs.SetMaxUpload(u);
	SetDlgItemInt(IDC_UPLOAD_CAP, thePrefs.GetMaxUpload(), FALSE);

	u = GetDlgItemInt(IDC_MAXSOURCEPERFILE, NULL, FALSE);
	thePrefs.SetMaxSourcesPerFile((u > INT_MAX) ? CPreferences::GetDefaultMaxSourcesPerFile() : u);

	bool bRestartApp = false;
	uint16 nNewPort = 0;

	if (!ValidatePortControl(this, IDC_PORT, false, nNewPort))
		return FALSE;
	if (nNewPort && nNewPort != thePrefs.port) {
		thePrefs.port = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.listensocket->Rebind();
		else
			bRestartApp = true;
	}

	if (IsDlgButtonChecked(IDC_UDPDISABLE) != 0)
		nNewPort = 0;
	else if (!ValidatePortControl(this, IDC_UDPPORT, false, nNewPort))
		return FALSE;
	if (nNewPort != thePrefs.udpport) {
		thePrefs.udpport = nNewPort;
		if (theApp.IsPortchangeAllowed())
			theApp.clientudp->Rebind();
		else
			bRestartApp = true;
	}

	const CString strBindInterface = GetBindInterfaceText();
	const CString strBindAddress = GetBindAddressText();
	if (!strBindAddress.IsEmpty() && !IsValidIPv4Literal(strBindAddress)) {
		AfxMessageBox(_T("BindAddr must be empty or a valid IPv4 address."), MB_ICONWARNING | MB_OK);
		GetDlgItem(IDC_BIND_ADDRESS)->SetFocus();
		static_cast<CEdit*>(GetDlgItem(IDC_BIND_ADDRESS))->SetSel(0, -1);
		return FALSE;
	}
	if (thePrefs.GetBindInterface().CompareNoCase(strBindInterface)
		|| thePrefs.GetConfiguredBindAddr().CompareNoCase(strBindAddress)) {
		thePrefs.SetBindNetworkSelection(strBindInterface, strBindAddress);
		bBindRestartRequired = true;
	}
	const bool bHasSpecificBindInterface = !strBindInterface.IsEmpty();
	const bool bStartupBindBlock = bHasSpecificBindInterface && (IsDlgButtonChecked(IDC_STARTUP_BIND_BLOCK) != 0);
	if (thePrefs.IsStartupBindBlockEnabled() != bStartupBindBlock) {
		thePrefs.m_bBlockNetworkWhenBindUnavailableAtStartup = bStartupBindBlock;
		bBindRestartRequired = true;
	}
	thePrefs.SetExitOnBindInterfaceLossEnabled(bHasSpecificBindInterface && (IsDlgButtonChecked(IDC_EXIT_ON_BIND_LOSS) != 0));

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
	thePrefs.SetPortRandomizationOnStartupEnabled(IsDlgButtonChecked(IDC_RANDOMIZE_PORTS_ON_STARTUP) != 0);

	if (lastmaxgu != thePrefs.GetMaxUpload())
		theApp.emuledlg->statisticswnd->SetARange(false, thePrefs.GetMaxUpload());
	if (lastmaxgd != thePrefs.maxGraphDownloadRate)
		theApp.emuledlg->statisticswnd->SetARange(true, thePrefs.maxGraphDownloadRate);

	UINT tempcon;
	u = GetDlgItemInt(IDC_MAXCON, NULL, FALSE);
	if (u <= 0)
		tempcon = thePrefs.maxconnections;
	else
		tempcon = (u >= INT_MAX ? CPreferences::GetRecommendedMaxConnections() : u);

	if (tempcon > GetMaxWindowsTCPConnections()) {
		CString strMessage;
		strMessage.Format(GetResString(IDS_PW_WARNING), (LPCTSTR)GetResString(IDS_PW_MAXC), GetMaxWindowsTCPConnections());
		int iResult = AfxMessageBox(strMessage, MB_ICONWARNING | MB_YESNO);
		if (iResult != IDYES) {
			//TODO: set focus to max connection?
			SetDlgItemInt(IDC_MAXCON, thePrefs.maxconnections);
			tempcon = GetMaxWindowsTCPConnections();
		}
	}
	thePrefs.SetMaxConnections(tempcon);

	if (thePrefs.IsUPnPEnabled() != (IsDlgButtonChecked(IDC_PREF_UPNPONSTART) != 0)) {
		thePrefs.m_bEnableUPnP = !thePrefs.m_bEnableUPnP;
		if (thePrefs.m_bEnableUPnP)
			theApp.emuledlg->StartUPnP();
		if (theApp.emuledlg->preferenceswnd->m_wndWebServer)
			theApp.emuledlg->preferenceswnd->m_wndWebServer.SetUPnPState();
	}

	theApp.scheduler->SaveOriginals();

	SetModified(FALSE);
	LoadSettings();
	SetModified(FALSE);
	UpdateBindStatus();
	UpdateRestartRequiredNotice();
	theApp.emuledlg->UpdateBindLossMonitor();

	theApp.emuledlg->ShowConnectionState();

	if (bRestartApp)
		LocMessageBox(IDS_NOPORTCHANGEPOSSIBLE, MB_OK, 0);

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
		SetDlgItemText(IDC_LIMITS_FRM, _T("Equivalent"));
		SetDlgItemText(IDC_DLIMIT_LBL, GetResString(IDS_PW_CON_DOWNLBL));
		SetDlgItemText(IDC_ULIMIT_LBL, GetResString(IDS_PW_CON_UPLBL));
		SetDlgItemText(IDC_CONNECTION_NETWORK, GetResString(IDS_NETWORK));
		SetDlgItemText(IDC_KBS2, _T("KiB/s"));
		SetDlgItemText(IDC_KBS3, _T("KiB/s"));
		SetDlgItemText(IDC_MAXCONN_FRM, GetResString(IDS_PW_CONLIMITS));
		SetDlgItemText(IDC_MAXCONLABEL, GetResString(IDS_PW_MAXC));
		SetDlgItemText(IDC_SHOWOVERHEAD, GetResString(IDS_SHOWOVERHEAD));
		SetDlgItemText(IDC_CLIENTPORT_FRM, GetResString(IDS_PW_CLIENTPORT));
		SetDlgItemText(IDC_MAXSRC_FRM, GetResString(IDS_PW_MAXSOURCES));
		SetDlgItemText(IDC_AUTOCONNECT, GetResString(IDS_PW_AUTOCON));
		SetDlgItemText(IDC_RECONN, GetResString(IDS_PW_RECON));
		SetDlgItemText(IDC_MAXSRCHARD_LBL, GetResString(IDS_HARDLIMIT));
		SetDlgItemText(IDC_UDPDISABLE, GetResString(IDS_UDPDISABLED));
		SetDlgItemText(IDC_STARTTEST, GetResString(IDS_STARTTEST));
		SetDlgItemText(IDC_PREF_UPNPONSTART, GetResString(IDS_UPNPSTART));
		SetDlgItemText(IDC_RANDOMIZE_PORTS_ON_STARTUP, _T("Randomize listen ports on startup"));
		SetDlgItemText(IDC_BIND_INTERFACE_LABEL, GetResString(IDS_BIND_INTERFACE));
		SetDlgItemText(IDC_BIND_ADDRESS_LABEL, GetResString(IDS_BIND_ADDRESS));
		SetDlgItemText(IDC_STARTUP_BIND_BLOCK, _T("Keep networking offline if the bind target is unavailable at startup"));
		SetDlgItemText(IDC_EXIT_ON_BIND_LOSS, _T("Exit eMule if the bound interface is lost"));
		ShowLimitValues();
	}
}

bool CPPgConnection::CheckUp(uint32 mUp, uint32 &mDown)
{
	(void)mUp;
	(void)mDown;
	return false;
}

bool CPPgConnection::CheckDown(uint32 &mUp, uint32 mDown)
{
	(void)mUp;
	(void)mDown;
	return false;
}

void CPPgConnection::OnSettingsChange()
{
	SetModified();
	if (m_hWnd) {
		ShowLimitValues();
		UpdateBindProtectionControls();
		UpdateRestartRequiredNotice();
	}
}

void CPPgConnection::OnCbnSelChangeBindInterface()
{
	SyncBindInterfaceEditTextFromSelection();
	OnSettingsChange();
}

void CPPgConnection::ShowLimitValues()
{
	static constexpr double kKiBPerMiB = 1024.0;
	CString buffer;
	const UINT uploadLimit = GetDlgItemInt(IDC_UPLOAD_CAP, NULL, FALSE);
	const UINT downloadLimit = GetDlgItemInt(IDC_DOWNLOAD_CAP, NULL, FALSE);

	buffer.Format(_T("%.2f MiB/s"), static_cast<double>(uploadLimit) / kKiBPerMiB);
	SetDlgItemText(IDC_KBS4, buffer);

	buffer.Format(_T("%.2f MiB/s"), static_cast<double>(downloadLimit) / kKiBPerMiB);
	SetDlgItemText(IDC_KBS1, buffer);
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

BOOL CPPgConnection::PreTranslateMessage(MSG *pMsg)
{
	m_toolTip.RelayEvent(pMsg);
	return CPropertyPage::PreTranslateMessage(pMsg);
}

void CPPgConnection::OnStartPortTest()
{
	uint16 tcp = 0;
	if (!ValidatePortControl(this, IDC_PORT, false, tcp))
		return;

	uint16 udp = 0;
	if (IsDlgButtonChecked(IDC_UDPDISABLE) == 0 && !ValidatePortControl(this, IDC_UDPPORT, false, udp))
		return;

	TriggerPortTest(tcp, udp);
}
