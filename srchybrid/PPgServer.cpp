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
#include "emuleDlg.h"
#include "ServerWnd.h"
#include "PPgServer.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "HelpIDs.h"
#include "Opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CPPgServer, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgServer, CPropertyPage)
	ON_EN_CHANGE(IDC_SERVERRETRIES, OnSettingsChange)
	ON_BN_CLICKED(IDC_AUTOSERVER, OnSettingsChange)
	ON_BN_CLICKED(IDC_UPDATESERVERCONNECT, OnSettingsChange)
	ON_BN_CLICKED(IDC_UPDATESERVERCLIENT, OnSettingsChange)
	ON_BN_CLICKED(IDC_SCORE, OnSettingsChange)
	ON_BN_CLICKED(IDC_SMARTIDCHECK, OnSettingsChange)
	ON_BN_CLICKED(IDC_SAFESERVERCONNECT, OnSettingsChange)
	ON_BN_CLICKED(IDC_AUTOCONNECTSTATICONLY, OnSettingsChange)
	ON_BN_CLICKED(IDC_MANUALSERVERHIGHPRIO, OnSettingsChange)
	ON_BN_CLICKED(IDC_EDITADR, OnBnClickedEditadr)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CPPgServer::CPPgServer()
	: CPropertyPage(CPPgServer::IDD)
{
}

void CPPgServer::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BOOL CPPgServer::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	LoadSettings();
	Localize();
	UpdateToolTips();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPgServer::UpdateToolTips()
{
	if (!m_toolTip.Init(this))
		return;

	m_toolTip.SetTool(this, IDC_AUTOSERVER,
		_T("Automatically updates the server list from connected clients and other trusted sources.\r\n\r\n")
		_T("Convenient, but it also lets the list drift over time. Disable it if you prefer a tightly curated static server list."));
	m_toolTip.SetTool(this, IDC_UPDATESERVERCONNECT,
		_T("Allows connected servers to advertise additional servers into your list.\r\n\r\n")
		_T("Convenient, but it can grow the list with low-value entries. Disable it if you prefer stricter manual curation."));
	m_toolTip.SetTool(this, IDC_UPDATESERVERCLIENT,
		_T("Allows clients to contribute server addresses to your server list.\r\n\r\n")
		_T("Most users should treat this cautiously. Disable it if you want tighter control over server-list quality."));
	m_toolTip.SetTool(this, IDC_SAFESERVERCONNECT,
		_T("Uses a more cautious server-connect strategy instead of rushing through many candidates.\r\n\r\n")
		_T("Recommended: enabled for stability. Disable it only if you deliberately want a more aggressive connect approach."));
	m_toolTip.SetTool(this, IDC_SMARTIDCHECK,
		_T("Performs the smart LowID check before deciding that your server-side reachability is poor.\r\n\r\n")
		_T("Recommended: enabled. It avoids some false LowID conclusions during transient network conditions."));
	m_toolTip.SetTool(this, IDC_SCORE,
		_T("Uses server priorities when choosing and ordering servers.\r\n\r\n")
		_T("Recommended: enabled if you maintain a preferred subset of servers and want eMule to favor them."));
	m_toolTip.SetTool(this, IDC_SERVERRETRIES,
		_T("How many failed connection attempts a server may accumulate before eMule treats it as dead.\r\n\r\n")
		_T("Lower values prune bad servers faster; higher values are more forgiving of transient outages."));
	m_toolTip.SetTool(this, IDC_AUTOCONNECTSTATICONLY,
		_T("Restricts automatic server connect attempts to servers marked as static.\r\n\r\n")
		_T("Enable it if you maintain a small trusted static list and do not want automatic rotation across the full server list."));
	m_toolTip.SetTool(this, IDC_MANUALSERVERHIGHPRIO,
		_T("Gives manually added servers a higher priority bias than ordinary discovered entries.\r\n\r\n")
		_T("Enable it if you maintain a hand-picked server set and want those additions favored."));
}

