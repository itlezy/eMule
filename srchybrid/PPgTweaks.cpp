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
#include "opcodes.h"
#include "OtherFunctions.h"
#include "SearchDlg.h"
#include "PPgTweaks.h"
#include "Scheduler.h"
#include "DownloadQueue.h"
#include "Preferences.h"
#include "TransferDlg.h"
#include "emuledlg.h"
#include "ClientUDPSocket.h"
#include "SharedFilesWnd.h"
#include "ServerWnd.h"
#include "HelpIDs.h"
#include "Log.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	DFLT_MAXCONPERFIVE	50
#define DFLT_MAXHALFOPEN	50

namespace
{
	enum EBroadbandSessionTransferMode
	{
		BBSTM_DISABLED = 0,
		BBSTM_PERCENT = 1,
		BBSTM_ABSOLUTE = 2
	};

	static void FailTreeValidation(CDataExchange *pDX, UINT uMessageId, HTREEITEM hItem)
	{
		AfxMessageBox(uMessageId);
		pDX->PrepareEditCtrl(IDC_EXT_OPTS);
		pDX->Fail();
		UNREFERENCED_PARAMETER(hItem);
	}
}

///////////////////////////////////////////////////////////////////////////////
// CPPgTweaks dialog

IMPLEMENT_DYNAMIC(CPPgTweaks, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgTweaks, CPropertyPage)
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_MESSAGE(UM_TREEOPTSCTRL_NOTIFY, OnTreeOptsCtrlNotify)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_OPENPREFINI, OnBnClickedOpenprefini)
END_MESSAGE_MAP()

CPPgTweaks::CPPgTweaks()
	: CPropertyPage(CPPgTweaks::IDD)
	, m_ctrlTreeOptions(theApp.m_iDfltImageListColorFlags)
	, m_htiA4AFSaveCpu()
	, m_htiAutoArch()
	, m_htiAutoTakeEd2kLinks()
	, m_htiBroadband()
	, m_htiBBLowIDDeboost()
	, m_htiBBLowIDDeboostDivisor()
	, m_htiBBLowRatioBoost()
	, m_htiBBLowRatioBonus()
	, m_htiBBLowRatioThreshold()
	, m_htiBBMaxUpClientsAllowed()
	, m_htiBBSessionMaxTime()
	, m_htiBBSessionMaxTimeMinutes()
	, m_htiBBSessionTransferLimit()
	, m_htiBBSessionTransAbsolute()
	, m_htiBBSessionTransAbsoluteValue()
	, m_htiBBSessionTransDisabled()
	, m_htiBBSessionTransPercent()
	, m_htiBBSessionTransPercentValue()
	, m_htiCheckDiskspace()
	, m_htiCloseUPnPPorts()
	, m_htiCommit()
	, m_htiCommitAlways()
	, m_htiCommitNever()
	, m_htiCommitOnShutdown()
	, m_htiConditionalTCPAccept()
	, m_htiCreditSystem()
	, m_htiDebug2Disk()
	, m_htiDebugSourceExchange()
	, m_htiExtControls()
	, m_htiHiddenDisplay()
	, m_htiHiddenFile()
	, m_htiHiddenSecurity()
	, m_htiHiddenStartup()
	, m_htiExtractMetaData()
	, m_htiExtractMetaDataID3Lib()
	, m_htiExtractMetaDataNever()
	, m_htiFilterLANIPs()
	, m_htiFullAlloc()
	, m_htiImportParts()
	, m_htiInspectAllFileTypes()
	, m_htiLog2Disk()
	, m_htiLogA4AF()
	, m_htiLogBannedClients()
	, m_htiLogFileSaving()
	, m_htiLogFilteredIPs()
	, m_htiLogLevel()
	, m_htiLogRatingDescReceived()
	, m_htiLogSecureIdent()
	, m_htiLogUlDlEvents()
	, m_htiMaxCon5Sec()
	, m_htiMaxHalfOpen()
	, m_htiTCPBigSendBuffer()
	, m_htiUDPReceiveBuffer()
	, m_htiMinFreeDiskSpace()
	, m_htiDateTimeFormat4Lists()
	, m_htiPreviewCopiedArchives()
	, m_htiPreviewOnIconDblClk()
	, m_htiShowActiveDownloadsBold()
	, m_htiUseSystemFontForMainControls()
	, m_htiReBarToolbar()
	, m_htiShowUpDownIconInTaskbar()
	, m_htiShowVerticalHourMarkers()
	, m_htiForceSpeedsToKB()
	, m_htiExtraPreviewWithMenu()
	, m_htiKeepUnavailableFixedSharedDirs()
	, m_htiPartiallyPurgeOldKnownFiles()
	, m_htiAdjustNTFSDaylightFileTime()
	, m_htiRearrangeKadSearchKeywords()
	, m_htiMessageFromValidSourcesOnly()
	, m_htiFileBufferTimeLimit()
	, m_htiResolveShellLinks()
	, m_htiRestoreLastLogPane()
	, m_htiRestoreLastMainWndDlg()
	, m_htiServerKeepAliveTimeout()
	, m_htiShareeMule()
	, m_htiShareeMuleMultiUser()
	, m_htiShareeMuleOldStyle()
	, m_htiShareeMulePublicUser()
	, m_htiSkipWANIPSetup()
	, m_htiSkipWANPPPSetup()
	, m_htiSparsePartFiles()
	, m_htiTCPGroup()
	, m_htiUPnP()
	, m_htiVerbose()
	, m_htiVerboseGroup()
	, m_htiYourHostname()
	, m_fBBLowRatioBonus()
	, m_fBBLowRatioThreshold()
	, m_fMinFreeDiskSpaceGB()
	, m_iQueueSize()
	, m_uTCPBigSendBufferSizeKiB()
	, m_uUDPReceiveBufferSizeKiB()
	, m_uBBSessionMaxTimeMinutes()
	, m_uBBSessionTransAbsoluteMiB()
	, m_uFileBufferSize()
	, m_uServerKeepAliveTimeout()
	, m_iBBLowIDDeboostDivisor()
	, m_iBBMaxUpClientsAllowed()
	, m_iBBSessionTransMode(BBSTM_DISABLED)
	, m_iBBSessionTransPercent()
	, m_iCommitFiles()
	, m_iExtractMetaData()
	, m_iInspectAllFileTypes()
	, m_iLogLevel()
	, m_iMaxConnPerFive()
	, m_iMaxHalfOpen()
	, m_iShareeMule()
	, m_sDateTimeFormat4Lists()
	, m_bA4AFSaveCpu()
	, m_bAdjustNTFSDaylightFileTime()
	, m_bAutoArchDisable(true)
	, m_bAutoTakeEd2kLinks()
	, m_bBBLowIDDeboost()
	, m_bBBLowRatioBoost()
	, m_bBBSessionMaxTime()
	, m_bCheckDiskspace()
	, m_bCloseUPnPOnExit(true)
	, m_bConditionalTCPAccept()
	, m_bCreditSystem()
	, m_bDebug2Disk()
	, m_bDebugSourceExchange()
	, m_bExtControls()
	, m_bExtraPreviewWithMenu()
	, m_bFilterLANIPs()
	, m_bFullAlloc()
	, m_bImportParts()
	, m_bInitializedTreeOpts()
	, m_bKeepUnavailableFixedSharedDirs()
	, m_bLog2Disk()
	, m_bLogA4AF()
	, m_bLogBannedClients()
	, m_bLogFileSaving()
	, m_bLogFilteredIPs()
	, m_bLogRatingDescReceived()
	, m_bLogSecureIdent()
	, m_bLogUlDlEvents()
	, m_bMessageFromValidSourcesOnly()
	, m_bPartiallyPurgeOldKnownFiles()
	, m_bPreviewCopiedArchives()
	, m_bPreviewOnIconDblClk()
	, m_bRearrangeKadSearchKeywords()
	, m_bReBarToolbar()
	, m_bRestoreLastLogPane()
	, m_bRestoreLastMainWndDlg()
	, m_bResolveShellLinks()
	, m_bShowActiveDownloadsBold()
	, m_bShowedWarning()
	, m_bShowUpDownIconInTaskbar()
	, m_bShowVerticalHourMarkers()
	, m_bSkipWANIPSetup()
	, m_bSkipWANPPPSetup()
	, m_bSparsePartFiles()
	, m_bUseSystemFontForMainControls()
	, m_bVerbose()
	, m_bForceSpeedsToKB()
	, m_uFileBufferTimeLimitSeconds()
{
}

