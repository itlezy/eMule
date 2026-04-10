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
#include "UploadListCtrl.h"
#include "TransferWnd.h"
#include "TransferDlg.h"
#include "UpDownClient.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "emuledlg.h"
#include "friendlist.h"
#include "MemDC.h"
#include "KnownFile.h"
#include "SharedFileList.h"
#include "ChatWnd.h"
#include "kademlia/kademlia/Kademlia.h"
#include "UploadQueue.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	CString FormatUploadRatio(float fRatio)
	{
		CString str;
		str.Format(_T("%.1f"), fRatio);
		return str;
	}

	CString FormatCooldown(ULONGLONG ullRemainingMs)
	{
		if (ullRemainingMs == 0)
			return _T("-");
		CString str;
		str.Format(_T("%us"), static_cast<UINT>((ullRemainingMs + 999) / 1000));
		return str;
	}

	int CompareRatio(float fLeft, float fRight)
	{
		if (fLeft < fRight)
			return -1;
		if (fLeft > fRight)
			return 1;
		return 0;
	}
}


IMPLEMENT_DYNAMIC(CUploadListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CUploadListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, OnLvnGetInfoTip)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CUploadListCtrl::CUploadListCtrl()
	: CListCtrlItemWalk(this)
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("UploadsLv"));
}

void CUploadListCtrl::Init()
{
	SetPrefsKey(_T("UploadListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip) {
		m_tooltip.SubclassWindow(tooltip->m_hWnd);
		tooltip->ModifyStyle(0, TTS_NOPREFIX);
		tooltip->SetDelayTime(TTDT_AUTOPOP, SEC2MS(20));
		tooltip->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	}

	InsertColumn(0,	_T(""),	LVCFMT_LEFT, DFLT_CLIENTNAME_COL_WIDTH);	//IDS_QL_USERNAME
	InsertColumn(1,	_T(""),	LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);		//IDS_FILE
	InsertColumn(2,	_T(""),	LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);		//IDS_DL_SPEED
	InsertColumn(3,	_T(""),	LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);		//IDS_DL_TRANSF
	InsertColumn(4,	_T(""),	LVCFMT_LEFT, 60);							//IDS_WAITED
	InsertColumn(5,	_T(""),	LVCFMT_LEFT, 80);							//IDS_UPLOADTIME
	InsertColumn(6,	_T(""),	LVCFMT_LEFT, 100);							//IDS_STATUS
	InsertColumn(7,	_T(""),	LVCFMT_RIGHT,85);							//IDS_ALL_TIME_RATIO
	InsertColumn(8,	_T(""),	LVCFMT_RIGHT,85);							//IDS_SESSION_RATIO
	InsertColumn(9,	_T(""),	LVCFMT_RIGHT,70);							//IDS_COOLDOWN
	InsertColumn(10,_T(""),	LVCFMT_LEFT, DFLT_PARTSTATUS_COL_WIDTH);	//IDS_UPSTATUS

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, MAKELONG(GetSortItem(), !GetSortAscending()));
}

void CUploadListCtrl::Localize()
{
	static const UINT uids[11] =
	{
		IDS_QL_USERNAME, IDS_FILE, IDS_DL_SPEED, IDS_DL_TRANSF, IDS_WAITED
		, IDS_UPLOADTIME, IDS_STATUS, IDS_ALL_TIME_RATIO, IDS_SESSION_RATIO, IDS_COOLDOWN, IDS_UPSTATUS
	};

	LocaliseHeaderCtrl(uids, _countof(uids));
}

void CUploadListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CUploadListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	m_pImageList = &theApp.emuledlg->GetClientIconList();
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CUploadListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (!lpDrawItemStruct->itemData || theApp.IsClosing())
		return;

	CRect rcItem(lpDrawItemStruct->rcItem);
	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	RECT rcClient;
	GetClientRect(&rcClient);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(lpDrawItemStruct->itemData);
	if (client->GetSlotNumber() > (UINT)theApp.uploadqueue->GetActiveUploadsCount())
		dc.SetTextColor(::GetSysColor(COLOR_GRAYTEXT));

	const CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	LONG itemLeft = rcItem.left;
	LONG iIconY = max((rcItem.Height() - 15) / 2, 0);
	for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
		int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
		if (IsColumnHidden(iColumn))
			continue;

		UINT uDrawTextAlignment;
		int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
		rcItem.left = itemLeft;
		rcItem.right = itemLeft + iColumnWidth;
		if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem)) {
			const CString &sItem(GetItemDisplayText(client, iColumn));
			switch (iColumn) {
			case 0: //user name
				{
					int iImage;
					UINT uOverlayImage;
					client->GetDisplayImage(iImage, uOverlayImage);

					rcItem.left += sm_iIconOffset;
					const POINT point{rcItem.left, rcItem.top + iIconY};
					m_pImageList->Draw(dc, iImage, point, ILD_NORMAL | INDEXTOOVERLAYMASK(uOverlayImage));
					rcItem.left += 16 + sm_iLabelOffset - sm_iSubItemInset;
				}
			default: //any text column
				rcItem.left += sm_iSubItemInset;
				rcItem.right -= sm_iSubItemInset;
				dc.DrawText(sItem, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
				break;
			case 10: //upload status bar
				++rcItem.top;
				--rcItem.bottom;
				client->DrawUpStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
				++rcItem.bottom;
				--rcItem.top;
			}
		}
		itemLeft += iColumnWidth;
	}

	DrawFocusRect(dc, &lpDrawItemStruct->rcItem, lpDrawItemStruct->itemState & ODS_FOCUS, bCtrlFocused, lpDrawItemStruct->itemState & ODS_SELECTED);
}

