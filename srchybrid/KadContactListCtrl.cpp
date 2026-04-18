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
#include "KademliaWnd.h"
#include "KadContactListCtrl.h"
#include "emuledlg.h"
#include "GeoLocation.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CONContactListCtrl

IMPLEMENT_DYNAMIC(CKadContactListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CKadContactListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnNmCustomDraw)
	ON_WM_DESTROY()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CKadContactListCtrl::CKadContactListCtrl()
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("KadContactsLv"));
}

void CKadContactListCtrl::Init()
{
	SetPrefsKey(_T("ONContactListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(colID,		  _T(""),	LVCFMT_LEFT, 16 + DFLT_HASH_COL_WIDTH);	//IDS_ID
	InsertColumn(colType,	  _T(""),	LVCFMT_LEFT, 55);						//IDS_TYPE
	InsertColumn(colVersion,  _T(""),	LVCFMT_LEFT, 60);						//IDS_VERSION
	InsertColumn(colDistance, _T(""),	LVCFMT_LEFT, 480);						//IDS_KADDISTANCE
	InsertColumn(colIP,		  _T(""),	LVCFMT_LEFT, 105);						//IDS_IP
	InsertColumn(colLocation, _T(""),	LVCFMT_LEFT, 180);						//IDS_GEOLOCATION

	SetAllIcons();
	Localize();

	LoadSettings();
	int iSortItem = GetSortItem();
	bool bSortAscending = GetSortAscending();

	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, !bSortAscending));
}

void CKadContactListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CKadContactListCtrl::SetAllIcons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("Contact0")));
	iml.Add(CTempIconLoader(_T("Contact1")));
	iml.Add(CTempIconLoader(_T("Contact2")));
	iml.Add(CTempIconLoader(_T("Contact3")));
	iml.Add(CTempIconLoader(_T("Contact4")));
	iml.Add(CTempIconLoader(_T("SrcUnknown"))); // replace
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) == 0);
	HIMAGELIST himl = ApplyImageList(iml.Detach());
	if (himl)
		::ImageList_Destroy(himl);
}

void CKadContactListCtrl::Localize()
{
	static const UINT uids[6] =
	{
		IDS_ID, IDS_TYPE, IDS_VERSION, IDS_KADDISTANCE, IDS_IP, IDS_GEOLOCATION
	};

	LocaliseHeaderCtrl(uids, _countof(uids));
}

CString CKadContactListCtrl::GetContactLocationText(const Kademlia::CContact *contact) const
{
	if (theApp.geolocation == NULL || contact == NULL)
		return CString();
	return theApp.geolocation->GetDisplayText(contact->GetIPAddress());
}

void CKadContactListCtrl::UpdateContact(int iItem, const Kademlia::CContact *contact)
{
	CString id;
	contact->GetClientID(id);
	SetItemText(iItem, colID, id);

	id.Format(_T("%u"), static_cast<unsigned>(contact->GetType()));
	SetItemText(iItem, colType, id);

	id.Format(_T("%u"), static_cast<unsigned>(contact->GetVersion()));
	SetItemText(iItem, colVersion, id);

	contact->GetDistance(id);
	SetItemText(iItem, colDistance, id);

	contact->GetIPAddress(id);
	SetItemText(iItem, colIP, id);

	SetItemText(iItem, colLocation, GetContactLocationText(contact));

	UINT nImageShown;
	if (contact->IsBootstrapContact())
		nImageShown = 5; //contact->IsBootstrapFailed() ? 4 : 5;
	else {
		nImageShown = contact->GetType() > 4 ? 4 : contact->GetType();
		if (nImageShown < 3 && !contact->IsIpVerified())
			nImageShown = 5; // if we have an active contact, which is however not IP verified (and therefore not used), show this icon instead
	}
	SetItem(iItem, 0, LVIF_IMAGE, 0, nImageShown, 0, 0, 0, 0);
}

void CKadContactListCtrl::UpdateKadContactCount()
{
	theApp.emuledlg->kademliawnd->UpdateContactCount();
}

bool CKadContactListCtrl::ContactAdd(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), _T(""), 0, 0, 0, (LPARAM)contact);
		if (iItem >= 0) {
			UpdateContact(iItem, contact);
			UpdateKadContactCount();
			return true;
		}
	} catch (...) {
		ASSERT(0);
	}
	return false;
}

void CKadContactListCtrl::ContactRem(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)contact;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			DeleteItem(iItem);
			UpdateKadContactCount();
		}
	} catch (...) {
		ASSERT(0);
	}
}

void CKadContactListCtrl::ContactRef(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)contact;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			UpdateContact(iItem, contact);
	} catch (...) {
		ASSERT(0);
	}
}

BOOL CKadContactListCtrl::OnCommand(WPARAM, LPARAM)
{
	// ???
	return TRUE;
}

void CKadContactListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	const LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// Determine ascending based on whether already sorted on this column
	bool bSortAscending = (GetSortItem() != pNMLV->iSubItem || !GetSortAscending());

	// Item is column clicked
	int iSortItem = pNMLV->iSubItem;

	// Sort table
	UpdateSortHistory(MAKELONG(iSortItem, !bSortAscending));
	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, !bSortAscending));
	*pResult = 0;
}

void CKadContactListCtrl::OnNmCustomDraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVCUSTOMDRAW *pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	if (pLVCD->nmcd.dwDrawStage == CDDS_PREPAINT) {
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}

	if (pLVCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
		*pResult = CDRF_NOTIFYSUBITEMDRAW;
		return;
	}

	if (pLVCD->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && pLVCD->iSubItem == colLocation) {
		DrawLocationSubItem(pLVCD, pResult);
		return;
	}

	*pResult = CDRF_DODEFAULT;
}

void CKadContactListCtrl::DrawLocationSubItem(NMLVCUSTOMDRAW *pLVCD, LRESULT *pResult)
{
	const Kademlia::CContact *contact = reinterpret_cast<Kademlia::CContact*>(pLVCD->nmcd.lItemlParam);
	if (contact == NULL) {
		*pResult = CDRF_DODEFAULT;
		return;
	}

	CDC *pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
	CRect rcSubItem;
	if (!GetSubItemRect(static_cast<int>(pLVCD->nmcd.dwItemSpec), colLocation, LVIR_LABEL, rcSubItem)) {
		*pResult = CDRF_DODEFAULT;
		return;
	}

	const bool bSelected = (pLVCD->nmcd.uItemState & CDIS_SELECTED) != 0;
	const bool bFocused = GetFocus() == this || (GetStyle() & LVS_SHOWSELALWAYS) != 0;
	const COLORREF crBk = bSelected ? (bFocused ? m_crHighlight : m_crNoHighlight)
		: (m_crWindowTextBk == CLR_NONE ? GetBkColor() : m_crWindowTextBk);
	const COLORREF crText = bSelected ? m_crHighlightText : m_crWindowText;
	pDC->FillSolidRect(&rcSubItem, crBk);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(crText);

	const int iIconY = max((rcSubItem.Height() - 15) / 2, 0);
	const int iItemLeft = rcSubItem.left;
	if (theApp.geolocation != NULL) {
		const POINT point{ iItemLeft + sm_iIconOffset, rcSubItem.top + iIconY };
		if (theApp.geolocation->DrawFlag(*pDC, contact->GetIPAddress(), point))
			rcSubItem.left = iItemLeft + sm_iIconOffset + 18 + sm_iLabelOffset - sm_iSubItemInset;
	}

	rcSubItem.left += sm_iSubItemInset;
	rcSubItem.right -= sm_iSubItemInset;
	pDC->DrawText(GetContactLocationText(contact), &rcSubItem, MLC_DT_TEXT | DT_LEFT);
	*pResult = CDRF_SKIPDEFAULT;
}

int CALLBACK CKadContactListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const Kademlia::CContact *item1 = reinterpret_cast<Kademlia::CContact*>(lParam1);
	const Kademlia::CContact *item2 = reinterpret_cast<Kademlia::CContact*>(lParam2);
	if (item1 == NULL || item2 == NULL)
		return 0;

	int iResult;
	switch (LOWORD(lParamSort)) {
	case colID:
		{
			Kademlia::CUInt128 i1;
			Kademlia::CUInt128 i2;
			item1->GetClientID(i1);
			item2->GetClientID(i2);
			iResult = i1.CompareTo(i2);
		}
		break;
	case colType:
		iResult = static_cast<int>(item1->GetType()) - static_cast<int>(item2->GetType());
		if (iResult == 0)
			iResult = static_cast<int>(item1->GetVersion()) - static_cast<int>(item2->GetVersion());
		break;
	case colVersion:
		iResult = static_cast<int>(item1->GetVersion()) - static_cast<int>(item2->GetVersion());
		break;
	case colDistance:
		{
			Kademlia::CUInt128 distance1, distance2;
			item1->GetDistance(distance1);
			item2->GetDistance(distance2);
			iResult = distance1.CompareTo(distance2);
		}
		break;
	case colIP:
		iResult = CompareUnsigned(item1->GetIPAddress(), item2->GetIPAddress());
		break;
	case colLocation:
		if (theApp.geolocation != NULL)
			iResult = CompareLocaleStringNoCase(theApp.geolocation->GetDisplayText(item1->GetIPAddress()), theApp.geolocation->GetDisplayText(item2->GetIPAddress()));
		else
			iResult = 0;
		break;
	default:
		return 0;
	}
	return HIWORD(lParamSort) ? -iResult : iResult;
}
