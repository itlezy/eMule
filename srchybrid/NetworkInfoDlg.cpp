#include "stdafx.h"
#include "emule.h"
#include "NetworkInfoDlg.h"
#include "RichEditCtrlX.h"
#include "OtherFunctions.h"
#include "ServerConnect.h"
#include "Preferences.h"
#include "ServerList.h"
#include "Server.h"
#include "kademlia/kademlia/kademlia.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/kademlia/indexed.h"
#include "clientlist.h"
#include "StatusBarInfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	PREF_INI_SECTION	_T("NetworkInfoDlg")

namespace
{
	/**
	 * Formats a label caption with the conventional trailing colon.
	 */
	CString MakeLabelText(const CString &strBaseLabel)
	{
		return strBaseLabel + _T(":");
	}

	/**
	 * Normalizes empty summary values to a visible placeholder.
	 */
	CString NormalizeSummaryValue(const CString &strValue)
	{
		return strValue.IsEmpty() ? CString(_T("-")) : strValue;
	}

	/**
	 * Formats a resolved bind target using the configured interface name when available.
	 */
	CString FormatBindSummary()
	{
		CString strBindSummary;
		if (!thePrefs.GetBindInterfaceName().IsEmpty())
			strBindSummary = thePrefs.GetBindInterfaceName();

		CString strResolvedAddress;
		if (thePrefs.GetBindAddr() != NULL)
			strResolvedAddress = thePrefs.GetBindAddr();
		else if (!thePrefs.GetConfiguredBindAddr().IsEmpty())
			strResolvedAddress = thePrefs.GetConfiguredBindAddr();

		if (!strResolvedAddress.IsEmpty()) {
			if (!strBindSummary.IsEmpty())
				strBindSummary.AppendFormat(_T(" (%s)"), (LPCTSTR)strResolvedAddress);
			else
				strBindSummary = strResolvedAddress;
		}

		if (strBindSummary.IsEmpty())
			strBindSummary = _T("Any interface");
		return strBindSummary;
	}

	/**
	 * Formats the eD2K public endpoint using the app's stored IPv4 byte order.
	 */
	CString FormatED2KPublicAddress()
	{
		if (!theApp.serverconnect->IsConnected())
			return CString();

		if (theApp.serverconnect->IsLowID() && theApp.GetED2KPublicIP() == 0)
			return GetResString(IDS_UNKNOWN);

		CString strAddress;
		strAddress.Format(_T("%s:%u")
			, (LPCTSTR)StatusBarInfo::Detail::FormatStoredIPv4Address(theApp.GetED2KPublicIP())
			, thePrefs.GetPort());
		return strAddress;
	}

	/**
	 * Formats the active eD2K server name and endpoint on one line.
	 */
	CString FormatED2KServer()
	{
		if (!theApp.serverconnect->IsConnected())
			return CString();

		CServer *pCurrentServer = theApp.serverconnect->GetCurrentServer();
		CServer *pServer = pCurrentServer ? theApp.serverlist->GetServerByAddress(pCurrentServer->GetAddress(), pCurrentServer->GetPort()) : NULL;
		if (pServer == NULL)
			return CString();

		CString strServer;
		strServer.Format(_T("%s (%s:%u)")
			, (LPCTSTR)pServer->GetListName()
			, (LPCTSTR)pServer->GetAddress()
			, pServer->GetPort());
		return strServer;
	}