CString  CUploadListCtrl::GetItemDisplayText(const CUpDownClient *client, int iSubItem) const
{
	CString sText;
	switch (iSubItem) {
	case 0:
		if (client->GetUserName() != NULL)
			sText = client->GetUserName();
		else
			sText.Format(_T("(%s)"), (LPCTSTR)GetResString(IDS_UNKNOWN));
		break;
	case 1:
		{
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file)
				sText = file->GetFileName();
		}
		break;
	case 2:
		sText = CastItoXBytes(client->GetUploadDatarate(), false, true);
		break;
	case 3:
		// NOTE: If you change (add/remove) anything which is displayed here, update also the sorting part.
		if (!thePrefs.m_bExtControls)
			return CastItoXBytes(client->GetSessionUp());
		sText.Format(_T("%s (%s)"), (LPCTSTR)CastItoXBytes(client->GetSessionUp()), (LPCTSTR)CastItoXBytes(client->GetQueueSessionPayloadUp()));
		break;
	case 4:
		if (!client->HasLowID())
			sText = CastSecondsToHM(client->GetWaitTime() / SEC2MS(1));
		else
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastSecondsToHM(client->GetWaitTime() / SEC2MS(1)), (LPCTSTR)GetResString(IDS_IDLOW));
		break;
	case 5:
		sText = CastSecondsToHM(client->GetUpStartTimeDelay() / SEC2MS(1));
		break;
	case 6:
		sText = client->GetUploadStateDisplayString();
		break;
	case 7:
		{
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			sText = file ? FormatUploadRatio(file->GetAllTimeUploadRatio()) : _T("-");
		}
		break;
	case 8:
		{
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			sText = file ? FormatUploadRatio(file->GetSessionUploadRatio()) : _T("-");
		}
		break;
	case 9:
		sText = theApp.uploadqueue->HasCollectionUploadSlot(client) || client->GetFriendSlot() ? _T("-") : FormatCooldown(client->GetSlowUploadCooldownRemaining());
		break;
	case 10:
		sText = GetResString(IDS_UPSTATUS);
	}
	return sText;
}

void CUploadListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
{
	if (!theApp.IsClosing()) {
		// Although we have an owner drawn listview control we store the text for the primary item in the
		// listview, to be capable of quick searching those items via the keyboard. Because our listview
		// items may change their contents, we do this via a text callback function. The listview control
		// will send us the LVN_DISPINFO notification if it needs to know the contents of the primary item.
		//
		// But, the listview control sends this notification all the time, even if we do not search for an item.
		// At least this notification is only sent for the visible items and not for all items in the list.
		// Though, because this function is invoked *very* often, do *NOT* put any time consuming code in here.
		//
		// Vista: That callback is used to get the strings for the label tips for the sub(!)-items.
		//
		const LVITEMW &rItem = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item;
		if (rItem.mask & LVIF_TEXT) {
			const CUpDownClient *pClient = reinterpret_cast<CUpDownClient*>(rItem.lParam);
			if (pClient != NULL)
				_tcsncpy_s(rItem.pszText, rItem.cchTextMax, GetItemDisplayText(pClient, rItem.iSubItem), _TRUNCATE);
		}
	}
	*pResult = 0;
}

