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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "emule.h"
#include "ArchivePreviewDlg.h"
#include "ShareableFile.h"
#include "OtherFunctions.h"
#include "UserMsgs.h"
#include "SplitterControl.h"
#include "MenuCmds.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////
// COLUMN_INIT -- List View Columns

enum EArchiveCols
{
	ARCHPREV_COL_NAME = 0,
	ARCHPREV_COL_SIZE,
	ARCHPREV_COL_CRC,
	ARCHPREV_COL_ATTR,
	ARCHPREV_COL_TIME,
	ARCHPREV_COL_CMT
};

static LCX_COLUMN_INIT s_aColumns[] =
{
	{ ARCHPREV_COL_NAME, _T("Name"),  IDS_DL_FILENAME,  LVCFMT_LEFT,  -1, 0, ASCENDING, NONE, _T("LONG FILENAME.DAT") },
	{ ARCHPREV_COL_SIZE, _T("Size"),  IDS_DL_SIZE,      LVCFMT_RIGHT, -1, 1, ASCENDING, NONE, _T("9999 MByte") },
	{ ARCHPREV_COL_CRC,  _T("CRC"),   UINT_MAX,         LVCFMT_LEFT,  -1, 2, ASCENDING, NONE, _T("1234abcd")  },
	{ ARCHPREV_COL_ATTR, _T("Attr"),  IDS_ATTRIBUTES,   LVCFMT_LEFT,  -1, 3, ASCENDING, NONE, _T("mmm") },
	{ ARCHPREV_COL_TIME, _T("Time"),  IDS_LASTMODIFIED, LVCFMT_LEFT,  -1, 4, ASCENDING, NONE, _T("12.12.1990 00:00:00") },
	{ ARCHPREV_COL_CMT,  _T("Cmt"),   IDS_COMMENT,      LVCFMT_LEFT,  -1, 5, ASCENDING, NONE, _T("Long long long long comment") }
};

#define	PREF_INI_SECTION	_T("ArchivePreviewDlg")

namespace
{
	/** Returns the fixed message shown after the internal archive inspector was removed. */
	LPCTSTR GetRetiredArchivePreviewMessage()
	{
		return _T("Archive inspection is no longer available. Use an external archive tool such as 7-Zip or WinRAR after the download completes.");
	}
}

IMPLEMENT_DYNAMIC(CArchivePreviewDlg, CResizablePage)

BEGIN_MESSAGE_MAP(CArchivePreviewDlg, CResizablePage)
	ON_BN_CLICKED(IDC_AP_EXPLAIN, OnBnExplain)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_DESTROY()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(MP_HM_HELP, OnBnExplain)
END_MESSAGE_MAP()

CArchivePreviewDlg::CArchivePreviewDlg()
	: CResizablePage(CArchivePreviewDlg::IDD)
	, m_pFile()
	, m_paFiles()
	, m_bDataChanged()
	, m_bReducedDlg()
{
	m_strCaption = GetResString(IDS_CONTENT_INFO);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;

	m_ContentList.m_pParent = this;
	m_ContentList.SetRegistryKey(PREF_INI_SECTION);
	m_ContentList.SetRegistryPrefix(_T("ContentList_"));
}

void CArchivePreviewDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILELIST, m_ContentList);
	DDX_Control(pDX, IDC_ARCHPROGRESS, m_progressbar);
}

BOOL CArchivePreviewDlg::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	if (!m_bReducedDlg) {
		AddAnchor(IDC_APV_FILEINFO, TOP_LEFT, TOP_RIGHT);
		AddAnchor(IDC_ARCP_ATTRIBS, TOP_CENTER);
		AddAnchor(IDC_INFO_ATTR, TOP_CENTER, TOP_RIGHT);
		AddAnchor(IDC_AP_EXPLAIN, BOTTOM_LEFT);
		AddAnchor(IDC_INFO_STATUS, TOP_LEFT, TOP_RIGHT);
	} else {
		CRect rc;
		int nDelta = 0;
		if (CWnd *pFileInfoWnd = GetDlgItem(IDC_APV_FILEINFO)) {
			pFileInfoWnd->GetWindowRect(rc);
			nDelta = rc.Height();
		}

		/** Guard reduced-layout controls so dialog-template drift cannot crash the page. */
		if (CWnd *pFileListWnd = GetDlgItem(IDC_FILELIST)) {
			CSplitterControl::ChangePos(pFileListWnd, 0, -nDelta);
			CSplitterControl::ChangeHeight(pFileListWnd, nDelta);
		}
		if (CWnd *pFileInfoWnd = GetDlgItem(IDC_APV_FILEINFO))
			pFileInfoWnd->ShowWindow(SW_HIDE);
		if (CWnd *pArcpAttribsWnd = GetDlgItem(IDC_ARCP_ATTRIBS))
			pArcpAttribsWnd->ShowWindow(SW_HIDE);
		if (CWnd *pInfoAttrWnd = GetDlgItem(IDC_INFO_ATTR))
			pInfoAttrWnd->ShowWindow(SW_HIDE);
		if (CWnd *pInfoStatusWnd = GetDlgItem(IDC_INFO_STATUS))
			pInfoStatusWnd->ShowWindow(SW_HIDE);
		if (CWnd *pInfoTypeWnd = GetDlgItem(IDC_INFO_TYPE))
			pInfoTypeWnd->ShowWindow(SW_HIDE);
		if (CWnd *pArcpTypeWnd = GetDlgItem(IDC_ARCP_TYPE))
			pArcpTypeWnd->ShowWindow(SW_HIDE);
		if (CWnd *pArcpStatusWnd = GetDlgItem(IDC_ARCP_STATUS))
			pArcpStatusWnd->ShowWindow(SW_HIDE);
	}
	if (CWnd *pReadArchWnd = GetDlgItem(IDC_READARCH))
		pReadArchWnd->ShowWindow(SW_HIDE);
	if (CWnd *pRestoreArchWnd = GetDlgItem(IDC_RESTOREARCH))
		pRestoreArchWnd->ShowWindow(SW_HIDE);

	AddAnchor(IDC_INFO_FILECOUNT, BOTTOM_RIGHT);
	AddAnchor(IDC_ARCHPROGRESS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_FILELIST, TOP_LEFT, BOTTOM_RIGHT);

	AddAllOtherAnchors();

	ASSERT(m_ContentList.GetStyle() & LVS_SORTASCENDING);
	ASSERT(m_ContentList.GetStyle() & LVS_SHAREIMAGELISTS);
	m_ContentList.SendMessage(LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)theApp.GetSystemImageList());
	m_ContentList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_GRIDLINES);
	m_ContentList.EnableHdrCtrlSortBitmaps();

	m_ContentList.ReadColumnStats(_countof(s_aColumns), s_aColumns);
	m_ContentList.CreateColumns(_countof(s_aColumns), s_aColumns);
	m_ContentList.InitColumnOrders(_countof(s_aColumns), s_aColumns);
	m_ContentList.UpdateSortColumn(_countof(s_aColumns), s_aColumns);

	CResizablePage::UpdateData(FALSE);
	Localize();

	m_progressbar.SetRange(0, 1000);
	m_progressbar.SetPos(0);

	return TRUE;
}