void CPPgTweaks::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILEBUFFERSIZE, m_ctlFileBuffSize);
	DDX_Control(pDX, IDC_QUEUESIZE, m_ctlQueueSize);
	DDX_Control(pDX, IDC_EXT_OPTS, m_ctrlTreeOptions);
	if (!m_bInitializedTreeOpts) {
		int iImgBackup = 8; // default icon
		int iImgLog = 8;
		int iImgDynyp = 8;
		int iImgConnection = 8;
		//	int iImgA4AF = 8;
		int iImgMetaData = 8;
		int iImgUPnP = 8;
		int iImgShareeMule = 8;
		CImageList *piml = m_ctrlTreeOptions.GetImageList(TVSIL_NORMAL);
		if (piml) {
			iImgBackup = piml->Add(CTempIconLoader(_T("Harddisk")));
			iImgLog = piml->Add(CTempIconLoader(_T("Log")));
			iImgDynyp = piml->Add(CTempIconLoader(_T("upload")));
			iImgConnection = piml->Add(CTempIconLoader(_T("connection")));
			//	iImgA4AF = piml->Add(CTempIconLoader(_T("Download")));
			iImgMetaData = piml->Add(CTempIconLoader(_T("MediaInfo")));
			iImgUPnP = piml->Add(CTempIconLoader(_T("connectedhighhigh")));
			iImgShareeMule = piml->Add(CTempIconLoader(_T("viewfiles")));
		}

		/////////////////////////////////////////////////////////////////////////////
		// TCP/IP group
		//
		m_htiTCPGroup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_TCPIP_CONNS), iImgConnection, TVI_ROOT);
		m_htiMaxCon5Sec = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MAXCON5SECLABEL), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiMaxCon5Sec, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiMaxHalfOpen = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MAXHALFOPENCONS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiMaxHalfOpen, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiUDPReceiveBuffer = m_ctrlTreeOptions.InsertItem(GetResString(IDS_UDPRECEIVEBUFFERSIZE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiUDPReceiveBuffer, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiTCPBigSendBuffer = m_ctrlTreeOptions.InsertItem(GetResString(IDS_TCPBIGSENDBUFFERSIZE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiTCPBigSendBuffer, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiConditionalTCPAccept = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_CONDTCPACCEPT), m_htiTCPGroup, m_bConditionalTCPAccept);
		m_htiServerKeepAliveTimeout = m_ctrlTreeOptions.InsertItem(GetResString(IDS_SERVERKEEPALIVETIMEOUT), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiServerKeepAliveTimeout, RUNTIME_CLASS(CNumTreeOptionsEdit));

		/////////////////////////////////////////////////////////////////////////////
		// Miscellaneous group
		//
		m_htiAutoTakeEd2kLinks = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_AUTOTAKEED2KLINKS), TVI_ROOT, m_bAutoTakeEd2kLinks);
		m_htiCreditSystem = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_USECREDITSYSTEM), TVI_ROOT, m_bCreditSystem);
		m_htiFilterLANIPs = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_PW_FILTER), TVI_ROOT, m_bFilterLANIPs);
		m_htiExtControls = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWEXTSETTINGS), TVI_ROOT, m_bExtControls);
		m_htiA4AFSaveCpu = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_A4AF_SAVE_CPU), TVI_ROOT, m_bA4AFSaveCpu); // ZZ:DownloadManager
		m_htiAutoArch = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_DISABLE_AUTOARCHPREV), TVI_ROOT, m_bAutoArchDisable);
		m_htiYourHostname = m_ctrlTreeOptions.InsertItem(GetResString(IDS_YOURHOSTNAME), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, TVI_ROOT);
		m_ctrlTreeOptions.AddEditBox(m_htiYourHostname, RUNTIME_CLASS(CTreeOptionsEditEx));

		/////////////////////////////////////////////////////////////////////////////
		// Broadband group
		//
		m_htiBroadband = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_BROADBAND), iImgDynyp, TVI_ROOT);
		m_htiBBMaxUpClientsAllowed = m_ctrlTreeOptions.InsertItem(GetResString(IDS_BB_MAX_UPLOAD_CLIENTS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBroadband);
		m_ctrlTreeOptions.AddEditBox(m_htiBBMaxUpClientsAllowed, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiBBSessionTransferLimit = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_BB_SESSION_TRANSFER_LIMIT), iImgDynyp, m_htiBroadband);
		m_htiBBSessionTransDisabled = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_DISABLED), m_htiBBSessionTransferLimit, m_iBBSessionTransMode == BBSTM_DISABLED);
		m_htiBBSessionTransPercent = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_BB_PERCENT_OF_FILE_SIZE), m_htiBBSessionTransferLimit, m_iBBSessionTransMode == BBSTM_PERCENT);
		m_htiBBSessionTransAbsolute = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_BB_ABSOLUTE_LIMIT), m_htiBBSessionTransferLimit, m_iBBSessionTransMode == BBSTM_ABSOLUTE);
		m_htiBBSessionTransPercentValue = m_ctrlTreeOptions.InsertItem(GetResString(IDS_PERCENTAGE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBSessionTransferLimit);
		m_ctrlTreeOptions.AddEditBox(m_htiBBSessionTransPercentValue, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiBBSessionTransAbsoluteValue = m_ctrlTreeOptions.InsertItem(GetResString(IDS_BB_ABSOLUTE_LIMIT_MIB), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBSessionTransferLimit);
		m_ctrlTreeOptions.AddEditBox(m_htiBBSessionTransAbsoluteValue, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiBBSessionMaxTime = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_BB_ENABLE_SESSION_TIME_LIMIT), m_htiBroadband, m_bBBSessionMaxTime);
		m_htiBBSessionMaxTimeMinutes = m_ctrlTreeOptions.InsertItem(GetResString(IDS_LONGMINS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBSessionMaxTime);
		m_ctrlTreeOptions.AddEditBox(m_htiBBSessionMaxTimeMinutes, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiBBLowRatioBoost = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_BB_ENABLE_LOW_RATIO_BOOST), m_htiBroadband, m_bBBLowRatioBoost);
		m_htiBBLowRatioThreshold = m_ctrlTreeOptions.InsertItem(GetResString(IDS_BB_RATIO_THRESHOLD), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBLowRatioBoost);
		m_ctrlTreeOptions.AddEditBox(m_htiBBLowRatioThreshold, RUNTIME_CLASS(CTreeOptionsEditEx));
		m_htiBBLowRatioBonus = m_ctrlTreeOptions.InsertItem(GetResString(IDS_BB_SCORE_BONUS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBLowRatioBoost);
		m_ctrlTreeOptions.AddEditBox(m_htiBBLowRatioBonus, RUNTIME_CLASS(CTreeOptionsEditEx));
		m_htiBBLowIDDeboost = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_BB_DEBOOST_LOWIDS), m_htiBroadband, m_bBBLowIDDeboost);
		m_htiBBLowIDDeboostDivisor = m_ctrlTreeOptions.InsertItem(GetResString(IDS_BB_DIVISOR), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiBBLowIDDeboost);
		m_ctrlTreeOptions.AddEditBox(m_htiBBLowIDDeboostDivisor, RUNTIME_CLASS(CNumTreeOptionsEdit));

		/////////////////////////////////////////////////////////////////////////////
		// File related group
		//
		m_htiSparsePartFiles = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SPARSEPARTFILES), TVI_ROOT, m_bSparsePartFiles);
		m_htiFullAlloc = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_FULLALLOC), TVI_ROOT, m_bFullAlloc);
		m_htiCheckDiskspace = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_CHECKDISKSPACE), TVI_ROOT, m_bCheckDiskspace);
		m_htiMinFreeDiskSpace = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MINFREEDISKSPACE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiCheckDiskspace);
		m_ctrlTreeOptions.AddEditBox(m_htiMinFreeDiskSpace, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiCommit = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_COMMITFILES), iImgBackup, TVI_ROOT);
		m_htiCommitNever = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_NEVER), m_htiCommit, m_iCommitFiles == 0);
		m_htiCommitOnShutdown = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_ONSHUTDOWN), m_htiCommit, m_iCommitFiles == 1);
		m_htiCommitAlways = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_ALWAYS), m_htiCommit, m_iCommitFiles == 2);
		m_htiExtractMetaData = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_EXTRACT_META_DATA), iImgMetaData, TVI_ROOT);
		m_htiExtractMetaDataNever = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_NEVER), m_htiExtractMetaData, m_iExtractMetaData == 0);
		m_htiExtractMetaDataID3Lib = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_META_DATA_ID3LIB), m_htiExtractMetaData, m_iExtractMetaData == 1);
		//m_htiExtractMetaDataMediaDet = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_META_DATA_MEDIADET), m_htiExtractMetaData, m_iExtractMetaData == 2);
		m_htiResolveShellLinks = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_RESOLVELINKS), TVI_ROOT, m_bResolveShellLinks);

		/////////////////////////////////////////////////////////////////////////////
		// Logging group
		//
		m_htiLog2Disk = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG2DISK), TVI_ROOT, m_bLog2Disk);
		if (thePrefs.GetEnableVerboseOptions()) {
			m_htiVerboseGroup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_VERBOSE), iImgLog, TVI_ROOT);
			m_htiVerbose = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_ENABLED), m_htiVerboseGroup, m_bVerbose);
			m_htiLogLevel = m_ctrlTreeOptions.InsertItem(GetResString(IDS_LOG_LEVEL), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiVerboseGroup);
			m_ctrlTreeOptions.AddEditBox(m_htiLogLevel, RUNTIME_CLASS(CNumTreeOptionsEdit));
			m_htiDebug2Disk = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG2DISK), m_htiVerboseGroup, m_bDebug2Disk);
			m_htiDebugSourceExchange = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_DEBUG_SOURCE_EXCHANGE), m_htiVerboseGroup, m_bDebugSourceExchange);
			m_htiLogBannedClients = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_BANNED_CLIENTS), m_htiVerboseGroup, m_bLogBannedClients);
			m_htiLogRatingDescReceived = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_RATING_RECV), m_htiVerboseGroup, m_bLogRatingDescReceived);
			m_htiLogSecureIdent = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_SECURE_IDENT), m_htiVerboseGroup, m_bLogSecureIdent);
			m_htiLogFilteredIPs = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_FILTERED_IPS), m_htiVerboseGroup, m_bLogFilteredIPs);
			m_htiLogFileSaving = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_FILE_SAVING), m_htiVerboseGroup, m_bLogFileSaving);
			m_htiLogA4AF = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_A4AF), m_htiVerboseGroup, m_bLogA4AF); // ZZ:DownloadManager
			m_htiLogUlDlEvents = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_ULDL_EVENTS), m_htiVerboseGroup, m_bLogUlDlEvents);
		}

		/////////////////////////////////////////////////////////////////////////////
		// UPnP group
		//
		m_htiUPnP = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_UPNP), iImgUPnP, TVI_ROOT);
		m_htiCloseUPnPPorts = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPCLOSEONEXIT), m_htiUPnP, m_bCloseUPnPOnExit);
		m_htiSkipWANIPSetup = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPSKIPWANIP), m_htiUPnP, m_bSkipWANIPSetup);
		m_htiSkipWANPPPSetup = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPSKIPWANPPP), m_htiUPnP, m_bSkipWANPPPSetup);

		/////////////////////////////////////////////////////////////////////////////
		// eMule Shared Use
		//
		m_htiShareeMule = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_SHAREEMULELABEL), iImgShareeMule, TVI_ROOT);
		m_htiShareeMuleMultiUser = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEMULTI), m_htiShareeMule, m_iShareeMule == 0);
		m_htiShareeMulePublicUser = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEPUBLIC), m_htiShareeMule, m_iShareeMule == 1);
		m_htiShareeMuleOldStyle = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEOLD), m_htiShareeMule, m_iShareeMule == 2);

		m_htiImportParts = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_ENABLEIMPORTPARTS), TVI_ROOT, m_bImportParts);

		/////////////////////////////////////////////////////////////////////////////
		// Hidden runtime groups
		//
		m_htiHiddenStartup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_HIDDENRUNTIME_STARTUP), iImgConnection, TVI_ROOT);
		m_htiRestoreLastMainWndDlg = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_RESTORELASTMAINWNDDLG), m_htiHiddenStartup, m_bRestoreLastMainWndDlg);
		m_htiRestoreLastLogPane = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_RESTORELASTLOGPANE), m_htiHiddenStartup, m_bRestoreLastLogPane);

		m_htiHiddenFile = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_HIDDENRUNTIME_FILE), iImgBackup, TVI_ROOT);
		m_htiFileBufferTimeLimit = m_ctrlTreeOptions.InsertItem(GetResString(IDS_FILEBUFFERTIMELIMIT), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiHiddenFile);
		m_ctrlTreeOptions.AddEditBox(m_htiFileBufferTimeLimit, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDateTimeFormat4Lists = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DATETIMEFORMAT4LISTS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiHiddenFile);
		m_ctrlTreeOptions.AddEditBox(m_htiDateTimeFormat4Lists, RUNTIME_CLASS(CTreeOptionsEditEx));
		m_htiPreviewCopiedArchives = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_PREVIEWCOPIEDARCHIVES), m_htiHiddenFile, m_bPreviewCopiedArchives);
		m_htiInspectAllFileTypes = m_ctrlTreeOptions.InsertItem(GetResString(IDS_INSPECTALLFILETYPES), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiHiddenFile);
		m_ctrlTreeOptions.AddEditBox(m_htiInspectAllFileTypes, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiPreviewOnIconDblClk = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_PREVIEWONICONDBLCLK), m_htiHiddenFile, m_bPreviewOnIconDblClk);
		m_htiExtraPreviewWithMenu = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_EXTRAPREVIEWWITHMENU), m_htiHiddenFile, m_bExtraPreviewWithMenu);
		m_htiKeepUnavailableFixedSharedDirs = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_KEEPUNAVAILABLEFIXEDSHAREDDIRS), m_htiHiddenFile, m_bKeepUnavailableFixedSharedDirs);
		m_htiPartiallyPurgeOldKnownFiles = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_PARTIALLYPURGEOLDKNOWNFILES), m_htiHiddenFile, m_bPartiallyPurgeOldKnownFiles);
		m_htiAdjustNTFSDaylightFileTime = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_ADJUSTNTFSDAYLIGHTFILETIME), m_htiHiddenFile, m_bAdjustNTFSDaylightFileTime);

		m_htiHiddenDisplay = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_HIDDENRUNTIME_DISPLAY), iImgLog, TVI_ROOT);
		m_htiShowActiveDownloadsBold = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWACTIVEDOWNLOADSBOLD), m_htiHiddenDisplay, m_bShowActiveDownloadsBold);
		m_htiUseSystemFontForMainControls = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_USESYSTEMFONTFORMAINCONTROLS), m_htiHiddenDisplay, m_bUseSystemFontForMainControls);
		m_htiReBarToolbar = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_REBARTOOLBAR), m_htiHiddenDisplay, m_bReBarToolbar);
		m_htiShowUpDownIconInTaskbar = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWUPDOWNICONINTASKBAR), m_htiHiddenDisplay, m_bShowUpDownIconInTaskbar);
		m_htiShowVerticalHourMarkers = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWVERTICALHOURMARKERS), m_htiHiddenDisplay, m_bShowVerticalHourMarkers);
		m_htiForceSpeedsToKB = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_FORCESPEEDSTOKB), m_htiHiddenDisplay, m_bForceSpeedsToKB);

		m_htiHiddenSecurity = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_HIDDENRUNTIME_SECURITY), iImgConnection, TVI_ROOT);
		m_htiRearrangeKadSearchKeywords = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_REARRANGEKADSEARCHKEYWORDS), m_htiHiddenSecurity, m_bRearrangeKadSearchKeywords);
		m_htiMessageFromValidSourcesOnly = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_MESSAGEFROMVALIDSOURCESONLY), m_htiHiddenSecurity, m_bMessageFromValidSourcesOnly);

		m_ctrlTreeOptions.Expand(m_htiTCPGroup, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiBroadband, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiBBSessionTransferLimit, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiBBSessionMaxTime, m_bBBSessionMaxTime ? TVE_EXPAND : TVE_COLLAPSE);
		m_ctrlTreeOptions.Expand(m_htiBBLowRatioBoost, m_bBBLowRatioBoost ? TVE_EXPAND : TVE_COLLAPSE);
		m_ctrlTreeOptions.Expand(m_htiBBLowIDDeboost, m_bBBLowIDDeboost ? TVE_EXPAND : TVE_COLLAPSE);
		if (m_htiVerboseGroup)
			m_ctrlTreeOptions.Expand(m_htiVerboseGroup, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiCommit, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiCheckDiskspace, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiExtractMetaData, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiHiddenStartup, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiHiddenFile, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiHiddenDisplay, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiHiddenSecurity, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiUPnP, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiShareeMule, TVE_EXPAND);
		m_ctrlTreeOptions.SendMessage(WM_VSCROLL, SB_TOP);
		m_bInitializedTreeOpts = true;
	}

	/////////////////////////////////////////////////////////////////////////////
	// TCP/IP group
	//
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiMaxCon5Sec, m_iMaxConnPerFive);
	DDV_MinMaxInt(pDX, m_iMaxConnPerFive, 1, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiMaxHalfOpen, m_iMaxHalfOpen);
	DDV_MinMaxInt(pDX, m_iMaxHalfOpen, 1, INT_MAX);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiUDPReceiveBuffer, m_uUDPReceiveBufferSizeKiB);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiTCPBigSendBuffer, m_uTCPBigSendBufferSizeKiB);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiConditionalTCPAccept, m_bConditionalTCPAccept);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiServerKeepAliveTimeout, m_uServerKeepAliveTimeout);
	if (pDX->m_bSaveAndValidate) {
		if (m_uUDPReceiveBufferSizeKiB < 64)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiUDPReceiveBuffer);
		if (m_uTCPBigSendBufferSizeKiB < 64)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiTCPBigSendBuffer);
	}

	/////////////////////////////////////////////////////////////////////////////
	// Miscellaneous group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiAutoTakeEd2kLinks, m_bAutoTakeEd2kLinks);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCreditSystem, m_bCreditSystem);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiFilterLANIPs, m_bFilterLANIPs);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiExtControls, m_bExtControls);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiA4AFSaveCpu, m_bA4AFSaveCpu);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiYourHostname, m_sYourHostname);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiAutoArch, m_bAutoArchDisable);

	/////////////////////////////////////////////////////////////////////////////
	// Broadband group
	//
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiBBMaxUpClientsAllowed, m_iBBMaxUpClientsAllowed);
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiBBSessionTransferLimit, m_iBBSessionTransMode);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBSessionTransPercentValue, m_iBBSessionTransPercent);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBSessionTransAbsoluteValue, m_uBBSessionTransAbsoluteMiB);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiBBSessionMaxTime, m_bBBSessionMaxTime);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBSessionMaxTimeMinutes, m_uBBSessionMaxTimeMinutes);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiBBLowRatioBoost, m_bBBLowRatioBoost);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBLowRatioThreshold, m_fBBLowRatioThreshold);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBLowRatioBonus, m_fBBLowRatioBonus);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiBBLowIDDeboost, m_bBBLowIDDeboost);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiBBLowIDDeboostDivisor, m_iBBLowIDDeboostDivisor);
	if (pDX->m_bSaveAndValidate) {
		if (m_iBBMaxUpClientsAllowed < MIN_UP_CLIENTS_ALLOWED || m_iBBMaxUpClientsAllowed > MAX_UP_CLIENTS_ALLOWED)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiBBMaxUpClientsAllowed);
		if (m_iBBSessionTransMode == BBSTM_PERCENT && (m_iBBSessionTransPercent < 1 || m_iBBSessionTransPercent > 100))
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiBBSessionTransPercentValue);
		if (m_iBBSessionTransMode == BBSTM_ABSOLUTE && m_uBBSessionTransAbsoluteMiB < 1)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiBBSessionTransAbsoluteValue);
		if (m_bBBSessionMaxTime && m_uBBSessionMaxTimeMinutes < 1)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiBBSessionMaxTimeMinutes);
		if (m_bBBLowRatioBoost && m_fBBLowRatioThreshold <= 0.0f)
			FailTreeValidation(pDX, AFX_IDP_PARSE_REAL, m_htiBBLowRatioThreshold);
		if (m_bBBLowRatioBoost && m_fBBLowRatioBonus <= 0.0f)
			FailTreeValidation(pDX, AFX_IDP_PARSE_REAL, m_htiBBLowRatioBonus);
		if (m_bBBLowIDDeboost && m_iBBLowIDDeboostDivisor < 2)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiBBLowIDDeboostDivisor);
	}

	/////////////////////////////////////////////////////////////////////////////
	// File related group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSparsePartFiles, m_bSparsePartFiles);
	m_ctrlTreeOptions.SetCheckBoxEnable(m_htiSparsePartFiles, TRUE);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiImportParts, m_bImportParts);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiFullAlloc, m_bFullAlloc);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCheckDiskspace, m_bCheckDiskspace);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiMinFreeDiskSpace, m_fMinFreeDiskSpaceGB);
	DDV_MinMaxFloat(pDX, m_fMinFreeDiskSpaceGB, 0.0, _UI32_MAX / (1024.0f * 1024.0f * 1024.0f));
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiCommit, m_iCommitFiles);
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiExtractMetaData, m_iExtractMetaData);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiResolveShellLinks, m_bResolveShellLinks);

	/////////////////////////////////////////////////////////////////////////////
	// Hidden runtime groups
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiRestoreLastMainWndDlg, m_bRestoreLastMainWndDlg);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiRestoreLastLogPane, m_bRestoreLastLogPane);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiFileBufferTimeLimit, m_uFileBufferTimeLimitSeconds);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDateTimeFormat4Lists, m_sDateTimeFormat4Lists);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiPreviewCopiedArchives, m_bPreviewCopiedArchives);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiInspectAllFileTypes, m_iInspectAllFileTypes);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiPreviewOnIconDblClk, m_bPreviewOnIconDblClk);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiExtraPreviewWithMenu, m_bExtraPreviewWithMenu);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiKeepUnavailableFixedSharedDirs, m_bKeepUnavailableFixedSharedDirs);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiPartiallyPurgeOldKnownFiles, m_bPartiallyPurgeOldKnownFiles);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiAdjustNTFSDaylightFileTime, m_bAdjustNTFSDaylightFileTime);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiShowActiveDownloadsBold, m_bShowActiveDownloadsBold);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiUseSystemFontForMainControls, m_bUseSystemFontForMainControls);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiReBarToolbar, m_bReBarToolbar);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiShowUpDownIconInTaskbar, m_bShowUpDownIconInTaskbar);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiShowVerticalHourMarkers, m_bShowVerticalHourMarkers);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiForceSpeedsToKB, m_bForceSpeedsToKB);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiRearrangeKadSearchKeywords, m_bRearrangeKadSearchKeywords);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiMessageFromValidSourcesOnly, m_bMessageFromValidSourcesOnly);
	if (pDX->m_bSaveAndValidate) {
		if (m_uFileBufferTimeLimitSeconds < 1)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiFileBufferTimeLimit);
		if (m_iInspectAllFileTypes < 0)
			FailTreeValidation(pDX, AFX_IDP_PARSE_INT, m_htiInspectAllFileTypes);
	}

	/////////////////////////////////////////////////////////////////////////////
	// Logging group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLog2Disk, m_bLog2Disk);
	if (m_htiLogLevel) {
		DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiLogLevel, m_iLogLevel);
		DDV_MinMaxInt(pDX, m_iLogLevel, 1, 5);
	}
	if (m_htiVerbose)
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiVerbose, m_bVerbose);
	if (m_htiDebug2Disk) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiDebug2Disk, m_bDebug2Disk);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebug2Disk, m_bVerbose);
	}
	if (m_htiDebugSourceExchange) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiDebugSourceExchange, m_bDebugSourceExchange);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebugSourceExchange, m_bVerbose);
	}
	if (m_htiLogBannedClients) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogBannedClients, m_bLogBannedClients);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogBannedClients, m_bVerbose);
	}
	if (m_htiLogRatingDescReceived) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogRatingDescReceived, m_bLogRatingDescReceived);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogRatingDescReceived, m_bVerbose);
	}
	if (m_htiLogSecureIdent) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogSecureIdent, m_bLogSecureIdent);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogSecureIdent, m_bVerbose);
	}
	if (m_htiLogFilteredIPs) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogFilteredIPs, m_bLogFilteredIPs);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFilteredIPs, m_bVerbose);
	}
	if (m_htiLogFileSaving) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogFileSaving, m_bLogFileSaving);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFileSaving, m_bVerbose);
	}
	if (m_htiLogA4AF) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogA4AF, m_bLogA4AF);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogA4AF, m_bVerbose);
	}
	if (m_htiLogUlDlEvents) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogUlDlEvents, m_bLogUlDlEvents);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogUlDlEvents, m_bVerbose);
	}

	/////////////////////////////////////////////////////////////////////////////
	// UPnP group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCloseUPnPPorts, m_bCloseUPnPOnExit);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSkipWANIPSetup, m_bSkipWANIPSetup);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSkipWANPPPSetup, m_bSkipWANPPPSetup);

	/////////////////////////////////////////////////////////////////////////////
	// eMule Shared User
	//
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiShareeMule, m_iShareeMule);
	m_ctrlTreeOptions.SetRadioButtonEnable(m_htiShareeMulePublicUser, TRUE);
}