	/**
	 * Formats the current Kademlia state, including LAN mode when active.
	 */
	CString FormatKadStatus()
	{
		if (!Kademlia::CKademlia::IsConnected())
			return GetResString(Kademlia::CKademlia::IsRunning() ? IDS_CONNECTING : IDS_DISCONNECTED);

		CString strStatus(GetResString(Kademlia::CKademlia::IsFirewalled() ? IDS_FIREWALLED : IDS_KADOPEN));
		if (Kademlia::CKademlia::IsRunningInLANMode())
			strStatus.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_LANMODE));
		return strStatus;
	}

	/**
	 * Formats the Kademlia UDP reachability summary.
	 */
	CString FormatKadUdpStatus()
	{
		if (!Kademlia::CKademlia::IsConnected())
			return CString();

		if (Kademlia::CUDPFirewallTester::IsFirewalledUDP(true))
			return GetResString(IDS_FIREWALLED);

		CString strStatus(GetResString(IDS_KADOPEN));
		if (!Kademlia::CUDPFirewallTester::IsVerified())
			strStatus.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_UNVERIFIED).MakeLower());
		return strStatus;
	}

	/**
	 * Formats the runtime Kademlia endpoint.
	 */
	CString FormatKadAddress()
	{
		if (!Kademlia::CKademlia::IsConnected())
			return CString();

		CString strAddress;
		strAddress.Format(_T("%s:%u")
			, (LPCTSTR)ipstr(htonl(Kademlia::CKademlia::GetPrefs()->GetIPAddress()))
			, thePrefs.GetUDPPort());
		return strAddress;
	}

	/**
	 * Formats the current buddy status when Kademlia is firewalled.
	 */
	CString FormatKadBuddyStatus()
	{
		if (!Kademlia::CKademlia::IsConnected() || !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true))
			return CString();

		UINT uStringId = 0;
		switch (theApp.clientlist->GetBuddyStatus()) {
		case Disconnected:
			uStringId = IDS_BUDDYNONE;
			break;
		case Connecting:
			uStringId = IDS_CONNECTING;
			break;
		case Connected:
			uStringId = IDS_CONNECTED;
			break;
		default:
			break;
		}
		return uStringId != 0 ? GetResString(uStringId) : CString();
	}

	/**
	 * Formats the connected Kademlia user/file counters as a compact summary.
	 */
	CString FormatKadCounts()
	{
		if (!Kademlia::CKademlia::IsConnected())
			return CString();

		CString strCounts;
		strCounts.Format(_T("%s / %s")
			, (LPCTSTR)GetFormatedUInt(Kademlia::CKademlia::GetKademliaUsers())
			, (LPCTSTR)GetFormatedUInt(Kademlia::CKademlia::GetKademliaFiles()));
		return strCounts;
	}

	/**
	 * Appends a labeled plain-text line for clipboard export.
	 */
	void AppendClipboardLine(CString &strReport, const CString &strLabel, const CString &strValue)
	{
		strReport.AppendFormat(_T("%s: %s\r\n"), (LPCTSTR)strLabel, (LPCTSTR)NormalizeSummaryValue(strValue));
	}
}

IMPLEMENT_DYNAMIC(CNetworkInfoDlg, CDialog)

BEGIN_MESSAGE_MAP(CNetworkInfoDlg, CResizableDialog)
	ON_BN_CLICKED(IDC_NETWORK_RELOAD, OnBnClickedReload)
	ON_BN_CLICKED(IDC_NETWORK_COPY, OnBnClickedCopy)
END_MESSAGE_MAP()

CNetworkInfoDlg::CNetworkInfoDlg(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CNetworkInfoDlg::IDD, pParent)
	, m_cfDef()
	, m_cfBold()
{
}

void CNetworkInfoDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NETWORK_INFO, m_info);
}