void CUploadListCtrl::OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);
	LVHITTESTINFO hti;
	if (pGetInfoTip->iSubItem == 0 && ::GetCursorPos(&hti.pt)) {
		ScreenToClient(&hti.pt);
		if (SubItemHitTest(&hti) == -1 || hti.iItem != pGetInfoTip->iItem || hti.iSubItem != 0) {
			// don't show the default label tip for the main item, if the mouse is not over the main item
			if ((pGetInfoTip->dwFlags & LVGIT_UNFOLDED) == 0 && pGetInfoTip->cchTextMax > 0 && pGetInfoTip->pszText)
				pGetInfoTip->pszText[0] = _T('\0');
			return;
		}

		const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(pGetInfoTip->iItem));
		if (client && pGetInfoTip->pszText && pGetInfoTip->cchTextMax > 0) {
			CString strInfo;
			strInfo.Format(GetResString(IDS_USERINFO), client->GetUserName());
			const CKnownFile *file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file) {
				strInfo.AppendFormat(_T("%s %s\n"), (LPCTSTR)GetResString(IDS_SF_REQUESTED), (LPCTSTR)file->GetFileName());
				strInfo.AppendFormat(GetResString(IDS_FILESTATS_SESSION) + GetResString(IDS_FILESTATS_TOTAL)
					, file->statistic.GetAccepts(), file->statistic.GetRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetTransferred())
					, file->statistic.GetAllTimeAccepts(), file->statistic.GetAllTimeRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetAllTimeTransferred()));
			} else
				strInfo += GetResString(IDS_REQ_UNKNOWNFILE);

			strInfo += TOOLTIP_AUTOFORMAT_SUFFIX_CH;
			_tcsncpy(pGetInfoTip->pszText, strInfo, pGetInfoTip->cchTextMax);
			pGetInfoTip->pszText[pGetInfoTip->cchTextMax - 1] = _T('\0');
		}
	}
	*pResult = 0;
}

void CUploadListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	const LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() != pNMLV->iSubItem) {
		switch (pNMLV->iSubItem) {
		case 2: // Data rate
		case 3: // Session Up
		case 4: // Wait Time
		case 7: // All-time ratio
		case 8: // Session ratio
		case 9: // Cooldown
		case 10: // Part Count
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}
	} else
		sortAscending = !GetSortAscending();

	// Sort table
	UpdateSortHistory(MAKELONG(pNMLV->iSubItem, !sortAscending));
	SetSortArrow(pNMLV->iSubItem, sortAscending);
	SortItems(SortProc, MAKELONG(pNMLV->iSubItem, !sortAscending));
	*pResult = 0;
}

int CALLBACK CUploadListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CUpDownClient *item1 = reinterpret_cast<CUpDownClient*>(lParam1);
	const CUpDownClient *item2 = reinterpret_cast<CUpDownClient*>(lParam2);

	int iResult = 0;
	switch (LOWORD(lParamSort)) {
	case 0:
		if (item1->GetUserName() && item2->GetUserName())
			iResult = CompareLocaleStringNoCase(item1->GetUserName(), item2->GetUserName());
		else if (item1->GetUserName() == NULL || item2->GetUserName() == NULL)
			iResult = 1; // place clients with no user names at the bottom
		break;
	case 1:
		{
			const CKnownFile *file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
			const CKnownFile *file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
			if (file1 != NULL && file2 != NULL)
				iResult = CompareLocaleStringNoCase(file1->GetFileName(), file2->GetFileName());
			else
				iResult = (file1 == NULL) ? 1 : -1;
		}
		break;
	case 2:
		iResult = CompareUnsigned(item1->GetUploadDatarate(), item2->GetUploadDatarate());
		break;
	case 3:
		iResult = CompareUnsigned(item1->GetSessionUp(), item2->GetSessionUp());
		if (iResult == 0 && thePrefs.m_bExtControls)
			iResult = CompareUnsigned(item1->GetQueueSessionPayloadUp(), item2->GetQueueSessionPayloadUp());
		break;
	case 4:
		iResult = CompareUnsigned(item1->GetWaitTime(), item2->GetWaitTime());
		break;
	case 5:
		iResult = CompareUnsigned(item1->GetUpStartTimeDelay(), item2->GetUpStartTimeDelay());
		break;
	case 6:
		iResult = item1->GetUploadState() - item2->GetUploadState();
		break;
	case 7:
		{
			const CKnownFile *file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
			const CKnownFile *file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
			if (file1 != NULL && file2 != NULL)
				iResult = CompareRatio(file1->GetAllTimeUploadRatio(), file2->GetAllTimeUploadRatio());
			else
				iResult = (file1 == NULL) ? 1 : -1;
		}
		break;
	case 8:
		{
			const CKnownFile *file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
			const CKnownFile *file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
			if (file1 != NULL && file2 != NULL)
				iResult = CompareRatio(file1->GetSessionUploadRatio(), file2->GetSessionUploadRatio());
			else
				iResult = (file1 == NULL) ? 1 : -1;
		}
		break;
	case 9:
		iResult = CompareUnsigned(item1->GetSlowUploadCooldownRemaining(), item2->GetSlowUploadCooldownRemaining());
		break;
	case 10:
		iResult = CompareUnsigned(item1->GetUpPartCount(), item2->GetUpPartCount());
	}

	if (HIWORD(lParamSort))
		iResult = -iResult;

	//call secondary sort order, if the first one resulted as equal
	if (iResult == 0) {
		LPARAM iNextSort = theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.GetNextSortOrder(lParamSort);
		if (iNextSort != -1)
			iResult = SortProc(lParam1, lParam2, iNextSort);
	}

	return iResult;
}

void CUploadListCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(iSel));
		if (client) {
			CClientDetailDialog dialog(client, this);
			dialog.DoModal();
		}
	}
	*pResult = 0;
}

void CUploadListCtrl::OnContextMenu(CWnd*, CPoint point)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	const CUpDownClient *client = (iSel >= 0) ? reinterpret_cast<CUpDownClient*>(GetItemData(iSel)) : NULL;
	const bool is_ed2k = client && client->IsEd2kClient();

	CTitledMenu ClientMenu;
	ClientMenu.CreatePopupMenu();
	ClientMenu.AddMenuTitle(GetResString(IDS_CLIENTS), true);
	ClientMenu.AppendMenu(MF_STRING | (client ? MF_ENABLED : MF_GRAYED), MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("CLIENTDETAILS"));
	ClientMenu.SetDefaultItem(MP_DETAIL);
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && !client->IsFriend()) ? MF_ENABLED : MF_GRAYED), MP_ADDFRIEND, GetResString(IDS_ADDFRIEND), _T("ADDFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | (is_ed2k ? MF_ENABLED : MF_GRAYED), MP_MESSAGE, GetResString(IDS_SEND_MSG), _T("SENDMESSAGE"));
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetViewSharedFilesSupport()) ? MF_ENABLED : MF_GRAYED), MP_SHOWLIST, GetResString(IDS_VIEWFILES), _T("VIEWFILES"));
	if (Kademlia::CKademlia::IsRunning() && !Kademlia::CKademlia::IsConnected())
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a) ? MF_ENABLED : MF_GRAYED), MP_BOOT, GetResString(IDS_BOOTSTRAP));
	ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));
	GetPopupMenuPos(*this, point);
	ClientMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

BOOL CUploadListCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	if (wParam == MP_FIND) {
		OnFindStart();
		return TRUE;
	}

	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(iSel));
		switch (wParam) {
		case MP_SHOWLIST:
			client->RequestSharedFileList();
			break;
		case MP_MESSAGE:
			theApp.emuledlg->chatwnd->StartSession(client);
			break;
		case MP_ADDFRIEND:
			if (theApp.friendlist->AddFriend(client))
				Update(iSel);
			break;
		case MP_DETAIL:
		case MPG_ALTENTER:
		case IDA_ENTER:
			{
				CClientDetailDialog dialog(client, this);
				dialog.DoModal();
			}
			break;
		case MP_BOOT:
			if (client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a)
				Kademlia::CKademlia::Bootstrap(ntohl(client->GetIP()), client->GetKadPort());
		}
	}
	return TRUE;
}

void CUploadListCtrl::AddClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	int iItemCount = GetItemCount();
	int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, iItemCount, LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)client);
	Update(iItem);
	theApp.emuledlg->transferwnd->m_pwndTransfer->UpdateListCount(CTransferWnd::wnd2Uploading, iItemCount + 1);
}

void CUploadListCtrl::RemoveClient(const CUpDownClient *client)
{
	if (theApp.IsClosing())
		return;

	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)client;
	int iItem = FindItem(&find);
	if (iItem >= 0) {
		DeleteItem(iItem);
		theApp.emuledlg->transferwnd->m_pwndTransfer->UpdateListCount(CTransferWnd::wnd2Uploading);
	}
}

void CUploadListCtrl::RefreshClient(const CUpDownClient *client)
{
	if (theApp.emuledlg->activewnd == theApp.emuledlg->transferwnd
		&& theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.IsWindowVisible()
		&& !theApp.IsClosing())
	{
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)client;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			Update(iItem);
	}
}

void CUploadListCtrl::ShowSelectedUserDetails()
{
	CPoint point;
	if (!::GetCursorPos(&point))
		return;
	ScreenToClient(&point);
	int it = HitTest(point);
	if (it < 0)
		return;

	SetItemState(-1, 0, LVIS_SELECTED);
	SetItemState(it, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	SetSelectionMark(it);   // display selection mark correctly!

	CUpDownClient *client = reinterpret_cast<CUpDownClient*>(GetItemData(GetSelectionMark()));
	if (client) {
		CClientDetailDialog dialog(client, this);
		dialog.DoModal();
	}
}
