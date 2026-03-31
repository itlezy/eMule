#pragma once

#include "../../eMule-ResizableLib/ResizableLib/ResizableDialog.h"
#include "RichEditCtrlX.h"

// CNetworkInfoDlg dialog

class CNetworkInfoDlg : public CResizableDialog
{
	DECLARE_DYNAMIC(CNetworkInfoDlg)

	enum
	{
		IDD = IDD_NETWORK_INFO
	};

public:
	explicit CNetworkInfoDlg(CWnd *pParent = NULL);   // standard constructor

protected:
	CRichEditCtrlX m_info;
	CHARFORMAT m_cfDef;
	CHARFORMAT m_cfBold;

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	/**
	 * Refreshes the summary fields and the detailed report from the current runtime state.
	 */
	void RefreshNetworkInformation();

	/**
	 * Builds the plain-text report copied to the clipboard.
	 */
	CString BuildClipboardReport() const;

	afx_msg void OnBnClickedReload();
	afx_msg void OnBnClickedCopy();

	DECLARE_MESSAGE_MAP()
};

void CreateNetworkInfo(CRichEditCtrlX &rCtrl, CHARFORMAT &rcfDef, CHARFORMAT &rcfBold, bool bFullInfo = false);