BOOL CPPgTweaks::OnInitDialog()
{
	m_iMaxConnPerFive = thePrefs.GetMaxConperFive();
	m_iMaxHalfOpen = thePrefs.GetMaxHalfConnections();
	m_uUDPReceiveBufferSizeKiB = max(64u, thePrefs.GetUDPReceiveBufferSize() / 1024u);
	m_uTCPBigSendBufferSizeKiB = max(64u, thePrefs.GetBigSendBufferSize() / 1024u);
	m_bConditionalTCPAccept = thePrefs.GetConditionalTCPAccept();
	m_bAutoTakeEd2kLinks = thePrefs.AutoTakeED2KLinks();
	if (thePrefs.GetEnableVerboseOptions()) {
		m_bVerbose = thePrefs.m_bVerbose;
		m_bDebug2Disk = thePrefs.debug2disk;							// do *not* use the corresponding 'Get...' function here!
		m_bDebugSourceExchange = thePrefs.m_bDebugSourceExchange;		// do *not* use the corresponding 'Get...' function here!
		m_bLogBannedClients = thePrefs.m_bLogBannedClients;				// do *not* use the corresponding 'Get...' function here!
		m_bLogRatingDescReceived = thePrefs.m_bLogRatingDescReceived;	// do *not* use the corresponding 'Get...' function here!
		m_bLogSecureIdent = thePrefs.m_bLogSecureIdent;					// do *not* use the corresponding 'Get...' function here!
		m_bLogFilteredIPs = thePrefs.m_bLogFilteredIPs;					// do *not* use the corresponding 'Get...' function here!
		m_bLogFileSaving = thePrefs.m_bLogFileSaving;					// do *not* use the corresponding 'Get...' function here!
		m_bLogA4AF = thePrefs.m_bLogA4AF;							    // do *not* use the corresponding 'Get...' function here! // ZZ:DownloadManager
		m_bLogUlDlEvents = thePrefs.m_bLogUlDlEvents;
		m_iLogLevel = 5 - thePrefs.m_byLogLevel;
	}
	m_bLog2Disk = thePrefs.log2disk;
	m_bCreditSystem = thePrefs.m_bCreditSystem;
	m_iCommitFiles = thePrefs.m_iCommitFiles;
	m_iExtractMetaData = thePrefs.m_iExtractMetaData;
	m_bFilterLANIPs = thePrefs.filterLANIPs;
	m_bExtControls = thePrefs.m_bExtControls;
	m_uServerKeepAliveTimeout = thePrefs.m_dwServerKeepAliveTimeout / MIN2MS(1);
	m_bSparsePartFiles = thePrefs.GetSparsePartFiles();
	m_bImportParts = thePrefs.m_bImportParts;
	m_bFullAlloc = thePrefs.m_bAllocFull;
	m_bCheckDiskspace = thePrefs.checkDiskspace;
	m_bResolveShellLinks = thePrefs.GetResolveSharedShellLinks();
	m_fMinFreeDiskSpaceGB = (float)(thePrefs.m_uMinFreeDiskSpace / (1024.0 * 1024.0 * 1024.0));
	m_sYourHostname = thePrefs.GetYourHostname();
	m_bAutoArchDisable = !thePrefs.m_bAutomaticArcPreviewStart;
	m_iBBMaxUpClientsAllowed = static_cast<int>(thePrefs.GetMaxUpClientsAllowed());
	const uint64 uBBSessionMaxTrans = thePrefs.GetBBSessionMaxTrans();
	if (uBBSessionMaxTrans == 0) {
		m_iBBSessionTransMode = BBSTM_DISABLED;
		m_iBBSessionTransPercent = 33;
		m_uBBSessionTransAbsoluteMiB = 256;
	} else if (uBBSessionMaxTrans <= 100) {
		m_iBBSessionTransMode = BBSTM_PERCENT;
		m_iBBSessionTransPercent = static_cast<int>(uBBSessionMaxTrans);
		m_uBBSessionTransAbsoluteMiB = 256;
	} else {
		m_iBBSessionTransMode = BBSTM_ABSOLUTE;
		m_iBBSessionTransPercent = 33;
		uint64 uAbsoluteMiB = uBBSessionMaxTrans / (1024ui64 * 1024ui64);
		if (uAbsoluteMiB < 1)
			uAbsoluteMiB = 1;
		m_uBBSessionTransAbsoluteMiB = static_cast<UINT>(uAbsoluteMiB);
	}
	const uint64 uBBSessionMaxTime = thePrefs.GetBBSessionMaxTime();
	m_bBBSessionMaxTime = (uBBSessionMaxTime != 0);
	if (m_bBBSessionMaxTime) {
		uint64 uMinutes = (uBBSessionMaxTime + MIN2MS(1) - 1) / MIN2MS(1);
		if (uMinutes < 1)
			uMinutes = 1;
		m_uBBSessionMaxTimeMinutes = static_cast<UINT>(uMinutes);
	} else {
		m_uBBSessionMaxTimeMinutes = 180;
	}
	m_fBBLowRatioThreshold = thePrefs.GetBBBoostLowRatioFiles() > 0.0f ? thePrefs.GetBBBoostLowRatioFiles() : 0.5f;
	m_fBBLowRatioBonus = thePrefs.GetBBBoostLowRatioFilesBy() > 0.0f ? thePrefs.GetBBBoostLowRatioFilesBy() : 50.0f;
	m_bBBLowRatioBoost = thePrefs.GetBBBoostLowRatioFiles() > 0.0f && thePrefs.GetBBBoostLowRatioFilesBy() > 0.0f;
	m_iBBLowIDDeboostDivisor = thePrefs.GetBBDeboostLowIDs() > 1 ? static_cast<int>(thePrefs.GetBBDeboostLowIDs()) : 4;
	m_bBBLowIDDeboost = thePrefs.GetBBDeboostLowIDs() > 1;

	m_bCloseUPnPOnExit = thePrefs.CloseUPnPOnExit();
	m_bSkipWANIPSetup = thePrefs.GetSkipWANIPSetup();
	m_bSkipWANPPPSetup = thePrefs.GetSkipWANPPPSetup();

	m_iShareeMule = thePrefs.m_nCurrentUserDirMode;

	m_bA4AFSaveCpu = thePrefs.GetA4AFSaveCpu();
	m_bRestoreLastMainWndDlg = thePrefs.GetRestoreLastMainWndDlg();
	m_bRestoreLastLogPane = thePrefs.GetRestoreLastLogPane();
	m_uFileBufferTimeLimitSeconds = max(1u, thePrefs.GetFileBufferTimeLimit() / SEC2MS(1));
	m_sDateTimeFormat4Lists = thePrefs.GetDateTimeFormat4Lists();
	m_bPreviewCopiedArchives = thePrefs.GetPreviewCopiedArchives();
	m_iInspectAllFileTypes = thePrefs.GetInspectAllFileTypes();
	m_bPreviewOnIconDblClk = thePrefs.GetPreviewOnIconDblClk();
	m_bShowActiveDownloadsBold = thePrefs.GetShowActiveDownloadsBold();
	m_bUseSystemFontForMainControls = thePrefs.GetUseSystemFontForMainControls();
	m_bReBarToolbar = thePrefs.GetReBarToolbar();
	m_bShowUpDownIconInTaskbar = thePrefs.IsShowUpDownIconInTaskbar();
	m_bShowVerticalHourMarkers = thePrefs.m_bShowVerticalHourMarkers;
	m_bForceSpeedsToKB = thePrefs.GetForceSpeedsToKB();
	m_bExtraPreviewWithMenu = thePrefs.GetExtraPreviewWithMenu();
	m_bKeepUnavailableFixedSharedDirs = thePrefs.m_bKeepUnavailableFixedSharedDirs;
	m_bPartiallyPurgeOldKnownFiles = thePrefs.DoPartiallyPurgeOldKnownFiles();
	m_bAdjustNTFSDaylightFileTime = thePrefs.GetAdjustNTFSDaylightFileTime();
	m_bRearrangeKadSearchKeywords = thePrefs.GetRearrangeKadSearchKeywords();
	m_bMessageFromValidSourcesOnly = thePrefs.MsgOnlySecure();

	m_ctrlTreeOptions.SetImageListColorFlags(theApp.m_iDfltImageListColorFlags);
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);
	m_ctrlTreeOptions.SetItemHeight(m_ctrlTreeOptions.GetItemHeight() + 2);

	m_uFileBufferSize = thePrefs.m_uFileBufferSize;
	m_ctlFileBuffSize.SetRange(16, 4096, TRUE);
	int iMin, iMax;
	m_ctlFileBuffSize.GetRange(iMin, iMax);
	m_ctlFileBuffSize.SetPos(m_uFileBufferSize / 1024);
	int iPage = 128;
	for (int i = ((iMin + iPage - 1) / iPage) * iPage; i < iMax; i += iPage)
		m_ctlFileBuffSize.SetTic(i);
	m_ctlFileBuffSize.SetPageSize(iPage);

	m_iQueueSize = thePrefs.m_iQueueSize;
	m_ctlQueueSize.SetRange(20, 100, TRUE);
	m_ctlQueueSize.SetPos((int)(m_iQueueSize / 100));
	m_ctlQueueSize.SetTicFreq(10);
	m_ctlQueueSize.SetPageSize(10);

	Localize();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPgTweaks::OnKillActive()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();
	return CPropertyPage::OnKillActive();
}

