//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "otherfunctions.h"
#include "MenuCmds.h"
#include "ClientDetailDialog.h"
#include "KademliaWnd.h"
#include "emuledlg.h"
#include "friendlist.h"
#include "MemDC.h"
#include "KnownFile.h"
#include "SharedFileList.h"
#include "UpDownClient.h"
#include "ClientCredits.h"
#include "ChatWnd.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "UploadQueue.h"
#include "ToolTipCtrlX.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


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
	m_tooltip = new CToolTipCtrlX;
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("UploadsLv"));
}

CUploadListCtrl::~CUploadListCtrl()
{
	delete m_tooltip;
}

void CUploadListCtrl::Init()
{
	SetPrefsKey(_T("UploadListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip) {
		m_tooltip->SubclassWindow(tooltip->m_hWnd);
		tooltip->ModifyStyle(0, TTS_NOPREFIX);
		tooltip->SetDelayTime(TTDT_AUTOPOP, 20000);
		tooltip->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	}

	InsertColumn(0, GetResString(IDS_QL_USERNAME),	LVCFMT_LEFT, DFLT_CLIENTNAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_FILE),			LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_DL_SPEED),		LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_DL_TRANSF),	LVCFMT_RIGHT,DFLT_DATARATE_COL_WIDTH);
	InsertColumn(4, GetResString(IDS_WAITED),		LVCFMT_LEFT, 60);
	InsertColumn(5, GetResString(IDS_UPLOADTIME),	LVCFMT_LEFT, 80);
	InsertColumn(6, GetResString(IDS_STATUS),		LVCFMT_LEFT, 100);
	InsertColumn(7, GetResString(IDS_UPSTATUS),		LVCFMT_LEFT, DFLT_PARTSTATUS_COL_WIDTH);

	//MORPH START - Added by SiRoB, Client Software
	InsertColumn(8, GetResString(IDS_CD_CSOFT),		LVCFMT_LEFT, 100);
	//MORPH END - Added by SiRoB, Client Software
	InsertColumn(9, CString("Client Uploaded"),		LVCFMT_LEFT, 100); //Total up down //TODO
	// Commander - Added: IP2Country column - Start
	InsertColumn(10, CString("Country"),			LVCFMT_LEFT, 100);
	// Commander - Added: IP2Country column - End
	InsertColumn(11, GetResString(IDS_IP),			LVCFMT_LEFT, 100);
	InsertColumn(12, GetResString(IDS_IDLOW),		LVCFMT_LEFT, 50);
	InsertColumn(13, CString("Client Hash"),		LVCFMT_LEFT, 50);
	InsertColumn(14, CString("Upload %"),			LVCFMT_RIGHT, 50);
	InsertColumn(15, CString("File Size"),			LVCFMT_RIGHT, 50);
	
	InsertColumn(16, CString("Ratio"), LVCFMT_RIGHT, 50);
	InsertColumn(17, CString("Session Ratio"), LVCFMT_RIGHT, 50);

	InsertColumn(18, GetResString(IDS_UPSTATUS), LVCFMT_RIGHT, 50);

	InsertColumn(19, CString("ETA"), LVCFMT_RIGHT, 50);
	InsertColumn(20, GetResString(IDS_FOLDER), LVCFMT_LEFT, 50);

	SetAllIcons();
	Localize();
	LoadSettings();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 100));
}

void CUploadListCtrl::Localize()
{
	static const UINT uids[8] =
	{
		IDS_QL_USERNAME, IDS_FILE, IDS_DL_SPEED, IDS_DL_TRANSF, IDS_WAITED
		, IDS_UPLOADTIME, IDS_STATUS, IDS_UPSTATUS
	};

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;

	for (int i = 0; i < _countof(uids); ++i) {
		CString strRes(GetResString(uids[i]));
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(i, &hdi);
	}
}

void CUploadListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CUploadListCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_pImageList = theApp.emuledlg->transferwnd->GetClientIconList();
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(*m_pImageList) == NULL);
}

void CUploadListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	CRect rcItem(lpDrawItemStruct->rcItem);
	CRect rcClient;
	GetClientRect(&rcClient);
	const CUpDownClient *client = reinterpret_cast<CUpDownClient*>(lpDrawItemStruct->itemData);
	if (client->GetSlotNumber() > (UINT)theApp.uploadqueue->GetActiveUploadsCount())
		dc.SetTextColor(::GetSysColor(COLOR_GRAYTEXT));

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	rcItem.right = rcItem.left - sm_iLabelOffset;
	rcItem.left += sm_iIconOffset;
	for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
		int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
		if (!IsColumnHidden(iColumn)) {
			UINT uDrawTextAlignment;
			int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
			rcItem.right += iColumnWidth;
			if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem)) {
				const CString &sItem(GetItemDisplayText(client, iColumn));
				switch (iColumn) {
				case 0:
					{
						int iImage;
						UINT uOverlayImage;
						client->GetDisplayImage(iImage, uOverlayImage);

						int iIconPosY = (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 1;
						POINT point = {rcItem.left, rcItem.top + iIconPosY};
						m_pImageList->Draw(dc, iImage, point, ILD_NORMAL | INDEXTOOVERLAYMASK(uOverlayImage));

						rcItem.left += 16 + sm_iLabelOffset;
						dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
						rcItem.left -= 16;
						rcItem.right -= sm_iSubItemInset;
					}
					break;
				case 7:
					++rcItem.top;
					--rcItem.bottom;
					client->DrawUpStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
					++rcItem.bottom;
					--rcItem.top;
					break;
				default:
					dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
				}
			}
			rcItem.left += iColumnWidth;
		}
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
		sText = CastItoXBytes(client->GetDatarate(), false, true);
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
		sText = GetResString(IDS_UPSTATUS);
		break;

	case 8:
		//sText.Format(_T("%s (%s)"), client->GetClientSoftVer(), client->GetClientModVer());
		sText = client->GetClientSoftVer();
		break;
		//MORPH END - Added by SiRoB, Client Software

		//MORPH START - Added By Yun.SF3, Upload/Download
	case 9: //LSD Total UP/DL
	{
		if (client->Credits()) {
			sText.Format(_T("%s"), CastItoXBytes(client->Credits()->GetUploadedTotal(), false, false));
		}
		else
			sText.Format(_T("%s/%s"), _T("?"), _T("?"));
		break;
	}
	//MORPH END - Added By Yun.SF3, Upload/Download

	case 10:
		sText = client->GetCountryName();
		break;

	case 11:
		sText = ipstr(client->GetIP());
		break;

	case 12:
		sText.Format(_T("%s"), GetResString(client->HasLowID() ? IDS_IDLOW : IDS_IDHIGH));
		break;

	case 13:
		sText.Format(_T("%s"), client->HasValidHash() ? (LPCTSTR)md4str(client->GetUserHash()) : _T("?"));
		break;

	case 14: // upload percentage %
	{
		// TODO need to review better this calculation as the client might have already part of the file
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file) 
			sText.Format(_T("%.1f%%"), (float)client->GetSessionUp() / (float)file->GetFileSize() * 100.0);
	}
		break;

	case 15: // file size
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file)
			sText.Format(_T("%s"), (LPCTSTR)CastItoXBytes(file->GetFileSize()));
	}
		break;

	case 16: // total ratio
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file)
			sText.Format(_T("%.1f"), file->GetAllTimeRatio());
	}
		break;

	case 17: // session ratio
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file)
			sText.Format(_T("%.1f"), file->GetRatio());
	}
		break;

	case 18: // upload parts status (numeric)
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		UploadingToClient_Struct* pUpClientStruct = theApp.uploadqueue->GetUploadingClientStructByClient(client);

		if (file && pUpClientStruct)
			sText.Format(_T("%d / %d"),
				(int)pUpClientStruct->m_DoneBlocks_list.GetCount(),
				(int)file->GetFileSize() / EMBLOCKSIZE);

	}
		break;

	case 19: // ETA
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file) {
			uint64 dataLeft = (uint64)file->GetFileSize() - client->GetSessionUp();
			uint64 dataRate = client->GetDatarate();

			if (dataLeft > 1024 && dataRate > 1024 &&
				dataLeft / dataRate > 0 && dataLeft / dataRate < 60 * 60 * 8) sText = CastSecondsToHM(dataLeft / dataRate);
			else sText = "-";
		}
	}
		break;

	case 20: // File Folder
	{
		const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
		if (file)
			sText = file->GetPath();
	}
		break;


	}

	return sText;
}

void CUploadListCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
{
	if (!theApp.IsClosing()) {
		// Although we have an owner drawn listview control we store the text for the primary item in the listview, to be
		// capable of quick searching those items via the keyboard. Because our listview items may change their contents,
		// we do this via a text callback function. The listview control will send us the LVN_DISPINFO notification if
		// it needs to know the contents of the primary item.
		//
		// But, the listview control sends this notification all the time, even if we do not search for an item. At least
		// this notification is only sent for the visible items and not for all items in the list. Though, because this
		// function is invoked *very* often, do *NOT* put any time consuming code in here.
		//
		// Vista: That callback is used to get the strings for the label tips for the sub(!) items.
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
				strInfo.AppendFormat(GetResString(IDS_FILESTATS_SESSION) + GetResString(IDS_FILESTATS_TOTAL),
					file->statistic.GetAccepts(), file->statistic.GetRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetTransferred()),
					file->statistic.GetAllTimeAccepts(), file->statistic.GetAllTimeRequests(), (LPCTSTR)CastItoXBytes(file->statistic.GetAllTimeTransferred()));
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
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() != pNMListView->iSubItem) {
		switch (pNMListView->iSubItem) {
		case 2: // Data rate
		case 3: // Session Up
		case 4: // Wait Time
		case 7: // Part Count
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}
	} else
		sortAscending = !GetSortAscending();

	// Sort table
	UpdateSortHistory(pNMListView->iSubItem + (sortAscending ? 0 : 100));
	SetSortArrow(pNMListView->iSubItem, sortAscending);
	SortItems(SortProc, pNMListView->iSubItem + (sortAscending ? 0 : 100));

	*pResult = 0;
}

