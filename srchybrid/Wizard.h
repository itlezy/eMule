#pragma once

class CConnectionWizardDlg : public CDialog
{
	DECLARE_DYNAMIC(CConnectionWizardDlg)

	enum
	{
		IDD = IDD_WIZARD
	};

public:
	explicit CConnectionWizardDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CConnectionWizardDlg();

	void Localize();

protected:
	HICON m_icoWnd;
	CListCtrl m_profileList;
	int m_iTotalDownload;

	static UINT GetRateInKBitsPerSec(uint32 nRateInKBytesPerSec);
	static int GetDefaultConcurrentDownloadPreset();
	void InitProfileList();
	void LoadSelectedProfile();
	void SetBandwidthInputs(UINT nDownloadKBitPerSec, UINT nUploadKBitPerSec);
	void SetCustomItemsActivation();

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedWizHighdownloadRadio();
	afx_msg void OnBnClickedWizLowdownloadRadio();
	afx_msg void OnBnClickedWizMediumdownloadRadio();
	afx_msg void OnNmClickProviders(LPNMHDR, LRESULT *pResult);
};