BOOL CPPgTweaks::OnApply()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();

	if (!UpdateData())
		return FALSE;

	thePrefs.SetMaxConsPerFive(m_iMaxConnPerFive ? m_iMaxConnPerFive : DFLT_MAXCONPERFIVE);
	theApp.scheduler->original_cons5s = thePrefs.GetMaxConperFive();
	thePrefs.SetMaxHalfConnections(m_iMaxHalfOpen ? m_iMaxHalfOpen : DFLT_MAXHALFOPEN);
	thePrefs.m_uUDPReceiveBufferSize = m_uUDPReceiveBufferSizeKiB * 1024u;
	thePrefs.m_uTCPSendBufferSize = m_uTCPBigSendBufferSizeKiB * 1024u;
	thePrefs.m_bConditionalTCPAccept = m_bConditionalTCPAccept;
	if (theApp.clientudp != NULL)
		theApp.clientudp->ApplyReceiveBufferSize();

	if (thePrefs.AutoTakeED2KLinks() != m_bAutoTakeEd2kLinks) {
		thePrefs.autotakeed2klinks = m_bAutoTakeEd2kLinks;
		if (thePrefs.AutoTakeED2KLinks())
			Ask4RegFix(false, true, false);
		else
			RevertReg();
	}

	if (!thePrefs.log2disk && m_bLog2Disk)
		theLog.Open();
	else if (thePrefs.log2disk && !m_bLog2Disk)
		theLog.Close();
	thePrefs.log2disk = m_bLog2Disk;

	if (thePrefs.GetEnableVerboseOptions()) {
		if (!thePrefs.GetDebug2Disk() && m_bVerbose && m_bDebug2Disk)
			theVerboseLog.Open();
		else if (thePrefs.GetDebug2Disk() && (!m_bVerbose || !m_bDebug2Disk))
			theVerboseLog.Close();
		thePrefs.debug2disk = m_bDebug2Disk;

		thePrefs.m_bDebugSourceExchange = m_bDebugSourceExchange;
		thePrefs.m_bLogBannedClients = m_bLogBannedClients;
		thePrefs.m_bLogRatingDescReceived = m_bLogRatingDescReceived;
		thePrefs.m_bLogSecureIdent = m_bLogSecureIdent;
		thePrefs.m_bLogFilteredIPs = m_bLogFilteredIPs;
		thePrefs.m_bLogFileSaving = m_bLogFileSaving;
		thePrefs.m_bLogA4AF = m_bLogA4AF;
		thePrefs.m_bLogUlDlEvents = m_bLogUlDlEvents;
		thePrefs.m_byLogLevel = 5 - m_iLogLevel;

		thePrefs.m_bVerbose = m_bVerbose; // store after related options were stored!
	}

	thePrefs.m_bCreditSystem = m_bCreditSystem;
	thePrefs.m_iCommitFiles = m_iCommitFiles;
	thePrefs.m_iExtractMetaData = m_iExtractMetaData;
	thePrefs.filterLANIPs = m_bFilterLANIPs;
	thePrefs.m_uFileBufferSize = m_uFileBufferSize;
	thePrefs.m_iQueueSize = m_iQueueSize;

	bool bUpdateDLmenu = (thePrefs.m_bImportParts != m_bImportParts);
	thePrefs.m_bImportParts = m_bImportParts;
	if (thePrefs.m_bExtControls != m_bExtControls) {
		bUpdateDLmenu = true;
		thePrefs.m_bExtControls = m_bExtControls;
		theApp.emuledlg->searchwnd->CreateMenus();
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.CreateMenus();
	}
	if (bUpdateDLmenu)
		theApp.emuledlg->transferwnd->GetDownloadList()->CreateMenus();

	thePrefs.m_dwServerKeepAliveTimeout = MIN2MS(m_uServerKeepAliveTimeout);
	thePrefs.m_maxUpClientsAllowed = static_cast<uint32>(max(MIN_UP_CLIENTS_ALLOWED, min(MAX_UP_CLIENTS_ALLOWED, m_iBBMaxUpClientsAllowed)));
	switch (m_iBBSessionTransMode) {
		case BBSTM_DISABLED:
			thePrefs.m_bbSessionMaxTrans = 0;
			break;
		case BBSTM_PERCENT:
			thePrefs.m_bbSessionMaxTrans = static_cast<uint64>(m_iBBSessionTransPercent);
			break;
		default:
			thePrefs.m_bbSessionMaxTrans = static_cast<uint64>(m_uBBSessionTransAbsoluteMiB) * 1024ui64 * 1024ui64;
			break;
	}
	thePrefs.m_bbSessionMaxTime = m_bBBSessionMaxTime ? static_cast<uint64>(m_uBBSessionMaxTimeMinutes) * MIN2MS(1) : 0;
	thePrefs.m_bbBoostLowRatioFiles = m_bBBLowRatioBoost ? max(0.0f, m_fBBLowRatioThreshold) : 0.0f;
	thePrefs.m_bbBoostLowRatioFilesBy = m_bBBLowRatioBoost ? max(0.0f, m_fBBLowRatioBonus) : 0.0f;
	thePrefs.m_bbDeboostLowIDs = m_bBBLowIDDeboost ? static_cast<uint32>(max(2, m_iBBLowIDDeboostDivisor)) : 0;
	thePrefs.m_bSparsePartFiles = m_bSparsePartFiles;
	thePrefs.m_bAllocFull = m_bFullAlloc;
	thePrefs.checkDiskspace = m_bCheckDiskspace;
	thePrefs.m_bResolveSharedShellLinks = m_bResolveShellLinks;
	thePrefs.m_uMinFreeDiskSpace = (UINT)(m_fMinFreeDiskSpaceGB * (1024 * 1024 * 1024));
	if (thePrefs.GetYourHostname() != m_sYourHostname) {
		thePrefs.SetYourHostname(m_sYourHostname);
		theApp.emuledlg->serverwnd->UpdateMyInfo();
	}
	thePrefs.m_bAutomaticArcPreviewStart = !m_bAutoArchDisable;

	thePrefs.m_bCloseUPnPOnExit = m_bCloseUPnPOnExit;
	thePrefs.SetSkipWANIPSetup(m_bSkipWANIPSetup);
	thePrefs.SetSkipWANPPPSetup(m_bSkipWANPPPSetup);

	thePrefs.ChangeUserDirMode(m_iShareeMule);

	thePrefs.m_bA4AFSaveCpu = m_bA4AFSaveCpu;
	thePrefs.m_bRestoreLastMainWndDlg = m_bRestoreLastMainWndDlg;
	thePrefs.m_bRestoreLastLogPane = m_bRestoreLastLogPane;
	thePrefs.m_uFileBufferTimeLimit = SEC2MS(m_uFileBufferTimeLimitSeconds);
	thePrefs.m_strDateTimeFormat4Lists = m_sDateTimeFormat4Lists;
	thePrefs.m_bPreviewCopiedArchives = m_bPreviewCopiedArchives;
	thePrefs.m_iInspectAllFileTypes = m_iInspectAllFileTypes;
	thePrefs.m_bPreviewOnIconDblClk = m_bPreviewOnIconDblClk;
	thePrefs.m_bShowActiveDownloadsBold = m_bShowActiveDownloadsBold;
	thePrefs.m_bUseSystemFontForMainControls = m_bUseSystemFontForMainControls;
	thePrefs.m_bReBarToolbar = m_bReBarToolbar;
	thePrefs.m_bShowUpDownIconInTaskbar = m_bShowUpDownIconInTaskbar;
	thePrefs.m_bShowVerticalHourMarkers = m_bShowVerticalHourMarkers;
	thePrefs.m_bForceSpeedsToKB = m_bForceSpeedsToKB;
	thePrefs.m_bExtraPreviewWithMenu = m_bExtraPreviewWithMenu;
	thePrefs.m_bKeepUnavailableFixedSharedDirs = m_bKeepUnavailableFixedSharedDirs;
	thePrefs.m_bPartiallyPurgeOldKnownFiles = m_bPartiallyPurgeOldKnownFiles;
	thePrefs.m_bAdjustNTFSDaylightFileTime = m_bAdjustNTFSDaylightFileTime;
	thePrefs.m_bRearrangeKadSearchKeywords = m_bRearrangeKadSearchKeywords;
	thePrefs.msgsecure = m_bMessageFromValidSourcesOnly;

	if (thePrefs.GetEnableVerboseOptions()) {
		theApp.emuledlg->serverwnd->ToggleDebugWindow();
		theApp.emuledlg->serverwnd->UpdateLogTabSelection();
	}
	theApp.downloadqueue->CheckDiskspace();

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgTweaks::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pScrollBar)
{
	if (pScrollBar->GetSafeHwnd() == m_ctlFileBuffSize.m_hWnd) {
		m_uFileBufferSize = m_ctlFileBuffSize.GetPos() * 1024;
		CString temp(GetResString(IDS_FILEBUFFERSIZE));
		temp.AppendFormat(_T(": %s"), (LPCTSTR)CastItoXBytes(m_uFileBufferSize));
		SetDlgItemText(IDC_FILEBUFFERSIZE_STATIC, temp);
		SetModified(TRUE);
	} else if (pScrollBar->GetSafeHwnd() == m_ctlQueueSize.m_hWnd) {
		m_iQueueSize = reinterpret_cast<CSliderCtrl*>(pScrollBar)->GetPos() * 100;
		CString temp(GetResString(IDS_QUEUESIZE));
		temp.AppendFormat(_T(": %s"), (LPCTSTR)GetFormatedUInt((ULONG)m_iQueueSize));
		SetDlgItemText(IDC_QUEUESIZE_STATIC, temp);
		SetModified(TRUE);
	}
}