int CALLBACK CUploadListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CUpDownClient *item1 = reinterpret_cast<CUpDownClient*>(lParam1);
	const CUpDownClient *item2 = reinterpret_cast<CUpDownClient*>(lParam2);
	LPARAM iColumn = (lParamSort >= 100) ? lParamSort - 100 : lParamSort;
	int iResult = 0;
	switch (iColumn) {
	case 0:
		if (item1->GetUserName() && item2->GetUserName())
			iResult = CompareLocaleStringNoCase(item1->GetUserName(), item2->GetUserName());
		else if (item1->GetUserName() == NULL)
			iResult = 1; // place clients with no usernames at bottom
		else if (item2->GetUserName() == NULL)
			iResult = -1; // place clients with no usernames at bottom
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
		iResult = CompareUnsigned(item1->GetDatarate(), item2->GetDatarate());
		break;
	case 3:
		iResult = CompareUnsigned64(item1->GetSessionUp(), item2->GetSessionUp());
		if (iResult == 0 && thePrefs.m_bExtControls)
			iResult = CompareUnsigned64(item1->GetQueueSessionPayloadUp(), item2->GetQueueSessionPayloadUp());
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
		iResult = CompareUnsigned(item1->GetUpPartCount(), item2->GetUpPartCount());
		break;
	case 8:
		iResult = CompareLocaleStringNoCase(item1->GetClientSoftVer(), item2->GetClientSoftVer());
		break;
	case 9:
		if (item1->Credits() && item2->Credits()) {
			iResult = CompareUnsigned64(item1->Credits()->GetUploadedTotal(), item2->Credits()->GetUploadedTotal());
		}
		break;
	case 10:
		iResult = CompareLocaleStringNoCase(item1->GetCountryName(), item2->GetCountryName());
	break;
	case 11:
		iResult = CompareLocaleStringNoCase(ipstr(item1->GetIP()), ipstr(item2->GetIP()));
		break;
	case 12:
		iResult = CompareUnsigned(item1->HasLowID(), item2->HasLowID());
		break;
	case 13:
		if (item1->HasValidHash() && item2->HasValidHash()) {
			iResult = CompareLocaleStringNoCase(md4str(item1->GetUserHash()), md4str(item2->GetUserHash()));
		}
		break;
	case 14: // Session Transfer %
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
		if (file1 != NULL && file2 != NULL)
			iResult = CompareUnsigned64(
				(float)item1->GetSessionUp() / (float)file1->GetFileSize() * 1000.0,
				(float)item2->GetSessionUp() / (float)file2->GetFileSize() * 1000.0);
		
	}
		break;
	case 15: // File Size
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
		if (file1 != NULL && file2 != NULL)
			iResult = CompareUnsigned64(file1->GetFileSize(), file2->GetFileSize());
		else
			iResult = (file1 == NULL) ? 1 : -1;
	}
		break;

	case 16: // All Time Ratio
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());
		
		if (file1 != NULL && file2 != NULL)
			iResult = CompareUnsigned(
				100 * file1->GetAllTimeRatio(),
				100 * file2->GetAllTimeRatio());
		else
			iResult = (file1 == NULL) ? 1 : -1;
	}
		break;

	case 17: // Session Ratio
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());

		if (file1 != NULL && file2 != NULL)
			iResult = CompareUnsigned(
				100 * file1->GetRatio(),
				100 * file2->GetRatio());
		else
			iResult = (file1 == NULL) ? 1 : -1;
	}
		break;


	case 18: // Uploaded parts
	{
		UploadingToClient_Struct* pUpClientStruct1 = theApp.uploadqueue->GetUploadingClientStructByClient(item1);
		UploadingToClient_Struct* pUpClientStruct2 = theApp.uploadqueue->GetUploadingClientStructByClient(item2);

		if (pUpClientStruct1 != NULL && pUpClientStruct2 != NULL)
			iResult = CompareUnsigned((int)pUpClientStruct1->m_DoneBlocks_list.GetCount(), (int)pUpClientStruct2->m_DoneBlocks_list.GetCount());
		else
			iResult = 1;
	}
		break;

	case 19: // ETA
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());

		if (file1 != NULL && file2 != NULL) {
			uint64 dataLeft1 = (uint64)file1->GetFileSize() - item1->GetSessionUp();
			uint64 dataRate1 = item1->GetDatarate();

			uint64 dataLeft2 = (uint64)file2->GetFileSize() - item2->GetSessionUp();
			uint64 dataRate2 = item2->GetDatarate();

			if (dataLeft1 > 1024 && dataRate1 > 1024 &&
				dataLeft1 / dataRate1 > 0 && dataLeft1 / dataRate1 < 60 * 60 * 8 &&
				dataLeft2 > 1024 && dataRate2 > 1024 &&
				dataLeft2 / dataRate2 > 0 && dataLeft2 / dataRate2 < 60 * 60 * 8)
				iResult = CompareUnsigned(dataLeft1 / dataRate1, dataLeft2 / dataRate2);
		}
		else
			iResult = 1;
	}
		break;

	case 20: // File Folder
	{
		const CKnownFile* file1 = theApp.sharedfiles->GetFileByID(item1->GetUploadFileID());
		const CKnownFile* file2 = theApp.sharedfiles->GetFileByID(item2->GetUploadFileID());

		if (file1 != NULL && file2 != NULL)
			iResult = CompareLocaleStringNoCase(file1->GetPath(), file2->GetPath());
		else
			iResult = 1;
	}
		break;

	}

	if (lParamSort >= 100)
		iResult = -iResult;

	//call secondary sortorder, if this one results in equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
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

	CTitleMenu ClientMenu;
	ClientMenu.CreatePopupMenu();
	ClientMenu.AddMenuTitle(GetResString(IDS_CLIENTS), true);
	ClientMenu.AppendMenu(MF_STRING | (client ? MF_ENABLED : MF_GRAYED), MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("CLIENTDETAILS"));
	ClientMenu.SetDefaultItem(MP_DETAIL);
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && !client->IsFriend()) ? MF_ENABLED : MF_GRAYED), MP_ADDFRIEND, GetResString(IDS_ADDFRIEND), _T("ADDFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->IsFriend()) ? MF_ENABLED : MF_GRAYED), MP_REMOVEFRIEND, GetResString(IDS_REMOVEFRIEND), _T("DELETEFRIEND"));
	ClientMenu.AppendMenu(MF_STRING | (is_ed2k ? MF_ENABLED : MF_GRAYED), MP_MESSAGE, GetResString(IDS_SEND_MSG), _T("SENDMESSAGE"));
	ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetViewSharedFilesSupport()) ? MF_ENABLED : MF_GRAYED), MP_SHOWLIST, GetResString(IDS_VIEWFILES), _T("VIEWFILES"));
	if (Kademlia::CKademlia::IsRunning() && !Kademlia::CKademlia::IsConnected())
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->GetKadPort() && client->GetKadVersion() >= KADEMLIA_VERSION2_47a) ? MF_ENABLED : MF_GRAYED), MP_BOOT, GetResString(IDS_BOOTSTRAP));
	ClientMenu.AppendMenu(MF_STRING | (GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED), MP_FIND, GetResString(IDS_FIND), _T("Search"));
	
	ClientMenu.AppendMenu(MF_STRING | MF_ENABLED, MP_OPEN, GetResString(IDS_OPENFILE), _T("OPENFILE"));
	ClientMenu.AppendMenu(MF_STRING | MF_ENABLED, MP_OPENFOLDER, GetResString(IDS_OPENFOLDER), _T("OPENFOLDER"));
	ClientMenu.AppendMenu(MF_STRING | MF_ENABLED, MP_COPY_ED2K_HASH, CString("Copy Has&h"));

	if (thePrefs.IsExtControlsEnabled()) {
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && !client->IsBanned()) ? MF_ENABLED : MF_GRAYED), MP_BAN, CString("Ban"));
		ClientMenu.AppendMenu(MF_STRING | ((is_ed2k && client->IsBanned()) ? MF_ENABLED : MF_GRAYED), MP_UNBAN, GetResString(IDS_UNBAN));
	}

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
		case MP_REMOVEFRIEND: {
			CFriend* fr = theApp.friendlist->SearchFriend(client->GetUserHash(), 0, 0);
			if (fr) {
				theApp.friendlist->RemoveFriend(fr);
				Update(iSel);
				}
			}
			break;
		case MP_UNBAN:
			if (client->IsBanned()) {
				client->UnBan();
				Update(iSel);
			}
			break;
		case MP_BAN:
			if (!client->IsBanned()) {
				client->Ban(CString("Arbitrary Ban"));
				Update(iSel);
			}
			break;
		case MP_COPY_ED2K_HASH: {
			const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file && !file->IsPartFile())
				theApp.CopyTextToClipboard(md4str(file->GetFileHash()));
			}
			break;
		case MP_OPEN: {
			const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file && !file->IsPartFile())
				ShellDefaultVerb(file->GetFilePath());
			}
			break;
		case MP_OPENFOLDER: {
			const CKnownFile* file = theApp.sharedfiles->GetFileByID(client->GetUploadFileID());
			if (file && !file->IsPartFile())
				ShellOpen(_T("explorer"), _T("/select,\"") + file->GetFilePath() + _T('\"'));
			}
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
	return true;
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
	if (theApp.IsClosing()
		|| theApp.emuledlg->activewnd != theApp.emuledlg->transferwnd
		|| !theApp.emuledlg->transferwnd->m_pwndTransfer->uploadlistctrl.IsWindowVisible())
	{
		return;
	}

	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)client;
	int iItem = FindItem(&find);
	if (iItem >= 0)
		Update(iItem);
}

void CUploadListCtrl::ShowSelectedUserDetails()
{
	CPoint point;
	if (!::GetCursorPos(&point))
		return;
	ScreenToClient(&point);
	int it = HitTest(point);
	if (it == -1)
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