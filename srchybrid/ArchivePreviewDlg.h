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
#pragma once
#include "ResizablePage.h"
#include "ListCtrlX.h"

class CShareableFile;

///////////////////////////////////////////////////////////////////////////////
// CArchivePreviewDlg

class CArchivePreviewDlg : public CResizablePage
{
	DECLARE_DYNAMIC(CArchivePreviewDlg)

	enum
	{
		IDD = IDD_ARCHPREV
	};

	CShareableFile *m_pFile; // archive page state belongs to this file

public:
	CArchivePreviewDlg();
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void SetReducedDialog()									{ m_bReducedDlg = true; }
	void Localize();

protected:
	const CSimpleArray<CObject*> *m_paFiles;

	CListCtrlX	m_ContentList;
	bool		m_bDataChanged;
	bool		m_bReducedDlg;

	/** Refreshes the archive page after archive recovery support was retired. */
	void UpdateArchiveDisplay();

	virtual void DoDataExchange(CDataExchange *pDX);	// DDX/DDV support
	virtual BOOL OnSetActive();

	CProgressCtrl m_progressbar;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnExplain();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnDestroy();
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
};