void CPPgServer::LoadSettings()
{
	SetDlgItemInt(IDC_SERVERRETRIES, thePrefs.m_uDeadServerRetries, FALSE);
	CheckDlgButton(IDC_AUTOSERVER, thePrefs.m_bAutoUpdateServerList);
	CheckDlgButton(IDC_UPDATESERVERCONNECT, thePrefs.m_bAddServersFromServer);
	CheckDlgButton(IDC_UPDATESERVERCLIENT, thePrefs.m_bAddServersFromClients);
	CheckDlgButton(IDC_SCORE, thePrefs.m_bUseServerPriorities);
	CheckDlgButton(IDC_SMARTIDCHECK, thePrefs.m_bSmartServerIdCheck);
	CheckDlgButton(IDC_SAFESERVERCONNECT, thePrefs.m_bSafeServerConnect);
	CheckDlgButton(IDC_AUTOCONNECTSTATICONLY, thePrefs.m_bAutoConnectToStaticServersOnly);
	CheckDlgButton(IDC_MANUALSERVERHIGHPRIO, thePrefs.m_bManualAddedServersHighPriority);
}

BOOL CPPgServer::OnApply()
{
	UINT uCurDeadServerRetries = thePrefs.m_uDeadServerRetries;
	thePrefs.m_uDeadServerRetries = CPreferences::NormalizeRetryCount(GetDlgItemInt(IDC_SERVERRETRIES, NULL, FALSE), 1, 1, MAX_SERVERFAILCOUNT);
	if (uCurDeadServerRetries != thePrefs.m_uDeadServerRetries) {
		theApp.emuledlg->serverwnd->serverlistctrl.Invalidate();
		theApp.emuledlg->serverwnd->serverlistctrl.UpdateWindow();
	}
	thePrefs.m_bAutoUpdateServerList = IsDlgButtonChecked(IDC_AUTOSERVER) != 0;
	thePrefs.m_bAddServersFromServer = IsDlgButtonChecked(IDC_UPDATESERVERCONNECT) != 0;
	thePrefs.m_bAddServersFromClients = IsDlgButtonChecked(IDC_UPDATESERVERCLIENT) != 0;
	thePrefs.m_bUseServerPriorities = IsDlgButtonChecked(IDC_SCORE) != 0;
	thePrefs.m_bSmartServerIdCheck = IsDlgButtonChecked(IDC_SMARTIDCHECK) != 0;
	thePrefs.SetSafeServerConnectEnabled(IsDlgButtonChecked(IDC_SAFESERVERCONNECT) != 0);
	thePrefs.m_bAutoConnectToStaticServersOnly = IsDlgButtonChecked(IDC_AUTOCONNECTSTATICONLY) != 0;
	thePrefs.m_bManualAddedServersHighPriority = IsDlgButtonChecked(IDC_MANUALSERVERHIGHPRIO) != 0;

	LoadSettings();

	SetModified();
	return CPropertyPage::OnApply();
}

void CPPgServer::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_SERVER));
		SetDlgItemText(IDC_LBL_UPDATE_SERVERS, GetResString(IDS_SV_UPDATE));
		SetDlgItemText(IDC_LBL_MISC, GetResString(IDS_PW_MISC));
		SetDlgItemText(IDC_REMOVEDEAD, GetResString(IDS_PW_RDEAD));
		SetDlgItemText(IDC_RETRIES_LBL, GetResString(IDS_PW_RETRIES));
		SetDlgItemText(IDC_UPDATESERVERCONNECT, GetResString(IDS_PW_USC));
		SetDlgItemText(IDC_UPDATESERVERCLIENT, GetResString(IDS_PW_UCC));
		SetDlgItemText(IDC_AUTOSERVER, GetResString(IDS_PW_USS));
		SetDlgItemText(IDC_SMARTIDCHECK, GetResString(IDS_SMARTLOWIDCHECK));
		SetDlgItemText(IDC_SAFESERVERCONNECT, GetResString(IDS_PW_FASTSRVCON));
		SetDlgItemText(IDC_SCORE, GetResString(IDS_PW_SCORE));
		SetDlgItemText(IDC_MANUALSERVERHIGHPRIO, GetResString(IDS_MANUALSERVERHIGHPRIO));
		SetDlgItemText(IDC_EDITADR, GetResString(IDS_EDITLIST));
		SetDlgItemText(IDC_AUTOCONNECTSTATICONLY, GetResString(IDS_PW_AUTOCONNECTSTATICONLY));
	}
}

void CPPgServer::OnBnClickedEditadr()
{
	CString sDat;
	sDat.Format(_T("\"%saddresses.dat\""), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_CONFIGDIR));
	ShellOpen(thePrefs.GetTxtEditor(), sDat);
}

void CPPgServer::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Server);
}

BOOL CPPgServer::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgServer::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

BOOL CPPgServer::PreTranslateMessage(MSG *pMsg)
{
	m_toolTip.RelayEvent(pMsg);
	return CPropertyPage::PreTranslateMessage(pMsg);
}