BOOL CNetworkInfoDlg::OnInitDialog()
{
	CResizableDialog::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_NETWORK_CLIENT_GROUP, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_NETWORK_DETAILS_GROUP, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_NETWORK_INFO, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_NETWORK_RELOAD, BOTTOM_RIGHT);
	AddAnchor(IDC_NETWORK_COPY, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	EnableSaveRestore(PREF_INI_SECTION);

	SetWindowText(GetResString(IDS_NETWORK_INFO));
	SetDlgItemText(IDOK, GetResString(IDS_TREEOPTIONS_OK));
	SetDlgItemText(IDC_NETWORK_RELOAD, GetResString(IDS_SF_RELOAD));
	SetDlgItemText(IDC_NETWORK_COPY, GetResString(IDS_COPY));
	SetDlgItemText(IDC_NETWORK_CLIENT_GROUP, GetResString(IDS_CLIENT));
	SetDlgItemText(IDC_NETWORK_ED2K_GROUP, CString(_T("eD2K ")) + GetResString(IDS_NETWORK));
	SetDlgItemText(IDC_NETWORK_KAD_GROUP, GetResString(IDS_KADEMLIA) + _T(" ") + GetResString(IDS_NETWORK));
	SetDlgItemText(IDC_NETWORK_DETAILS_GROUP, GetResString(IDS_DETAILS));
	SetDlgItemText(IDC_NETWORK_CLIENT_NICK_LABEL, MakeLabelText(GetResString(IDS_PW_NICK)));
	SetDlgItemText(IDC_NETWORK_CLIENT_HASH_LABEL, MakeLabelText(GetResString(IDS_CD_UHASH)));
	SetDlgItemText(IDC_NETWORK_CLIENT_TCP_LABEL, MakeLabelText(CString(_T("TCP ")) + GetResString(IDS_PORT)));
	SetDlgItemText(IDC_NETWORK_CLIENT_UDP_LABEL, MakeLabelText(CString(_T("UDP ")) + GetResString(IDS_PORT)));
	SetDlgItemText(IDC_NETWORK_CLIENT_BIND_LABEL, MakeLabelText(GetResString(IDS_BIND_INTERFACE)));
	SetDlgItemText(IDC_NETWORK_ED2K_STATUS_LABEL, MakeLabelText(GetResString(IDS_STATUS)));
	SetDlgItemText(IDC_NETWORK_ED2K_ADDRESS_LABEL, MakeLabelText(GetResString(IDS_IP) + _T(":") + GetResString(IDS_PORT)));
	SetDlgItemText(IDC_NETWORK_ED2K_ID_LABEL, MakeLabelText(GetResString(IDS_ID)));
	SetDlgItemText(IDC_NETWORK_ED2K_SERVER_LABEL, MakeLabelText(GetResString(IDS_SERVER)));
	SetDlgItemText(IDC_NETWORK_ED2K_CONN_LABEL, MakeLabelText(GetResString(IDS_CONNECTIONS)));
	SetDlgItemText(IDC_NETWORK_KAD_STATUS_LABEL, MakeLabelText(GetResString(IDS_STATUS)));
	SetDlgItemText(IDC_NETWORK_KAD_UDP_LABEL, MakeLabelText(CString(_T("UDP ")) + GetResString(IDS_STATUS)));
	SetDlgItemText(IDC_NETWORK_KAD_ADDRESS_LABEL, MakeLabelText(GetResString(IDS_IP) + _T(":") + GetResString(IDS_PORT)));
	SetDlgItemText(IDC_NETWORK_KAD_BUDDY_LABEL, MakeLabelText(GetResString(IDS_BUDDY)));
	SetDlgItemText(IDC_NETWORK_KAD_COUNTS_LABEL, MakeLabelText(GetResString(IDS_UUSERS) + _T(" / ") + GetResString(IDS_PW_FILES)));

	m_info.SendMessage(EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(3, 3));
	m_info.SetAutoURLDetect();
	m_info.SetEventMask(m_info.GetEventMask() | ENM_LINK);

	PARAFORMAT pf = {};
	pf.cbSize = (UINT)sizeof pf;
	if (m_info.GetParaFormat(pf)) {
		pf.dwMask |= PFM_TABSTOPS;
		pf.cTabCount = 4;
		pf.rgxTabs[0] = 900;
		pf.rgxTabs[1] = 1000;
		pf.rgxTabs[2] = 1100;
		pf.rgxTabs[3] = 1200;
		m_info.SetParaFormat(pf);
	}

	m_cfDef.cbSize = (UINT)sizeof m_cfDef;
	if (m_info.GetSelectionCharFormat(m_cfDef)) {
		m_cfBold = m_cfDef;
		m_cfBold.dwMask |= CFM_BOLD;
		m_cfBold.dwEffects |= CFE_BOLD;
	}

	RefreshNetworkInformation();
	DisableAutoSelect(m_info);
	return TRUE;
}

