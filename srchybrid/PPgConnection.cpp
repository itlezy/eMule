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

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
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
END_MESSAGE_MAP()

CPPgConnection::CPPgConnection()
	: CPropertyPage(CPPgConnection::IDD)
	, m_lastudp()
{
}

void CPPgConnection::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
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

	LoadSettings();
	Localize();
	UpdateToolTips();

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
		_T("Your configured downstream capacity. eMule uses this as an upper reference for sliders and bandwidth decisions.\r\n\r\n")
		_T("Set it close to your real usable line rate, not the marketing maximum."));
	m_toolTip.SetTool(this, IDC_UPLOAD_CAP,
		_T("Your configured upstream capacity. eMule uses this as an upper reference for upload control.\r\n\r\n")
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
	m_toolTip.SetTool(this, IDC_UDPDISABLE,
		_T("Disables eMule's UDP port and therefore disables UDP-dependent features such as Kad networking.\r\n\r\n")
		_T("Recommended: leave this off unless you must run TCP-only for a very specific network environment."));
	m_toolTip.SetTool(this, IDC_STARTTEST,
		_T("Launches the external connectivity test for the currently configured TCP and UDP ports.\r\n\r\n")
		_T("Use it after changing ports, bind settings, or router mapping rules."));
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
		SetDlgItemText(IDC_LIMITS_FRM, GetResString(IDS_SPEED_LIMITS));
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
		SetDlgItemText(IDC_STARTTEST, GetResString(IDS_STARTTEST));
		SetDlgItemText(IDC_PREF_UPNPONSTART, GetResString(IDS_UPNPSTART));
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
	if (m_hWnd)
		ShowLimitValues();
}

void CPPgConnection::ShowLimitValues()
{
	static LPCTSTR const pszFmt = _T("%i %s");
	CString buffer;
	const UINT uploadLimit = GetDlgItemInt(IDC_UPLOAD_CAP, NULL, FALSE);
	const UINT downloadLimit = GetDlgItemInt(IDC_DOWNLOAD_CAP, NULL, FALSE);

	buffer.Format(pszFmt, uploadLimit, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
	SetDlgItemText(IDC_KBS4, buffer);

	buffer.Format(pszFmt, downloadLimit, (LPCTSTR)GetResString(IDS_KBYTESPERSEC));
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
