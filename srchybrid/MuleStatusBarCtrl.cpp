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
#include "MuleStatusBarCtrl.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include "ServerWnd.h"
#include "StatisticsDlg.h"
#include "ChatWnd.h"
#include "ServerConnect.h"
#include "Server.h"
#include "ServerList.h"
#include "Preferences.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"
#include "kademlia/kademlia/Kademlia.h"
#include "StatusBarInfo.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CMuleStatusBarCtrl

IMPLEMENT_DYNAMIC(CMuleStatusBarCtrl, CStatusBarCtrl)

BEGIN_MESSAGE_MAP(CMuleStatusBarCtrl, CStatusBarCtrl)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

void CMuleStatusBarCtrl::Init()
{
	EnableToolTips();
}

void CMuleStatusBarCtrl::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int iPane = GetPaneAtPosition(point);
	switch (iPane) {
	case SBarLog:
		{
			CString sBar;
			sBar.Format(_T("eMule %s\n\n%s"), (LPCTSTR)GetResString(IDS_SV_LOG), (LPCTSTR)GetText(SBarLog));
			AfxMessageBox(sBar);
		}
		break;
	case SBarUsers:
	case SBarConnected:
	case SBarIP:
		theApp.emuledlg->serverwnd->ShowNetworkInfo();
		break;
	case SBarUpDown:
		theApp.emuledlg->SetActiveDialog(theApp.emuledlg->statisticswnd);
		break;
	case SBarChatMsg:
		theApp.emuledlg->SetActiveDialog(theApp.emuledlg->chatwnd);
		break;
	}
}

int CMuleStatusBarCtrl::GetPaneAtPosition(CPoint &point) const
{
	CRect rect;
	for (int i = GetParts(0, NULL); --i >= 0;) {
		GetRect(i, rect);
		if (rect.PtInRect(point))
			return i;
	}
	return -1;
}

CString CMuleStatusBarCtrl::GetPaneToolTipText(EStatusBarPane iPane) const
{
	if (iPane == SBarIP) {
		CString strBindAddress;
		if (thePrefs.GetBindAddr() != NULL)
			strBindAddress = thePrefs.GetBindAddr();
		return StatusBarInfo::FormatNetworkAddressPaneToolTip(strBindAddress
			, theApp.GetED2KPublicIP()
			, theApp.IsStartupBindBlocked()
			, theApp.GetStartupBindBlockReason());
	}
	if (iPane == SBarUsers && theApp.serverconnect && theApp.serverlist) {
		uint32 totaluser = 0;
		uint32 totalfile = 0;
		theApp.serverlist->GetUserFileStatus(totaluser, totalfile);
		const bool bHasEd2k = theApp.serverconnect->IsConnected();
		const bool bHasKad = Kademlia::CKademlia::IsRunning() && Kademlia::CKademlia::IsConnected();
		return StatusBarInfo::FormatUsersPaneToolTip(GetResString(IDS_UUSERS)
			, GetResString(IDS_FILES)
			, CastItoIShort(totaluser, false, 1)
			, CastItoIShort(Kademlia::CKademlia::GetKademliaUsers(), false, 1)
			, CastItoIShort(totalfile, false, 1)
			, CastItoIShort(Kademlia::CKademlia::GetKademliaFiles(), false, 1)
			, bHasEd2k
			, bHasKad);
	}
	if (iPane == SBarUpDown && theApp.downloadqueue && theApp.uploadqueue && theApp.emuledlg) {
		return StatusBarInfo::FormatTransferPaneToolTip(theApp.emuledlg->GetTransferRateString()
			, GetResString(IDS_DOWNLOADING)
			, GetResString(IDS_UPLOADING)
			, GetResString(IDS_ONQUEUE)
			, theApp.downloadqueue->GetDownloadingFileCount()
			, static_cast<uint32>(theApp.downloadqueue->GetFileCount())
			, static_cast<uint32>(theApp.uploadqueue->GetActiveUploadsCount())
			, static_cast<uint32>(theApp.uploadqueue->GetUploadQueueLength())
			, static_cast<uint32>(theApp.uploadqueue->GetWaitingUserCount()));
	}
	if (iPane == SBarConnected && theApp.emuledlg && theApp.serverconnect) {
		const CString strConnectionSummary = theApp.emuledlg->GetConnectionStateString();
		const int iKadMarker = strConnectionSummary.Find(_T("|Kad:"));
		CString strEd2kState;
		CString strKadState;
		if (iKadMarker > 5) {
			strEd2kState = strConnectionSummary.Mid(5, iKadMarker - 5);
			strKadState = strConnectionSummary.Mid(iKadMarker + 5);
		} else {
			strEd2kState = strConnectionSummary;
		}

		CString strServerName;
		CString strServerUsers;
		CString strServerPing;
		if (theApp.serverconnect->IsConnected() && theApp.serverlist) {
			const CServer *cur_server = theApp.serverconnect->GetCurrentServer();
			if (cur_server) {
				const CServer *srv = theApp.serverlist->GetServerByAddress(cur_server->GetAddress(), cur_server->GetPort());
				if (srv) {
					strServerName = srv->GetListName();
					strServerUsers = GetFormatedUInt(srv->GetUsers());
					if (srv->GetPing() > 0)
						strServerPing.Format(_T("%u ms"), srv->GetPing());
				}
			}
		}
		return StatusBarInfo::FormatConnectionPaneToolTip(strEd2kState
			, strKadState
			, GetResString(IDS_SERVER)
			, GetResString(IDS_UUSERS)
			, strServerName
			, strServerUsers
			, strServerPing);
	}
	return CString();
}

INT_PTR CMuleStatusBarCtrl::OnToolHitTest(CPoint point, TOOLINFO *pTI) const
{
	INT_PTR iHit = CWnd::OnToolHitTest(point, pTI);
	if (iHit == -1 && pTI != NULL && pTI->cbSize >= sizeof(AFX_OLDTOOLINFO)) {
		int iPane = GetPaneAtPosition(point);
		if (iPane >= 0) {
			const CString &strToolTipText = GetPaneToolTipText((EStatusBarPane)iPane);
			if (!strToolTipText.IsEmpty()) {
				pTI->hwnd = m_hWnd;
				pTI->uId = iPane;
				pTI->uFlags &= ~TTF_IDISHWND;
				pTI->uFlags |= TTF_NOTBUTTON | TTF_ALWAYSTIP;
				pTI->lpszText = _tcsdup(strToolTipText); // gets freed by MFC
				GetRect(iPane, &pTI->rect);
				iHit = iPane;
			}
		}
	}
	return iHit;
}