void CNetworkInfoDlg::RefreshNetworkInformation()
{
	SetDlgItemText(IDC_NETWORK_CLIENT_NICK_VALUE, NormalizeSummaryValue(thePrefs.GetUserNick()));
	SetDlgItemText(IDC_NETWORK_CLIENT_HASH_VALUE, NormalizeSummaryValue(md4str(thePrefs.GetUserHash())));

	CString strPort;
	strPort.Format(_T("%u"), thePrefs.GetPort());
	SetDlgItemText(IDC_NETWORK_CLIENT_TCP_VALUE, strPort);
	strPort.Format(_T("%u"), thePrefs.GetUDPPort());
	SetDlgItemText(IDC_NETWORK_CLIENT_UDP_VALUE, strPort);
	SetDlgItemText(IDC_NETWORK_CLIENT_BIND_VALUE, NormalizeSummaryValue(FormatBindSummary()));

	UINT uEd2kStatusId = IDS_DISCONNECTED;
	if (theApp.serverconnect->IsConnected())
		uEd2kStatusId = IDS_CONNECTED;
	else if (theApp.serverconnect->IsConnecting())
		uEd2kStatusId = IDS_CONNECTING;
	SetDlgItemText(IDC_NETWORK_ED2K_STATUS_VALUE, GetResString(uEd2kStatusId));
	SetDlgItemText(IDC_NETWORK_ED2K_ADDRESS_VALUE, NormalizeSummaryValue(FormatED2KPublicAddress()));

	CString strClientId;
	if (theApp.serverconnect->IsConnected())
		strClientId.Format(_T("%u (%s)")
			, theApp.serverconnect->GetClientID()
			, (LPCTSTR)GetResString(theApp.serverconnect->IsLowID() ? IDS_IDLOW : IDS_IDHIGH));
	SetDlgItemText(IDC_NETWORK_ED2K_ID_VALUE, NormalizeSummaryValue(strClientId));
	SetDlgItemText(IDC_NETWORK_ED2K_SERVER_VALUE, NormalizeSummaryValue(FormatED2KServer()));
	SetDlgItemText(IDC_NETWORK_ED2K_CONN_VALUE, NormalizeSummaryValue(theApp.serverconnect->IsConnected()
		? GetResString(theApp.serverconnect->IsConnectedObfuscated() ? IDS_OBFUSCATED : IDS_PRIONORMAL)
		: CString()));

	SetDlgItemText(IDC_NETWORK_KAD_STATUS_VALUE, NormalizeSummaryValue(FormatKadStatus()));
	SetDlgItemText(IDC_NETWORK_KAD_UDP_VALUE, NormalizeSummaryValue(FormatKadUdpStatus()));
	SetDlgItemText(IDC_NETWORK_KAD_ADDRESS_VALUE, NormalizeSummaryValue(FormatKadAddress()));
	SetDlgItemText(IDC_NETWORK_KAD_BUDDY_VALUE, NormalizeSummaryValue(FormatKadBuddyStatus()));
	SetDlgItemText(IDC_NETWORK_KAD_COUNTS_VALUE, NormalizeSummaryValue(FormatKadCounts()));

	m_info.SetRedraw(FALSE);
	m_info.SetWindowText(_T(""));
	CreateNetworkInfo(m_info, m_cfDef, m_cfBold, true);
	m_info.SetRedraw(TRUE);
	m_info.Invalidate();
}

