//this file is part of eMule
//Copyright (C)2020-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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
#pragma once

class CSMTPserverDlg : public CDialog
{
	DECLARE_DYNAMIC(CSMTPserverDlg)

	enum
	{
		IDD = IDD_SMTPSERVER
	};
public:
	explicit CSMTPserverDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CSMTPserverDlg();

	void Localize();

protected:
	HICON m_icoWnd;
	EmailSettings m_mail;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnCbnSelChangeSecurity();
	afx_msg void OnCbnSelChangeAuthMethod();
};