void CPPgTweaks::LocalizeItemText(HTREEITEM item, UINT strid)
{
	if (item)
		m_ctrlTreeOptions.SetItemText(item, GetResString(strid));
}

void CPPgTweaks::LocalizeEditLabel(HTREEITEM item, UINT strid)
{
	if (item)
		m_ctrlTreeOptions.SetEditLabel(item, GetResString(strid));
}

void CPPgTweaks::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_TWEAK));
		SetDlgItemText(IDC_WARNING, GetResString(IDS_TWEAKS_WARNING));
		SetDlgItemText(IDC_PREFINI_STATIC, GetResString(IDS_PW_TWEAK));
		SetDlgItemText(IDC_OPENPREFINI, GetResString(IDS_OPENPREFINI));

		LocalizeEditLabel(m_htiBBMaxUpClientsAllowed, IDS_BB_MAX_UPLOAD_CLIENTS);
		LocalizeEditLabel(m_htiBBSessionTransPercentValue, IDS_PERCENTAGE);
		LocalizeEditLabel(m_htiBBSessionTransAbsoluteValue, IDS_BB_ABSOLUTE_LIMIT_MIB);
		LocalizeEditLabel(m_htiBBSessionMaxTimeMinutes, IDS_LONGMINS);
		LocalizeEditLabel(m_htiBBLowRatioThreshold, IDS_BB_RATIO_THRESHOLD);
		LocalizeEditLabel(m_htiBBLowRatioBonus, IDS_BB_SCORE_BONUS);
		LocalizeEditLabel(m_htiBBLowIDDeboostDivisor, IDS_BB_DIVISOR);
		LocalizeEditLabel(m_htiDateTimeFormat4Lists, IDS_DATETIMEFORMAT4LISTS);
		LocalizeEditLabel(m_htiFileBufferTimeLimit, IDS_FILEBUFFERTIMELIMIT);
		LocalizeEditLabel(m_htiInspectAllFileTypes, IDS_INSPECTALLFILETYPES);
		LocalizeEditLabel(m_htiLogLevel, IDS_LOG_LEVEL);
		LocalizeEditLabel(m_htiMaxCon5Sec, IDS_MAXCON5SECLABEL);
		LocalizeEditLabel(m_htiMaxHalfOpen, IDS_MAXHALFOPENCONS);
		LocalizeEditLabel(m_htiUDPReceiveBuffer, IDS_UDPRECEIVEBUFFERSIZE);
		LocalizeEditLabel(m_htiTCPBigSendBuffer, IDS_TCPBIGSENDBUFFERSIZE);
		LocalizeEditLabel(m_htiMinFreeDiskSpace, IDS_MINFREEDISKSPACE);
		LocalizeEditLabel(m_htiServerKeepAliveTimeout, IDS_SERVERKEEPALIVETIMEOUT);
		LocalizeEditLabel(m_htiYourHostname, IDS_YOURHOSTNAME);	// itsonlyme: hostnameSource
		LocalizeItemText(m_htiA4AFSaveCpu, IDS_A4AF_SAVE_CPU);
		LocalizeItemText(m_htiAutoArch, IDS_DISABLE_AUTOARCHPREV);
		LocalizeItemText(m_htiAutoTakeEd2kLinks, IDS_AUTOTAKEED2KLINKS);
		LocalizeItemText(m_htiBroadband, IDS_BROADBAND);
		LocalizeItemText(m_htiBBSessionTransferLimit, IDS_BB_SESSION_TRANSFER_LIMIT);
		LocalizeItemText(m_htiBBSessionTransDisabled, IDS_DISABLED);
		LocalizeItemText(m_htiBBSessionTransPercent, IDS_BB_PERCENT_OF_FILE_SIZE);
		LocalizeItemText(m_htiBBSessionTransAbsolute, IDS_BB_ABSOLUTE_LIMIT);
		LocalizeItemText(m_htiBBSessionMaxTime, IDS_BB_ENABLE_SESSION_TIME_LIMIT);
		LocalizeItemText(m_htiBBLowRatioBoost, IDS_BB_ENABLE_LOW_RATIO_BOOST);
		LocalizeItemText(m_htiBBLowIDDeboost, IDS_BB_DEBOOST_LOWIDS);
		LocalizeItemText(m_htiCheckDiskspace, IDS_CHECKDISKSPACE);
		LocalizeItemText(m_htiCloseUPnPPorts, IDS_UPNPCLOSEONEXIT);
		LocalizeItemText(m_htiCommit, IDS_COMMITFILES);
		LocalizeItemText(m_htiCommitAlways, IDS_ALWAYS);
		LocalizeItemText(m_htiCommitNever, IDS_NEVER);
		LocalizeItemText(m_htiCommitOnShutdown, IDS_ONSHUTDOWN);
		LocalizeItemText(m_htiConditionalTCPAccept, IDS_CONDTCPACCEPT);
		LocalizeItemText(m_htiCreditSystem, IDS_USECREDITSYSTEM);
		LocalizeItemText(m_htiDebug2Disk, IDS_LOG2DISK);
		LocalizeItemText(m_htiDebugSourceExchange, IDS_DEBUG_SOURCE_EXCHANGE);
		LocalizeItemText(m_htiExtControls, IDS_SHOWEXTSETTINGS);
		LocalizeItemText(m_htiExtraPreviewWithMenu, IDS_EXTRAPREVIEWWITHMENU);
		LocalizeItemText(m_htiExtractMetaData, IDS_EXTRACT_META_DATA);
		LocalizeItemText(m_htiExtractMetaDataID3Lib, IDS_META_DATA_ID3LIB);
		//LocalizeItemText(m_htiExtractMetaDataMediaDet, IDS_META_DATA_MEDIADET);
		LocalizeItemText(m_htiExtractMetaDataNever, IDS_NEVER);
		LocalizeItemText(m_htiFilterLANIPs, IDS_PW_FILTER);
		LocalizeItemText(m_htiForceSpeedsToKB, IDS_FORCESPEEDSTOKB);
		LocalizeItemText(m_htiFullAlloc, IDS_FULLALLOC);
		LocalizeItemText(m_htiImportParts, IDS_ENABLEIMPORTPARTS);
		LocalizeItemText(m_htiKeepUnavailableFixedSharedDirs, IDS_KEEPUNAVAILABLEFIXEDSHAREDDIRS);
		LocalizeItemText(m_htiHiddenDisplay, IDS_HIDDENRUNTIME_DISPLAY);
		LocalizeItemText(m_htiHiddenFile, IDS_HIDDENRUNTIME_FILE);
		LocalizeItemText(m_htiHiddenSecurity, IDS_HIDDENRUNTIME_SECURITY);
		LocalizeItemText(m_htiHiddenStartup, IDS_HIDDENRUNTIME_STARTUP);
		LocalizeItemText(m_htiLog2Disk, IDS_LOG2DISK);
		LocalizeItemText(m_htiLogA4AF, IDS_LOG_A4AF);
		LocalizeItemText(m_htiLogBannedClients, IDS_LOG_BANNED_CLIENTS);
		LocalizeItemText(m_htiLogFileSaving, IDS_LOG_FILE_SAVING);
		LocalizeItemText(m_htiLogFilteredIPs, IDS_LOG_FILTERED_IPS);
		LocalizeItemText(m_htiLogRatingDescReceived, IDS_LOG_RATING_RECV);
		LocalizeItemText(m_htiLogSecureIdent, IDS_LOG_SECURE_IDENT);
		LocalizeItemText(m_htiLogUlDlEvents, IDS_LOG_ULDL_EVENTS);
		LocalizeItemText(m_htiMessageFromValidSourcesOnly, IDS_MESSAGEFROMVALIDSOURCESONLY);
		LocalizeItemText(m_htiPartiallyPurgeOldKnownFiles, IDS_PARTIALLYPURGEOLDKNOWNFILES);
		LocalizeItemText(m_htiPreviewCopiedArchives, IDS_PREVIEWCOPIEDARCHIVES);
		LocalizeItemText(m_htiPreviewOnIconDblClk, IDS_PREVIEWONICONDBLCLK);
		LocalizeItemText(m_htiRearrangeKadSearchKeywords, IDS_REARRANGEKADSEARCHKEYWORDS);
		LocalizeItemText(m_htiReBarToolbar, IDS_REBARTOOLBAR);
		LocalizeItemText(m_htiResolveShellLinks, IDS_RESOLVELINKS);
		LocalizeItemText(m_htiRestoreLastLogPane, IDS_RESTORELASTLOGPANE);
		LocalizeItemText(m_htiRestoreLastMainWndDlg, IDS_RESTORELASTMAINWNDDLG);
		LocalizeItemText(m_htiShareeMule, IDS_SHAREEMULELABEL);
		LocalizeItemText(m_htiShareeMuleMultiUser, IDS_SHAREEMULEMULTI);
		LocalizeItemText(m_htiShareeMuleOldStyle, IDS_SHAREEMULEOLD);
		LocalizeItemText(m_htiShareeMulePublicUser, IDS_SHAREEMULEPUBLIC);
		LocalizeItemText(m_htiSkipWANIPSetup, IDS_UPNPSKIPWANIP);
		LocalizeItemText(m_htiSkipWANPPPSetup, IDS_UPNPSKIPWANPPP);
		LocalizeItemText(m_htiShowActiveDownloadsBold, IDS_SHOWACTIVEDOWNLOADSBOLD);
		LocalizeItemText(m_htiShowUpDownIconInTaskbar, IDS_SHOWUPDOWNICONINTASKBAR);
		LocalizeItemText(m_htiShowVerticalHourMarkers, IDS_SHOWVERTICALHOURMARKERS);
		LocalizeItemText(m_htiSparsePartFiles, IDS_SPARSEPARTFILES);
		LocalizeItemText(m_htiTCPGroup, IDS_TCPIP_CONNS);
		LocalizeItemText(m_htiUPnP, IDS_UPNP);
		LocalizeItemText(m_htiUseSystemFontForMainControls, IDS_USESYSTEMFONTFORMAINCONTROLS);
		LocalizeItemText(m_htiVerbose, IDS_ENABLED);
		LocalizeItemText(m_htiVerboseGroup, IDS_VERBOSE);
		LocalizeItemText(m_htiAdjustNTFSDaylightFileTime, IDS_ADJUSTNTFSDAYLIGHTFILETIME);

		CString temp;
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_FILEBUFFERSIZE), (LPCTSTR)CastItoXBytes(m_uFileBufferSize));
		SetDlgItemText(IDC_FILEBUFFERSIZE_STATIC, temp);
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_QUEUESIZE), (LPCTSTR)GetFormatedUInt((ULONG)m_iQueueSize));
		SetDlgItemText(IDC_QUEUESIZE_STATIC, temp);
	}
}

