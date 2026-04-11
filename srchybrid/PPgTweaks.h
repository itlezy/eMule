#pragma once
#include "TreeOptionsCtrlEx.h"

class CPPgTweaks : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgTweaks)

	enum
	{
		IDD = IDD_PPG_TWEAKS
	};
	void LocalizeItemText(HTREEITEM item, UINT strid);
	void LocalizeEditLabel(HTREEITEM item, UINT strid);

public:
	CPPgTweaks();

	void Localize();

protected:
	CSliderCtrl m_ctlFileBuffSize;
	CSliderCtrl m_ctlQueueSize;
	CTreeOptionsCtrlEx m_ctrlTreeOptions;
	CString m_sYourHostname;

	HTREEITEM m_htiA4AFSaveCpu;
	HTREEITEM m_htiAutoArch;
	HTREEITEM m_htiAutoTakeEd2kLinks;
	HTREEITEM m_htiBroadband;
	HTREEITEM m_htiBBMaxUploadClients;
	HTREEITEM m_htiBBSlowThreshold;
	HTREEITEM m_htiBBSlowGrace;
	HTREEITEM m_htiBBSlowWarmup;
	HTREEITEM m_htiBBZeroRateGrace;
	HTREEITEM m_htiBBCooldown;
	HTREEITEM m_htiBBLowRatioBoost;
	HTREEITEM m_htiBBLowRatioThreshold;
	HTREEITEM m_htiBBLowRatioBonus;
	HTREEITEM m_htiBBLowIdDivisor;
	HTREEITEM m_htiBBSessionTransfer;
	HTREEITEM m_htiBBSessionTransferDisabled;
	HTREEITEM m_htiBBSessionTransferPercent;
	HTREEITEM m_htiBBSessionTransferMiB;
	HTREEITEM m_htiBBSessionTransferPercentValue;
	HTREEITEM m_htiBBSessionTransferMiBValue;
	HTREEITEM m_htiBBSessionTimeLimit;
	HTREEITEM m_htiCheckDiskspace;
	HTREEITEM m_htiCloseUPnPPorts;
	HTREEITEM m_htiCommit;
	HTREEITEM m_htiCommitAlways;
	HTREEITEM m_htiCommitNever;
	HTREEITEM m_htiCommitOnShutdown;
	HTREEITEM m_htiConditionalTCPAccept;
	HTREEITEM m_htiCreditSystem;
	HTREEITEM m_htiDebug2Disk;
	HTREEITEM m_htiDebugSourceExchange;
	HTREEITEM m_htiExtControls;
	HTREEITEM m_htiHiddenDisplay;
	HTREEITEM m_htiHiddenFile;
	HTREEITEM m_htiHiddenSecurity;
	HTREEITEM m_htiHiddenStartup;
	HTREEITEM m_htiExtractMetaData;
	HTREEITEM m_htiExtractMetaDataID3Lib;
	//HTREEITEM m_htiExtractMetaDataMediaDet;
	HTREEITEM m_htiExtractMetaDataNever;
	HTREEITEM m_htiFilterLANIPs;
	HTREEITEM m_htiFullAlloc;
	HTREEITEM m_htiImportParts;
	HTREEITEM m_htiInspectAllFileTypes;
	HTREEITEM m_htiLog2Disk;
	HTREEITEM m_htiLogA4AF;
	HTREEITEM m_htiLogBannedClients;
	HTREEITEM m_htiLogFileSaving;
	HTREEITEM m_htiLogFilteredIPs;
	HTREEITEM m_htiLogLevel;
	HTREEITEM m_htiLogRatingDescReceived;
	HTREEITEM m_htiLogSecureIdent;
	HTREEITEM m_htiLogUlDlEvents;
	HTREEITEM m_htiConnectionTimeout;
	HTREEITEM m_htiMaxCon5Sec;
	HTREEITEM m_htiMaxHalfOpen;
	HTREEITEM m_htiDateTimeFormat4Lists;
	HTREEITEM m_htiPreviewCopiedArchives;
	HTREEITEM m_htiPreviewOnIconDblClk;
	HTREEITEM m_htiShowActiveDownloadsBold;
	HTREEITEM m_htiUseSystemFontForMainControls;
	HTREEITEM m_htiReBarToolbar;
	HTREEITEM m_htiShowUpDownIconInTaskbar;
	HTREEITEM m_htiShowVerticalHourMarkers;
	HTREEITEM m_htiForceSpeedsToKB;
	HTREEITEM m_htiExtraPreviewWithMenu;
	HTREEITEM m_htiKeepUnavailableFixedSharedDirs;
	HTREEITEM m_htiPartiallyPurgeOldKnownFiles;
	HTREEITEM m_htiAdjustNTFSDaylightFileTime;
	HTREEITEM m_htiRearrangeKadSearchKeywords;
	HTREEITEM m_htiMessageFromValidSourcesOnly;
	HTREEITEM m_htiFileBufferTimeLimit;
	HTREEITEM m_htiDownloadTimeout;
	HTREEITEM m_htiRestoreLastLogPane;
	HTREEITEM m_htiRestoreLastMainWndDlg;
	HTREEITEM m_htiMinFreeDiskSpace;
	HTREEITEM m_htiResolveShellLinks;
	HTREEITEM m_htiServerKeepAliveTimeout;
	HTREEITEM m_htiShareeMule;
	HTREEITEM m_htiShareeMuleMultiUser;
	HTREEITEM m_htiShareeMuleOldStyle;
	HTREEITEM m_htiShareeMulePublicUser;
	HTREEITEM m_htiSkipWANIPSetup;
	HTREEITEM m_htiSkipWANPPPSetup;
	HTREEITEM m_htiSparsePartFiles;
	HTREEITEM m_htiTCPGroup;
	HTREEITEM m_htiUPnP;
	HTREEITEM m_htiVerbose;
	HTREEITEM m_htiVerboseGroup;
	HTREEITEM m_htiYourHostname;

	int m_iMinFreeDiskSpaceGB;
	INT_PTR m_iQueueSize;
	UINT m_uFileBufferSize;
	UINT m_uConnectionTimeoutSeconds;
	UINT m_uDownloadTimeoutSeconds;
	UINT m_uServerKeepAliveTimeout;
	int m_iCommitFiles;
	int m_iExtractMetaData;
	int m_iInspectAllFileTypes;
	int m_iLogLevel;
	int m_iMaxConnPerFive;
	int m_iMaxHalfOpen;
	int m_iShareeMule;
	int m_iBBSessionTransferMode;

	CString m_sDateTimeFormat4Lists;
	CString m_sBBSlowThresholdFactor;
	CString m_sBBLowRatioThreshold;

	bool m_bA4AFSaveCpu;
	bool m_bAutoArchDisable;
	bool m_bAutoTakeEd2kLinks;
	bool m_bBBLowRatioBoost;
	bool m_bAdjustNTFSDaylightFileTime;
	bool m_bCheckDiskspace;
	bool m_bCloseUPnPOnExit;
	bool m_bConditionalTCPAccept;
	bool m_bCreditSystem;
	bool m_bDebug2Disk;
	bool m_bDebugSourceExchange;
	bool m_bExtControls;
	bool m_bExtraPreviewWithMenu;
	bool m_bFilterLANIPs;
	bool m_bFullAlloc;
	bool m_bImportParts;
	bool m_bInitializedTreeOpts;
	bool m_bKeepUnavailableFixedSharedDirs;
	bool m_bLog2Disk;
	bool m_bLogA4AF;
	bool m_bLogBannedClients;
	bool m_bLogFileSaving;
	bool m_bLogFilteredIPs;
	bool m_bLogRatingDescReceived;
	bool m_bLogSecureIdent;
	bool m_bLogUlDlEvents;
	bool m_bMessageFromValidSourcesOnly;
	bool m_bPartiallyPurgeOldKnownFiles;
	bool m_bPreviewCopiedArchives;
	bool m_bPreviewOnIconDblClk;
	bool m_bRearrangeKadSearchKeywords;
	bool m_bReBarToolbar;
	bool m_bResolveShellLinks;
	bool m_bRestoreLastLogPane;
	bool m_bRestoreLastMainWndDlg;
	bool m_bShowedWarning;
	bool m_bShowActiveDownloadsBold;
	bool m_bShowUpDownIconInTaskbar;
	bool m_bShowVerticalHourMarkers;
	bool m_bSkipWANIPSetup;
	bool m_bSkipWANPPPSetup;
	bool m_bSparsePartFiles;
	bool m_bUseSystemFontForMainControls;
	bool m_bVerbose;
	bool m_bForceSpeedsToKB;

	UINT m_uFileBufferTimeLimitSeconds;
	int m_iBBMaxUploadClients;
	int m_iBBSlowGraceSeconds;
	int m_iBBSlowWarmupSeconds;
	int m_iBBZeroRateGraceSeconds;
	int m_iBBCooldownSeconds;
	int m_iBBLowRatioBonus;
	int m_iBBLowIdDivisor;
	int m_iBBSessionTransferPercent;
	int m_iBBSessionTransferMiB;
	int m_iBBSessionTimeLimitSeconds;

	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnKillActive();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnBnClickedOpenprefini();
};
