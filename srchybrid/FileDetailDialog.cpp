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
#include "FileDetailDialog.h"
#include "PartFile.h"
#include "HighColorTab.hpp"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////////////
// Helper Functions for FileDetail and SharedFileDetailsSheet dialogs

///////////////////////////////////////////////////////////////////////////////
// CFileDetailDialog

LPCTSTR CFileDetailDialog::m_pPshStartPage;

IMPLEMENT_DYNAMIC(CFileDetailDialog, CListViewWalkerPropertySheet)

BEGIN_MESSAGE_MAP(CFileDetailDialog, CListViewWalkerPropertySheet)
	ON_WM_DESTROY()
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
END_MESSAGE_MAP()

void CFileDetailDialog::Localize()
{
	m_wndInfo.Localize();
	SetTabTitle(IDS_FD_GENERAL, &m_wndInfo, this);
	m_wndName.Localize();
	SetTabTitle(IDS_SW_NAME, &m_wndName, this);
	m_wndComments.Localize();
	SetTabTitle(IDS_CMT_READALL, &m_wndComments, this);
	m_wndMediaInfo.Localize();
	SetTabTitle(IDS_CONTENT_INFO, &m_wndMediaInfo, this);
	m_wndMetaData.Localize();
	SetTabTitle(IDS_META_DATA, &m_wndMetaData, this);
	m_wndFileLink.Localize();
	SetTabTitle(IDS_SW_LINK, &m_wndFileLink, this);
}

CFileDetailDialog::CFileDetailDialog(const CSimpleArray<CPartFile*> *paFiles, UINT uInvokePage, CListCtrlItemWalk *pListCtrl)
	: CListViewWalkerPropertySheet(pListCtrl)
	, m_uInvokePage(uInvokePage)
{
	for (int i = 0; i < paFiles->GetSize(); ++i)
		m_aItems.Add((*paFiles)[i]);
	m_psh.dwFlags &= ~PSH_HASHELP;

	m_wndInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndInfo.m_psp.pszIcon = _T("FILEINFO");
	m_wndInfo.SetFiles(&m_aItems);
	AddPage(&m_wndInfo);

	if (m_aItems.GetSize() == 1) {
		m_wndName.m_psp.dwFlags &= ~PSP_HASHELP;
		m_wndName.m_psp.dwFlags |= PSP_USEICONID;
		m_wndName.m_psp.pszIcon = _T("FILERENAME");
		m_wndName.SetFiles(&m_aItems);
		AddPage(&m_wndName);
	}

	m_wndComments.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndComments.m_psp.dwFlags |= PSP_USEICONID;
	m_wndComments.m_psp.pszIcon = _T("FILECOMMENTS");
	m_wndComments.SetFiles(&m_aItems);
	AddPage(&m_wndComments);

	m_wndMediaInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMediaInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMediaInfo.m_psp.pszIcon = _T("MEDIAINFO");
	m_wndMediaInfo.SetFiles(&m_aItems);
	AddPage(&m_wndMediaInfo);

	m_wndMetaData.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMetaData.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMetaData.m_psp.pszIcon = _T("METADATA");
	if (thePrefs.IsExtControlsEnabled() && m_aItems.GetSize() == 1) {
		m_wndMetaData.SetFiles(&m_aItems);
		AddPage(&m_wndMetaData);
	}

	m_wndFileLink.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndFileLink.m_psp.dwFlags |= PSP_USEICONID;
	m_wndFileLink.m_psp.pszIcon = _T("ED2KLINK");
	m_wndFileLink.SetFiles(&m_aItems);
	AddPage(&m_wndFileLink);

	LPCTSTR pPshStartPage = m_pPshStartPage;
	if (m_uInvokePage != 0)
		pPshStartPage = MAKEINTRESOURCE(m_uInvokePage);
	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == pPshStartPage) {
			m_psh.nStartPage = i;
			break;
		}
}

void CFileDetailDialog::OnDestroy()
{
	if (m_uInvokePage == 0)
		m_pPshStartPage = GetPage(GetActiveIndex())->m_psp.pszTemplate;
	CListViewWalkerPropertySheet::OnDestroy();
}

BOOL CFileDetailDialog::OnInitDialog()
{
	EnableStackedTabs(FALSE);
	BOOL bResult = CListViewWalkerPropertySheet::OnInitDialog();
	HighColorTab::UpdateImageList(*this);
	InitWindowStyles(this);
	EnableSaveRestore(_T("FileDetailDialog")); // call this after(!) OnInitDialog
	Localize();
	UpdateTitle();
	return bResult;
}

LRESULT CFileDetailDialog::OnDataChanged(WPARAM, LPARAM)
{
	UpdateTitle();
	/** Keep dependent tabs in sync after the archive page removal. */
	if (m_wndMediaInfo.m_hWnd)
		m_wndMediaInfo.SendMessage(UM_DATA_CHANGED);
	if (m_wndFileLink.m_hWnd)
		m_wndFileLink.SendMessage(UM_DATA_CHANGED);
	return 1;
}

void CFileDetailDialog::UpdateTitle()
{
	CString sTitle(GetResString(IDS_DETAILS));
	if (m_aItems.GetSize() == 1)
		sTitle.AppendFormat(_T(": %s"), (LPCTSTR)(static_cast<CAbstractFile*>(m_aItems[0])->GetFileName()));
	SetWindowText(sTitle);
}