CString CNetworkInfoDlg::BuildClipboardReport() const
{
	CString strReport;
	CString strValue;

	strReport.AppendFormat(_T("%s\r\n"), (LPCTSTR)GetResString(IDS_NETWORK_INFO));
	strReport.AppendFormat(_T("%s\r\n"), (LPCTSTR)_T("============"));
	strReport.Append(_T("\r\n"));

	strReport.AppendFormat(_T("%s\r\n"), (LPCTSTR)GetResString(IDS_CLIENT));
	GetDlgItemText(IDC_NETWORK_CLIENT_NICK_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_PW_NICK), strValue);
	GetDlgItemText(IDC_NETWORK_CLIENT_HASH_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_CD_UHASH), strValue);
	GetDlgItemText(IDC_NETWORK_CLIENT_TCP_VALUE, strValue);
	AppendClipboardLine(strReport, CString(_T("TCP ")) + GetResString(IDS_PORT), strValue);
	GetDlgItemText(IDC_NETWORK_CLIENT_UDP_VALUE, strValue);
	AppendClipboardLine(strReport, CString(_T("UDP ")) + GetResString(IDS_PORT), strValue);
	GetDlgItemText(IDC_NETWORK_CLIENT_BIND_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_BIND_INTERFACE), strValue);
	strReport.Append(_T("\r\n"));

	strReport.AppendFormat(_T("eD2K %s\r\n"), (LPCTSTR)GetResString(IDS_NETWORK));
	GetDlgItemText(IDC_NETWORK_ED2K_STATUS_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_STATUS), strValue);
	GetDlgItemText(IDC_NETWORK_ED2K_ADDRESS_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_IP) + _T(":") + GetResString(IDS_PORT), strValue);
	GetDlgItemText(IDC_NETWORK_ED2K_ID_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_ID), strValue);
	GetDlgItemText(IDC_NETWORK_ED2K_SERVER_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_SERVER), strValue);
	GetDlgItemText(IDC_NETWORK_ED2K_CONN_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_CONNECTIONS), strValue);
	strReport.Append(_T("\r\n"));

	strReport.AppendFormat(_T("%s %s\r\n"), (LPCTSTR)GetResString(IDS_KADEMLIA), (LPCTSTR)GetResString(IDS_NETWORK));
	GetDlgItemText(IDC_NETWORK_KAD_STATUS_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_STATUS), strValue);
	GetDlgItemText(IDC_NETWORK_KAD_UDP_VALUE, strValue);
	AppendClipboardLine(strReport, CString(_T("UDP ")) + GetResString(IDS_STATUS), strValue);
	GetDlgItemText(IDC_NETWORK_KAD_ADDRESS_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_IP) + _T(":") + GetResString(IDS_PORT), strValue);
	GetDlgItemText(IDC_NETWORK_KAD_BUDDY_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_BUDDY), strValue);
	GetDlgItemText(IDC_NETWORK_KAD_COUNTS_VALUE, strValue);
	AppendClipboardLine(strReport, GetResString(IDS_UUSERS) + _T(" / ") + GetResString(IDS_PW_FILES), strValue);
	strReport.Append(_T("\r\n"));
	strReport.AppendFormat(_T("%s\r\n"), (LPCTSTR)GetResString(IDS_DETAILS));
	strReport.AppendFormat(_T("%s\r\n"), (LPCTSTR)_T("-------"));

	m_info.GetWindowText(strValue);
	strReport.Append(strValue);
	return strReport;
}

void CNetworkInfoDlg::OnBnClickedReload()
{
	RefreshNetworkInformation();
}

void CNetworkInfoDlg::OnBnClickedCopy()
{
	theApp.CopyTextToClipboard(BuildClipboardReport());
}