void CPPgTweaks::OnDestroy()
{
	m_ctrlTreeOptions.DeleteAllItems();
	m_ctrlTreeOptions.DestroyWindow();
	m_bInitializedTreeOpts = false;
	m_htiTCPGroup = NULL;
	m_htiBroadband = NULL;
	m_htiBBMaxUpClientsAllowed = NULL;
	m_htiBBSessionTransferLimit = NULL;
	m_htiBBSessionTransDisabled = NULL;
	m_htiBBSessionTransPercent = NULL;
	m_htiBBSessionTransAbsolute = NULL;
	m_htiBBSessionTransPercentValue = NULL;
	m_htiBBSessionTransAbsoluteValue = NULL;
	m_htiBBSessionMaxTime = NULL;
	m_htiBBSessionMaxTimeMinutes = NULL;
	m_htiBBLowRatioBoost = NULL;
	m_htiBBLowRatioThreshold = NULL;
	m_htiBBLowRatioBonus = NULL;
	m_htiBBLowIDDeboost = NULL;
	m_htiBBLowIDDeboostDivisor = NULL;
	m_htiMaxCon5Sec = NULL;
	m_htiMaxHalfOpen = NULL;
	m_htiTCPBigSendBuffer = NULL;
	m_htiUDPReceiveBuffer = NULL;
	m_htiConditionalTCPAccept = NULL;
	m_htiAutoTakeEd2kLinks = NULL;
	m_htiVerboseGroup = NULL;
	m_htiVerbose = NULL;
	m_htiDebugSourceExchange = NULL;
	m_htiLogBannedClients = NULL;
	m_htiLogRatingDescReceived = NULL;
	m_htiLogSecureIdent = NULL;
	m_htiLogFilteredIPs = NULL;
	m_htiLogFileSaving = NULL;
	m_htiLogA4AF = NULL;
	m_htiLogLevel = NULL;
	m_htiLogUlDlEvents = NULL;
	m_htiCreditSystem = NULL;
	m_htiLog2Disk = NULL;
	m_htiDebug2Disk = NULL;
	m_htiHiddenDisplay = NULL;
	m_htiHiddenFile = NULL;
	m_htiHiddenSecurity = NULL;
	m_htiHiddenStartup = NULL;
	m_htiDateTimeFormat4Lists = NULL;
	m_htiPreviewCopiedArchives = NULL;
	m_htiInspectAllFileTypes = NULL;
	m_htiPreviewOnIconDblClk = NULL;
	m_htiShowActiveDownloadsBold = NULL;
	m_htiUseSystemFontForMainControls = NULL;
	m_htiReBarToolbar = NULL;
	m_htiShowUpDownIconInTaskbar = NULL;
	m_htiShowVerticalHourMarkers = NULL;
	m_htiForceSpeedsToKB = NULL;
	m_htiExtraPreviewWithMenu = NULL;
	m_htiKeepUnavailableFixedSharedDirs = NULL;
	m_htiPartiallyPurgeOldKnownFiles = NULL;
	m_htiAdjustNTFSDaylightFileTime = NULL;
	m_htiRearrangeKadSearchKeywords = NULL;
	m_htiMessageFromValidSourcesOnly = NULL;
	m_htiFileBufferTimeLimit = NULL;
	m_htiRestoreLastLogPane = NULL;
	m_htiRestoreLastMainWndDlg = NULL;
	m_htiCommit = NULL;
	m_htiCommitNever = NULL;
	m_htiCommitOnShutdown = NULL;
	m_htiCommitAlways = NULL;
	m_htiFilterLANIPs = NULL;
	m_htiExtControls = NULL;
	m_htiServerKeepAliveTimeout = NULL;
	m_htiSparsePartFiles = NULL;
	m_htiImportParts = NULL;
	m_htiFullAlloc = NULL;
	m_htiCheckDiskspace = NULL;
	m_htiMinFreeDiskSpace = NULL;
	m_htiYourHostname = NULL;
	m_htiA4AFSaveCpu = NULL;
	m_htiExtractMetaData = NULL;
	m_htiExtractMetaDataNever = NULL;
	m_htiExtractMetaDataID3Lib = NULL;
	m_htiAutoArch = NULL;
	m_htiUPnP = NULL;
	m_htiCloseUPnPPorts = NULL;
	m_htiSkipWANIPSetup = NULL;
	m_htiSkipWANPPPSetup = NULL;
	m_htiShareeMule = NULL;
	m_htiShareeMuleMultiUser = NULL;
	m_htiShareeMulePublicUser = NULL;
	m_htiShareeMuleOldStyle = NULL;
	//m_htiExtractMetaDataMediaDet = NULL;
	m_htiResolveShellLinks = NULL;

	CPropertyPage::OnDestroy();
}