void CArchivePreviewDlg::OnDestroy()
{
	m_ContentList.WriteColumnStats(_countof(s_aColumns), s_aColumns);

	// This property sheet's window may get destroyed and re-created several times although
	// the corresponding C++ class is kept -> explicitly reset ResizableLib state
	RemoveAllAnchors();

	CResizablePage::OnDestroy();
}

BOOL CArchivePreviewDlg::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;

	if (m_bDataChanged) {
		UpdateArchiveDisplay();
		m_bDataChanged = false;
	}
	return TRUE;
}

LRESULT CArchivePreviewDlg::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CArchivePreviewDlg::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_CONTENT_INFO, this);

	if (!m_bReducedDlg) {
		SetDlgItemText(IDC_ARCP_TYPE, GetResString(IDS_ARCHTYPE) + _T(':'));
		SetDlgItemText(IDC_ARCP_STATUS, GetResString(IDS_STATUS) + _T(':'));
		SetDlgItemText(IDC_ARCP_ATTRIBS, GetResString(IDS_INFO) + _T(':'));
	}
}

void CArchivePreviewDlg::OnBnExplain()
{
	AfxMessageBox(GetRetiredArchivePreviewMessage(), MB_OK | MB_ICONINFORMATION);
}

void CArchivePreviewDlg::UpdateArchiveDisplay()
{
	if (m_paFiles == NULL || m_paFiles->GetSize() <= 0)
		m_pFile = NULL;
	else if (m_pFile == static_cast<CShareableFile*>(const_cast<CObject*>((*m_paFiles)[0])))
		return;

	m_progressbar.SetPos(0);
	m_ContentList.DeleteAllItems();
	m_ContentList.UpdateWindow();

	SetDlgItemText(IDC_APV_FILEINFO, _T(""));
	SetDlgItemText(IDC_INFO_ATTR, _T(""));
	SetDlgItemText(IDC_INFO_STATUS, _T(""));
	SetDlgItemText(IDC_INFO_FILECOUNT, _T(""));

	if (m_paFiles == NULL || m_paFiles->GetSize() <= 0)
		return;

	CShareableFile *pFile = static_cast<CShareableFile*>(const_cast<CObject*>((*m_paFiles)[0]));
	m_pFile = pFile;

	EFileType type = GetFileTypeEx(pFile);
	LPCTSTR pType;
	switch (type) {
	case ARCHIVE_ZIP:
		pType = _T("ZIP");
		break;
	case ARCHIVE_RAR:
		pType = _T("RAR");
		break;
	case ARCHIVE_ACE:
		pType = _T("ACE");
		break;
	case IMAGE_ISO:
		pType = _T("ISO");
		break;
	default:
		SetDlgItemText(IDC_INFO_TYPE, GetResString(IDS_ARCPREV_UNKNOWNFORMAT));
		return;
	}

	SetDlgItemText(IDC_INFO_TYPE, pType);

	/** The legacy archive scanner was removed in favor of external archive tools. */
	SetDlgItemText(IDC_INFO_STATUS, GetRetiredArchivePreviewMessage());
	m_ContentList.InsertItem(INT_MAX, GetRetiredArchivePreviewMessage());
}

void CArchivePreviewDlg::OnContextMenu(CWnd *pWnd, CPoint point)
{
	if (m_bReducedDlg) {
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, MP_HM_HELP, GetResString(IDS_EM_HELP));
		menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	} else
		__super::OnContextMenu(pWnd, point);
}