void CreateNetworkInfo(CRichEditCtrlX &rCtrl, CHARFORMAT &rcfDef, CHARFORMAT &rcfBold, bool bFullInfo)
{
	if (bFullInfo) {
		///////////////////////////////////////////////////////////////////////////
		// Ports Info
		///////////////////////////////////////////////////////////////////////////
		rCtrl.SetSelectionCharFormat(rcfBold);
		rCtrl << GetResString(IDS_CLIENT) << _T("\r\n");
		rCtrl.SetSelectionCharFormat(rcfDef);

		rCtrl << GetResString(IDS_PW_NICK) << _T(":\t") << thePrefs.GetUserNick() << _T("\r\n");
		rCtrl << GetResString(IDS_CD_UHASH) << _T("\t") << md4str(thePrefs.GetUserHash()) << _T("\r\n");
		rCtrl << _T("TCP ") << GetResString(IDS_PORT) << _T(":\t") << thePrefs.GetPort() << _T("\r\n");
		rCtrl << _T("UDP ") << GetResString(IDS_PORT) << _T(":\t") << thePrefs.GetUDPPort() << _T("\r\n");
		rCtrl << _T("\r\n");
	}

	///////////////////////////////////////////////////////////////////////////
	// ED2K
	///////////////////////////////////////////////////////////////////////////
	rCtrl.SetSelectionCharFormat(rcfBold);
	rCtrl << _T("eD2K ") << GetResString(IDS_NETWORK) << _T("\r\n");
	rCtrl.SetSelectionCharFormat(rcfDef);

	rCtrl << GetResString(IDS_STATUS) << _T(":\t");
	UINT uid;
	if (theApp.serverconnect->IsConnected())
		uid = IDS_CONNECTED;
	else if (theApp.serverconnect->IsConnecting())
		uid = IDS_CONNECTING;
	else
		uid = IDS_DISCONNECTED;
	rCtrl << GetResString(uid) << _T("\r\n");

	//I only show this in full display as the normal display is not
	//updated at regular intervals.
	if (bFullInfo && theApp.serverconnect->IsConnected()) {
		uint32 uTotalUser = 0;
		uint32 uTotalFile = 0;

		theApp.serverlist->GetUserFileStatus(uTotalUser, uTotalFile);
		rCtrl << GetResString(IDS_UUSERS) << _T(":\t") << GetFormatedUInt(uTotalUser) << _T("\r\n");
		rCtrl << GetResString(IDS_PW_FILES) << _T(":\t") << GetFormatedUInt(uTotalFile) << _T("\r\n");
	}

	CString buffer;
	if (theApp.serverconnect->IsConnected()) {
		rCtrl << GetResString(IDS_IP) << _T(":") << GetResString(IDS_PORT) << _T(":");
		if (theApp.serverconnect->IsLowID() && theApp.GetED2KPublicIP() == 0)
			buffer = GetResString(IDS_UNKNOWN);
		else
			buffer.Format(_T("%s:%u"), (LPCTSTR)StatusBarInfo::Detail::FormatStoredIPv4Address(theApp.GetED2KPublicIP()), thePrefs.GetPort());
		rCtrl << _T("\t") << buffer << _T("\r\n");

		rCtrl << GetResString(IDS_ID) << _T(":\t");
		if (theApp.serverconnect->IsConnected()) {
			buffer.Format(_T("%u"), theApp.serverconnect->GetClientID());
			rCtrl << buffer;
		}
		rCtrl << _T("\r\n");

		rCtrl << _T("\t");
		rCtrl << GetResString(theApp.serverconnect->IsLowID() ? IDS_IDLOW : IDS_IDHIGH);
		rCtrl << _T("\r\n");

		CServer *cur_server = theApp.serverconnect->GetCurrentServer();
		CServer *srv = cur_server ? theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort()) : NULL;
		if (srv) {
			rCtrl << _T("\r\n");
			rCtrl.SetSelectionCharFormat(rcfBold);
			rCtrl << _T("eD2K ") << GetResString(IDS_SERVER) << _T("\r\n");
			rCtrl.SetSelectionCharFormat(rcfDef);

			rCtrl << GetResString(IDS_SW_NAME) << _T(":\t") << srv->GetListName() << _T("\r\n");
			rCtrl << GetResString(IDS_DESCRIPTION) << _T(":\t") << srv->GetDescription() << _T("\r\n");
			rCtrl << GetResString(IDS_IP) << _T(":") << GetResString(IDS_PORT) << _T(":\t") << srv->GetAddress() << _T(":") << srv->GetPort() << _T("\r\n");
			rCtrl << GetResString(IDS_VERSION) << _T(":\t") << srv->GetVersion() << _T("\r\n");
			rCtrl << GetResString(IDS_UUSERS) << _T(":\t") << GetFormatedUInt(srv->GetUsers()) << _T("\r\n");
			rCtrl << GetResString(IDS_PW_FILES) << _T(":\t") << GetFormatedUInt(srv->GetFiles()) << _T("\r\n");
			rCtrl << GetResString(IDS_CONNECTIONS) << _T(":\t");
			rCtrl << GetResString(theApp.serverconnect->IsConnectedObfuscated() ? IDS_OBFUSCATED : IDS_PRIONORMAL);
			rCtrl << _T("\r\n");


			if (bFullInfo) {
				rCtrl << GetResString(IDS_IDLOW) << _T(":\t") << GetFormatedUInt(srv->GetLowIDUsers()) << _T("\r\n");
				rCtrl << GetResString(IDS_PING) << _T(":\t") << (UINT)srv->GetPing() << _T(" ms\r\n");

				rCtrl << _T("\r\n");
				rCtrl.SetSelectionCharFormat(rcfBold);
				rCtrl << _T("eD2K ") << GetResString(IDS_SERVER) << _T(" ") << GetResString(IDS_FEATURES) << _T("\r\n");
				rCtrl.SetSelectionCharFormat(rcfDef);

				rCtrl << GetResString(IDS_SERVER_LIMITS) << _T(": ") << GetFormatedUInt(srv->GetSoftFiles()) << _T("/") << GetFormatedUInt(srv->GetHardFiles()) << _T("\r\n");

				if (thePrefs.IsExtControlsEnabled()) {
					CString sNo, sYes;
					sNo.Format(_T(": %s\r\n"), (LPCTSTR)GetResString(IDS_NO));
					sYes.Format(_T(": %s\r\n"), (LPCTSTR)GetResString(IDS_YES));
					bool bYes = srv->GetTCPFlags() & SRV_TCPFLG_COMPRESSION;
					rCtrl << GetResString(IDS_SRV_TCPCOMPR) << (bYes ? sYes : sNo);

					bYes = (srv->GetTCPFlags() & SRV_TCPFLG_NEWTAGS) || (srv->GetUDPFlags() & SRV_UDPFLG_NEWTAGS);
					rCtrl << GetResString(IDS_SHORTTAGS) << (bYes ? sYes : sNo);

					bYes = (srv->GetTCPFlags() & SRV_TCPFLG_UNICODE) || (srv->GetUDPFlags() & SRV_UDPFLG_UNICODE);
					rCtrl << _T("Unicode") << (bYes ? sYes : sNo);

					bYes = srv->GetTCPFlags() & SRV_TCPFLG_TYPETAGINTEGER;
					rCtrl << GetResString(IDS_SERVERFEATURE_INTTYPETAGS) << (bYes ? sYes : sNo);

					bYes = srv->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES;
					rCtrl << GetResString(IDS_SRV_UDPSR) << (bYes ? sYes : sNo);

					bYes = srv->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2;
					rCtrl << GetResString(IDS_SRV_UDPSR) << _T(" #2") << (bYes ? sYes : sNo);

					bYes = srv->GetUDPFlags() & SRV_UDPFLG_EXT_GETFILES;
					rCtrl << GetResString(IDS_SRV_UDPFR) << (bYes ? sYes : sNo);

					bYes = srv->SupportsLargeFilesTCP() || srv->SupportsLargeFilesUDP();
					rCtrl << GetResString(IDS_SRV_LARGEFILES) << (bYes ? sYes : sNo);

					bYes = srv->SupportsObfuscationUDP();
					rCtrl << GetResString(IDS_PROTOCOLOBFUSCATION) << _T(" (UDP)") << (bYes ? sYes : sNo);

					bYes = srv->SupportsObfuscationTCP();
					rCtrl << GetResString(IDS_PROTOCOLOBFUSCATION) << _T(" (TCP)") << (bYes ? sYes : sNo);
				}
			}
		}
	}
	rCtrl << _T("\r\n");

	///////////////////////////////////////////////////////////////////////////
	// Kademlia
	///////////////////////////////////////////////////////////////////////////
	rCtrl.SetSelectionCharFormat(rcfBold);
	rCtrl << GetResString(IDS_KADEMLIA) << _T(" ") << GetResString(IDS_NETWORK) << _T("\r\n");
	rCtrl.SetSelectionCharFormat(rcfDef);

	rCtrl << GetResString(IDS_STATUS) << _T(":\t");
	if (Kademlia::CKademlia::IsConnected()) {
		rCtrl << GetResString(Kademlia::CKademlia::IsFirewalled() ? IDS_FIREWALLED : IDS_KADOPEN);
		if (Kademlia::CKademlia::IsRunningInLANMode())
			rCtrl << _T(" (") << GetResString(IDS_LANMODE) << _T(")");
		rCtrl << _T("\r\n");
		rCtrl << _T("UDP ") << GetResString(IDS_STATUS) << _T(":\t");
		if (Kademlia::CUDPFirewallTester::IsFirewalledUDP(true))
			rCtrl << GetResString(IDS_FIREWALLED);
		else {
			rCtrl << GetResString(IDS_KADOPEN);
			if (!Kademlia::CUDPFirewallTester::IsVerified())
				rCtrl << _T(" (") << GetResString(IDS_UNVERIFIED).MakeLower() << _T(")");
		}
		rCtrl << _T("\r\n");

		buffer.Format(_T("%s:%i"), (LPCTSTR)ipstr(htonl(Kademlia::CKademlia::GetPrefs()->GetIPAddress())), thePrefs.GetUDPPort());
		rCtrl << GetResString(IDS_IP) << _T(":") << GetResString(IDS_PORT) << _T(":\t") << buffer << _T("\r\n");

		buffer.Format(_T("%u"), Kademlia::CKademlia::GetPrefs()->GetIPAddress());
		rCtrl << GetResString(IDS_ID) << _T(":\t") << buffer << _T("\r\n");
		if (Kademlia::CKademlia::GetPrefs()->GetUseExternKadPort() && Kademlia::CKademlia::GetPrefs()->GetExternalKadPort() != 0
			&& Kademlia::CKademlia::GetPrefs()->GetInternKadPort() != Kademlia::CKademlia::GetPrefs()->GetExternalKadPort())
		{
			buffer.Format(_T("%u"), Kademlia::CKademlia::GetPrefs()->GetExternalKadPort());
			rCtrl << GetResString(IDS_EXTERNUDPPORT) << _T(":\t") << buffer << _T("\r\n");
		}

		if (Kademlia::CUDPFirewallTester::IsFirewalledUDP(true)) {
			rCtrl << GetResString(IDS_BUDDY) << _T(":\t");
			switch (theApp.clientlist->GetBuddyStatus()) {
			case Disconnected:
				uid = IDS_BUDDYNONE;
				break;
			case Connecting:
				uid = IDS_CONNECTING;
				break;
			case Connected:
				uid = IDS_CONNECTED;
				break;
			default:
				uid = 0;
			}
			if (uid)
				rCtrl << GetResString(uid);
			rCtrl << _T("\r\n");
		}

		if (bFullInfo) {
			CString sKadID;
			Kademlia::CKademlia::GetPrefs()->GetKadID(sKadID);
			rCtrl << GetResString(IDS_CD_UHASH) << _T("\t") << sKadID << _T("\r\n");

			rCtrl << GetResString(IDS_UUSERS) << _T(":\t") << GetFormatedUInt(Kademlia::CKademlia::GetKademliaUsers()) << _T(" (Experimental: ") << GetFormatedUInt(Kademlia::CKademlia::GetKademliaUsers(true)) << _T(")\r\n");
			//rCtrl << GetResString(IDS_UUSERS) << _T(":\t") << GetFormatedUInt(Kademlia::CKademlia::GetKademliaUsers()) << _T("\r\n");
			rCtrl << GetResString(IDS_PW_FILES) << _T(":\t") << GetFormatedUInt(Kademlia::CKademlia::GetKademliaFiles()) << _T("\r\n");
			rCtrl << GetResString(IDS_INDEXED) << _T(":\r\n");
			buffer.Format(GetResString(IDS_KADINFO_SRC), Kademlia::CKademlia::GetIndexed()->m_uTotalIndexSource);
			rCtrl << buffer;
			buffer.Format(GetResString(IDS_KADINFO_KEYW), Kademlia::CKademlia::GetIndexed()->m_uTotalIndexKeyword);
			rCtrl << buffer;
			buffer.Format(_T("\t%s: %u\r\n"), (LPCTSTR)GetResString(IDS_NOTES), Kademlia::CKademlia::GetIndexed()->m_uTotalIndexNotes);
			rCtrl << buffer;
			buffer.Format(_T("\t%s: %u\r\n"), (LPCTSTR)GetResString(IDS_THELOAD), Kademlia::CKademlia::GetIndexed()->m_uTotalIndexLoad);
			rCtrl << buffer;
		}
	} else
		rCtrl << GetResString(Kademlia::CKademlia::IsRunning() ? IDS_CONNECTING : IDS_DISCONNECTED) << _T("\r\n");

	rCtrl << _T("\r\n");
}