LRESULT CPPgTweaks::OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == IDC_EXT_OPTS) {
		TREEOPTSCTRLNOTIFY *pton = (TREEOPTSCTRLNOTIFY*)lParam;
		if (m_htiVerbose && pton->hItem == m_htiVerbose) {
			BOOL bCheck;
			if (m_ctrlTreeOptions.GetCheckBox(m_htiVerbose, bCheck)) {
				if (m_htiDebug2Disk)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebug2Disk, bCheck);
				if (m_htiDebugSourceExchange)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebugSourceExchange, bCheck);
				if (m_htiLogBannedClients)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogBannedClients, bCheck);
				if (m_htiLogRatingDescReceived)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogRatingDescReceived, bCheck);
				if (m_htiLogSecureIdent)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogSecureIdent, bCheck);
				if (m_htiLogFilteredIPs)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFilteredIPs, bCheck);
				if (m_htiLogFileSaving)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFileSaving, bCheck);
				if (m_htiLogA4AF)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogA4AF, bCheck);
				if (m_htiLogUlDlEvents)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogUlDlEvents, bCheck);
			}
		} else if ((m_htiBBSessionMaxTime && pton->hItem == m_htiBBSessionMaxTime)
				|| (m_htiBBLowRatioBoost && pton->hItem == m_htiBBLowRatioBoost)
				|| (m_htiBBLowIDDeboost && pton->hItem == m_htiBBLowIDDeboost))
		{
			BOOL bCheck = FALSE;
			if (pton->hItem == m_htiBBSessionMaxTime && m_ctrlTreeOptions.GetCheckBox(m_htiBBSessionMaxTime, bCheck))
				m_ctrlTreeOptions.Expand(m_htiBBSessionMaxTime, bCheck ? TVE_EXPAND : TVE_COLLAPSE);
			else if (pton->hItem == m_htiBBLowRatioBoost && m_ctrlTreeOptions.GetCheckBox(m_htiBBLowRatioBoost, bCheck))
				m_ctrlTreeOptions.Expand(m_htiBBLowRatioBoost, bCheck ? TVE_EXPAND : TVE_COLLAPSE);
			else if (pton->hItem == m_htiBBLowIDDeboost && m_ctrlTreeOptions.GetCheckBox(m_htiBBLowIDDeboost, bCheck))
				m_ctrlTreeOptions.Expand(m_htiBBLowIDDeboost, bCheck ? TVE_EXPAND : TVE_COLLAPSE);
		} else if ((m_htiShareeMuleMultiUser  && pton->hItem == m_htiShareeMuleMultiUser)
				|| (m_htiShareeMulePublicUser && pton->hItem == m_htiShareeMulePublicUser)
				|| (m_htiShareeMuleOldStyle   && pton->hItem == m_htiShareeMuleOldStyle))
		{
			if (m_htiShareeMule && !m_bShowedWarning) {
				HTREEITEM tmp;
				int nIndex;
				m_ctrlTreeOptions.GetRadioButton(m_htiShareeMule, nIndex, tmp);
				if (nIndex != thePrefs.m_nCurrentUserDirMode) {
					// TODO offer cancel option
					LocMessageBox(IDS_SHAREEMULEWARNING, MB_ICONINFORMATION | MB_OK, 0);
					m_bShowedWarning = true;
				}
			}
		}
		SetModified();
	}
	return 0;
}

void CPPgTweaks::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Extended_Settings);
}

BOOL CPPgTweaks::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgTweaks::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgTweaks::OnBnClickedOpenprefini()
{
	ShellOpenFile(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("preferences.ini"));
}
