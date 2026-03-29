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
	HTREEITEM m_htiBBLowIDDeboost;
	HTREEITEM m_htiBBLowIDDeboostDivisor;
	HTREEITEM m_htiBBLowRatioBoost;
	HTREEITEM m_htiBBLowRatioBonus;
	HTREEITEM m_htiBBLowRatioThreshold;
	HTREEITEM m_htiBBMaxUpClientsAllowed;
	HTREEITEM m_htiBBSessionMaxTime;
	HTREEITEM m_htiBBSessionMaxTimeMinutes;
	HTREEITEM m_htiBBSessionTransferLimit;
	HTREEITEM m_htiBBSessionTransAbsolute;
	HTREEITEM m_htiBBSessionTransAbsoluteValue;
	HTREEITEM m_htiBBSessionTransDisabled;
	HTREEITEM m_htiBBSessionTransPercent;
	HTREEITEM m_htiBBSessionTransPercentValue;
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
	HTREEITEM m_htiDynUp;
	HTREEITEM m_htiDynUpEnabled;
	HTREEITEM m_htiDynUpGoingDownDivider;
	HTREEITEM m_htiDynUpGoingUpDivider;
	HTREEITEM m_htiDynUpMinUpload;
	HTREEITEM m_htiDynUpNumberOfPings;
	HTREEITEM m_htiDynUpPingTolerance;
	HTREEITEM m_htiDynUpPingToleranceGroup;
	HTREEITEM m_htiDynUpPingToleranceMilliseconds;
	HTREEITEM m_htiDynUpRadioPingTolerance;
	HTREEITEM m_htiDynUpRadioPingToleranceMilliseconds;
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
	HTREEITEM m_htiMaxCon5Sec;
	HTREEITEM m_htiMaxHalfOpen;
	HTREEITEM m_htiTCPBigSendBuffer;
	HTREEITEM m_htiUDPReceiveBuffer;
	HTREEITEM m_htiMinFreeDiskSpace;
	HTREEITEM m_htiResolveShellLinks;
	HTREEITEM m_htiRestoreLastLogPane;
	HTREEITEM m_htiRestoreLastMainWndDlg;
	HTREEITEM m_htiServerKeepAliveTimeout;
	HTREEITEM m_htiShareeMule;
	HTREEITEM m_htiShareeMuleMultiUser;
	HTREEITEM m_htiShareeMuleOldStyle;
	HTREEITEM m_htiShareeMulePublicUser;
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
	HTREEITEM m_htiPreferRestrictedOverUser;
	HTREEITEM m_htiPartiallyPurgeOldKnownFiles;
	HTREEITEM m_htiAdjustNTFSDaylightFileTime;
	HTREEITEM m_htiRearrangeKadSearchKeywords;
	HTREEITEM m_htiMessageFromValidSourcesOnly;
	HTREEITEM m_htiFileBufferTimeLimit;
	HTREEITEM m_htiSkipWANIPSetup;
	HTREEITEM m_htiSkipWANPPPSetup;
	HTREEITEM m_htiSparsePartFiles;
	HTREEITEM m_htiTCPGroup;
	HTREEITEM m_htiUPnP;
	HTREEITEM m_htiVerbose;
	HTREEITEM m_htiVerboseGroup;
	HTREEITEM m_htiYourHostname;

	float m_fMinFreeDiskSpaceGB;
	float m_fBBLowRatioBonus;
	float m_fBBLowRatioThreshold;
	INT_PTR m_iQueueSize;
	UINT m_uFileBufferSize;
	UINT m_uTCPBigSendBufferSizeKiB;
	UINT m_uUDPReceiveBufferSizeKiB;
	UINT m_uBBSessionMaxTimeMinutes;
	UINT m_uBBSessionTransAbsoluteMiB;
	UINT m_uServerKeepAliveTimeout;
	int m_iBBLowIDDeboostDivisor;
	int m_iBBMaxUpClientsAllowed;
	int m_iBBSessionTransMode;
	int m_iBBSessionTransPercent;
	int m_iCommitFiles;
	int m_iDynUpGoingDownDivider;
	int m_iDynUpGoingUpDivider;
	int m_iDynUpMinUpload;
	int m_iDynUpNumberOfPings;
	int m_iDynUpPingTolerance;
	int m_iDynUpPingToleranceMilliseconds;
	int m_iDynUpRadioPingTolerance;
	int m_iExtractMetaData;
	int m_iInspectAllFileTypes;
	int m_iLogLevel;
	int m_iMaxConnPerFive;
	int m_iMaxHalfOpen;
	int m_iShareeMule;

	bool m_bA4AFSaveCpu;
	bool m_bAutoArchDisable;
	bool m_bAutoTakeEd2kLinks;
	bool m_bBBLowIDDeboost;
	bool m_bBBLowRatioBoost;
	bool m_bBBSessionMaxTime;
	bool m_bCheckDiskspace;
	bool m_bCloseUPnPOnExit;
	bool m_bConditionalTCPAccept;
	bool m_bCreditSystem;
	bool m_bDebug2Disk;
	bool m_bDebugSourceExchange;
	bool m_bDynUpEnabled;
	bool m_bExtControls;
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
	bool m_bPreferRestrictedOverUser;
	bool m_bPreviewCopiedArchives;
	bool m_bPreviewOnIconDblClk;
	bool m_bRearrangeKadSearchKeywords;
	bool m_bReBarToolbar;
	bool m_bRestoreLastLogPane;
	bool m_bRestoreLastMainWndDlg;
	bool m_bResolveShellLinks;
	bool m_bShowActiveDownloadsBold;
	bool m_bShowedWarning;
	bool m_bShowUpDownIconInTaskbar;
	bool m_bShowVerticalHourMarkers;
	bool m_bSkipWANIPSetup;
	bool m_bSkipWANPPPSetup;
	bool m_bSparsePartFiles;
	bool m_bUseSystemFontForMainControls;
	bool m_bVerbose;
	bool m_bAdjustNTFSDaylightFileTime;
	bool m_bExtraPreviewWithMenu;
	bool m_bForceSpeedsToKB;

	CString m_sDateTimeFormat4Lists;
	UINT m_uFileBufferTimeLimitSeconds;

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
