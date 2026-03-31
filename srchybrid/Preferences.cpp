//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( devs@emule-project.net / https://www.emule-project.net )
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
#include <share.h>
#include <ShlObj.h>
#include <dwmapi.h>
#include <iphlpapi.h>
#include "emule.h"
#include "ModernLimits.h"
#include "Preferences.h"
#include "Opcodes.h"
#include "UpDownClient.h"
#include "Ini2.h"
#include "DownloadQueue.h"
#include "UploadQueue.h"
#include "Statistics.h"
#include "MD5Sum.h"
#include "PartFile.h"
#include "ServerConnect.h"
#include "ListenSocket.h"
#include "ServerList.h"
#include "SharedFileList.h"
#include "emuledlg.h"
#include "StatisticsDlg.h"
#include "Log.h"
#include "BindAddressResolver.h"
#include "MuleToolbarCtrl.h"
#include "../../eMule-cryptopp/osrng.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SHAREDDIRS	_T("shareddir.dat")
LPCTSTR const strPreferencesDat = _T("preferences.dat");
LPCTSTR const strDefaultToolbar = _T("00990102030405060799080910");
CPreferences thePrefs;

namespace
{
	static const uint32 s_uDefaultUploadLimit = 20000;
	static const uint32 s_uDefaultDownloadLimit = 100000;
	static const uint16 s_uStartupTcpPortMin = 21756;
	static const uint16 s_uStartupTcpPortMax = 65500;
	static const uint16 s_uStartupUdpPortOffset = 10;

	/**
	 * @brief Convert persisted bandwidth input into the live limit format.
	 *
	 * A stored value of `0` or any negative number now means "unlimited".
	 */
	static uint32 NormalizeBandwidthLimit(const int nConfiguredLimit)
	{
		if (nConfiguredLimit <= 0)
			return UNLIMITED;
		return static_cast<uint32>(nConfiguredLimit);
	}

	/**
	 * @brief Persist unlimited limits as `0` so the single-limit UI round-trips cleanly.
	 */
	static int SerializeBandwidthLimit(const uint32 uConfiguredLimit)
	{
		return (uConfiguredLimit == UNLIMITED) ? 0 : static_cast<int>(uConfiguredLimit);
	}

	/**
	 * @brief Keep graph auto-ranging anchored to the last useful ceiling or the latest live estimate.
	 */
	static uint32 GetEstimatedGraphRange(const uint32 uFallbackRange, const uint32 uEstimatedRate)
	{
		const uint32 uEstimatedRange = (uEstimatedRate != 0) ? (uEstimatedRate + 4) : 0;
		return max(uFallbackRange, uEstimatedRange);
	}

	static void SetResolvedBindAddress(const CString &strResolvedAddress
		, CStringW &rstrBindAddrW
		, LPCWSTR &rpszBindAddrW
		, CStringA &rstrBindAddrA
		, LPCSTR &rpszBindAddrA)
	{
		rstrBindAddrW = strResolvedAddress;
		rpszBindAddrW = rstrBindAddrW.IsEmpty() ? NULL : (LPCWSTR)rstrBindAddrW;
		rstrBindAddrA = rstrBindAddrW;
		rpszBindAddrA = rstrBindAddrA.IsEmpty() ? NULL : (LPCSTR)rstrBindAddrA;
	}

	static void LogBindAddressResolution(LPCTSTR pszBindingName
		, const CString &strInterfaceId
		, const CString &strInterfaceName
		, const CString &strConfiguredAddress
		, EBindAddressResolveResult eResult
		, const CString &strResolvedAddress)
	{
		switch (eResult) {
		case BARR_Default:
			if (strInterfaceId.IsEmpty() && strConfiguredAddress.IsEmpty())
				DebugLog(_T("%s binding uses the default interface selection"), pszBindingName);
			break;
		case BARR_Resolved:
			if (!strResolvedAddress.IsEmpty())
				DebugLog(_T("%s binding resolved to %s"), pszBindingName, (LPCTSTR)strResolvedAddress);
			break;
		case BARR_InterfaceNotFound:
			DebugLogWarning(_T("%s binding interface not found; falling back to the default interface selection (%s)"), pszBindingName
				, !strInterfaceName.IsEmpty() ? (LPCTSTR)strInterfaceName : (LPCTSTR)strInterfaceId);
			break;
		case BARR_InterfaceHasNoAddress:
			DebugLogWarning(_T("%s binding interface has no usable IPv4 address; falling back to the default interface selection (%s)"), pszBindingName
				, !strInterfaceName.IsEmpty() ? (LPCTSTR)strInterfaceName : (LPCTSTR)strInterfaceId);
			break;
		case BARR_AddressNotFoundOnInterface:
			DebugLogWarning(_T("%s binding address %s was not found on interface %s; using %s instead"), pszBindingName
				, (LPCTSTR)strConfiguredAddress
				, !strInterfaceName.IsEmpty() ? (LPCTSTR)strInterfaceName : (LPCTSTR)strInterfaceId
				, (LPCTSTR)strResolvedAddress);
			break;
		case BARR_AddressNotFound:
			DebugLogWarning(_T("%s binding address %s is no longer present on any live interface"), pszBindingName
				, (LPCTSTR)strConfiguredAddress);
			break;
		default:
			ASSERT(0);
		}
	}

	static EBindAddressResolveResult ResolveConfiguredBinding(LPCTSTR pszBindingName
		, const CString &strInterfaceId
		, CString &rstrInterfaceName
		, const CString &strConfiguredAddress
		, CStringW &rstrBindAddrW
		, LPCWSTR &rpszBindAddrW
		, CStringA &rstrBindAddrA
		, LPCSTR &rpszBindAddrA)
	{
		CString strResolvedAddress;
		CString strResolvedInterfaceName;
		const EBindAddressResolveResult eResult = CBindAddressResolver::ResolveBindAddress(strInterfaceId, strConfiguredAddress
			, strResolvedAddress, &strResolvedInterfaceName);

		if (!strResolvedInterfaceName.IsEmpty())
			rstrInterfaceName = strResolvedInterfaceName;

		if (eResult == BARR_InterfaceNotFound || eResult == BARR_InterfaceHasNoAddress)
			strResolvedAddress.Empty();

		SetResolvedBindAddress(strResolvedAddress, rstrBindAddrW, rpszBindAddrW, rstrBindAddrA, rpszBindAddrA);
		LogBindAddressResolution(pszBindingName, strInterfaceId, rstrInterfaceName, strConfiguredAddress, eResult, strResolvedAddress);
		return eResult;
	}

	static uint16 GetRandomStartupTCPPort(bool bReserveUdpPair)
	{
		// Keep generated startup TCP defaults in the configured range and optionally reserve the paired UDP port.
		PMIB_TCPTABLE pTCPTab = NULL;
		PMIB_UDPTABLE pUDPTab = NULL;
		ULONG dwTCPSize = 0;
		ULONG dwUDPSize = 0;

		if (GetTcpTable(NULL, &dwTCPSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
			dwTCPSize += sizeof(MIB_TCPROW) * 50;
			pTCPTab = reinterpret_cast<PMIB_TCPTABLE>(malloc(dwTCPSize));
			if (pTCPTab != NULL && GetTcpTable(pTCPTab, &dwTCPSize, TRUE) != ERROR_SUCCESS) {
				free(pTCPTab);
				pTCPTab = NULL;
			}
		}

		if (GetUdpTable(NULL, &dwUDPSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
			dwUDPSize += sizeof(MIB_UDPROW) * 50;
			pUDPTab = reinterpret_cast<PMIB_UDPTABLE>(malloc(dwUDPSize));
			if (pUDPTab != NULL && GetUdpTable(pUDPTab, &dwUDPSize, TRUE) != ERROR_SUCCESS) {
				free(pUDPTab);
				pUDPTab = NULL;
			}
		}

		const UINT uCandidateRange = static_cast<UINT>(s_uStartupTcpPortMax - s_uStartupTcpPortMin + 1);
		int iMaxTests = uCandidateRange;
		uint16 nTcpPort = s_uStartupTcpPortMin;
		bool bPairIsFree = false;
		do {
			nTcpPort = static_cast<uint16>(s_uStartupTcpPortMin + (GetRandomUInt16() % uCandidateRange));
			const uint16 nUdpPort = static_cast<uint16>(nTcpPort + s_uStartupUdpPortOffset);
			bPairIsFree = true;

			if (pTCPTab != NULL) {
				const uint16 nTcpPortBE = htons(nTcpPort);
				for (DWORD e = pTCPTab->dwNumEntries; e-- > 0;) {
					if (pTCPTab->table[e].dwLocalPort == nTcpPortBE) {
						bPairIsFree = false;
						break;
					}
				}
			}

			if (bPairIsFree && bReserveUdpPair && pUDPTab != NULL) {
				const uint16 nUdpPortBE = htons(nUdpPort);
				for (DWORD e = pUDPTab->dwNumEntries; e-- > 0;) {
					if (pUDPTab->table[e].dwLocalPort == nUdpPortBE) {
						bPairIsFree = false;
						break;
					}
				}
			}
		} while (!bPairIsFree && --iMaxTests > 0);

		free(pTCPTab);
		free(pUDPTab);
		return nTcpPort;
	}
}

CString CPreferences::m_astrDefaultDirs[12];
CString	CPreferences::strNick;
bool	CPreferences::m_abDefaultDirsCreated[13] = {};
int		CPreferences::m_nCurrentUserDirMode = -1;
int		CPreferences::m_iDbgHeap;
uint32	CPreferences::m_maxUpClientsAllowed;
uint64	CPreferences::m_bbSessionMaxTrans;
uint64	CPreferences::m_bbSessionMaxTime;
float	CPreferences::m_bbBoostLowRatioFiles;
float	CPreferences::m_bbBoostLowRatioFilesBy;
uint32	CPreferences::m_bbDeboostLowIDs;
uint32	CPreferences::m_maxupload;
uint32	CPreferences::m_maxdownload;
CString	CPreferences::m_strConfiguredBindAddr;
CString	CPreferences::m_strBindInterface;
CString	CPreferences::m_strBindInterfaceName;
LPCSTR	CPreferences::m_pszBindAddrA;
CStringA CPreferences::m_strBindAddrA;
LPCWSTR	CPreferences::m_pszBindAddrW;
CStringW CPreferences::m_strBindAddrW;
EBindAddressResolveResult CPreferences::m_eBindAddrResolveResult = BARR_Default;
bool CPreferences::m_bBlockNetworkWhenBindUnavailableAtStartup;
uint16	CPreferences::port;
uint16	CPreferences::udpport;
uint16	CPreferences::nServerUDPPort;
UINT	CPreferences::maxconnections;
UINT	CPreferences::maxhalfconnections;
bool	CPreferences::m_bConditionalTCPAccept;
bool	CPreferences::reconnect;
bool	CPreferences::m_bUseServerPriorities;
bool	CPreferences::m_bUseUserSortedServerList;
CString	CPreferences::m_strIncomingDir;
CStringArray CPreferences::tempdir;
bool	CPreferences::ICH;
bool	CPreferences::m_bAutoUpdateServerList;
bool	CPreferences::mintotray;
bool	CPreferences::autoconnect;
bool	CPreferences::m_bAutoConnectToStaticServersOnly;
bool	CPreferences::autotakeed2klinks;
bool	CPreferences::addnewfilespaused;
UINT	CPreferences::depth3D;
int		CPreferences::m_iStraightWindowStyles;
bool	CPreferences::m_bUseSystemFontForMainControls;
bool	CPreferences::m_bRTLWindowsLayout;
CString	CPreferences::m_strSkinProfile;
CString	CPreferences::m_strSkinProfileDir;
bool	CPreferences::m_bAddServersFromServer;
bool	CPreferences::m_bAddServersFromClients;
UINT	CPreferences::maxsourceperfile;
UINT	CPreferences::trafficOMeterInterval;
UINT	CPreferences::statsInterval;
bool	CPreferences::m_bFillGraphs;
uchar	CPreferences::userhash[MDX_DIGEST_SIZE];
WINDOWPLACEMENT CPreferences::EmuleWindowPlacement;
uint32	CPreferences::maxGraphDownloadRate;
uint32	CPreferences::maxGraphUploadRate;
uint32	CPreferences::maxGraphDownloadRateEstimated = 0;
uint32	CPreferences::maxGraphUploadRateEstimated = 0;
bool	CPreferences::beepOnError;
bool	CPreferences::m_bIconflashOnNewMessage;
bool	CPreferences::confirmExit;
DWORD	CPreferences::m_adwStatsColors[15];
bool	CPreferences::m_bHasCustomTaskIconColor;
bool	CPreferences::filterLANIPs;
bool	CPreferences::m_bAllocLocalHostIP;
bool	CPreferences::onlineSig;
uint64	CPreferences::cumDownOverheadTotal;
uint64	CPreferences::cumDownOverheadFileReq;
uint64	CPreferences::cumDownOverheadSrcEx;
uint64	CPreferences::cumDownOverheadServer;
uint64	CPreferences::cumDownOverheadKad;
uint64	CPreferences::cumDownOverheadTotalPackets;
uint64	CPreferences::cumDownOverheadFileReqPackets;
uint64	CPreferences::cumDownOverheadSrcExPackets;
uint64	CPreferences::cumDownOverheadServerPackets;
uint64	CPreferences::cumDownOverheadKadPackets;
uint64	CPreferences::cumUpOverheadTotal;
uint64	CPreferences::cumUpOverheadFileReq;
uint64	CPreferences::cumUpOverheadSrcEx;
uint64	CPreferences::cumUpOverheadServer;
uint64	CPreferences::cumUpOverheadKad;
uint64	CPreferences::cumUpOverheadTotalPackets;
uint64	CPreferences::cumUpOverheadFileReqPackets;
uint64	CPreferences::cumUpOverheadSrcExPackets;
uint64	CPreferences::cumUpOverheadServerPackets;
uint64	CPreferences::cumUpOverheadKadPackets;
uint32	CPreferences::cumUpSuccessfulSessions;
uint32	CPreferences::cumUpFailedSessions;
uint32	CPreferences::cumUpAvgTime;
uint64	CPreferences::cumUpData_EDONKEY;
uint64	CPreferences::cumUpData_EDONKEYHYBRID;
uint64	CPreferences::cumUpData_EMULE;
uint64	CPreferences::cumUpData_MLDONKEY;
uint64	CPreferences::cumUpData_AMULE;
uint64	CPreferences::cumUpData_EMULECOMPAT;
uint64	CPreferences::cumUpData_SHAREAZA;
uint64	CPreferences::sesUpData_EDONKEY;
uint64	CPreferences::sesUpData_EDONKEYHYBRID;
uint64	CPreferences::sesUpData_EMULE;
uint64	CPreferences::sesUpData_MLDONKEY;
uint64	CPreferences::sesUpData_AMULE;
uint64	CPreferences::sesUpData_EMULECOMPAT;
uint64	CPreferences::sesUpData_SHAREAZA;
uint64	CPreferences::cumUpDataPort_4662;
uint64	CPreferences::cumUpDataPort_OTHER;
uint64	CPreferences::sesUpDataPort_4662;
uint64	CPreferences::sesUpDataPort_OTHER;
uint64	CPreferences::cumUpData_File;
uint64	CPreferences::cumUpData_Partfile;
uint64	CPreferences::sesUpData_File;
uint64	CPreferences::sesUpData_Partfile;
uint32	CPreferences::cumDownCompletedFiles;
uint32	CPreferences::cumDownSuccessfulSessions;
uint32	CPreferences::cumDownFailedSessions;
uint32	CPreferences::cumDownAvgTime;
uint64	CPreferences::cumLostFromCorruption;
uint64	CPreferences::cumSavedFromCompression;
uint32	CPreferences::cumPartsSavedByICH;
uint32	CPreferences::sesDownSuccessfulSessions;
uint32	CPreferences::sesDownFailedSessions;
uint32	CPreferences::sesDownAvgTime;
uint32	CPreferences::sesDownCompletedFiles;
uint64	CPreferences::sesLostFromCorruption;
uint64	CPreferences::sesSavedFromCompression;
uint32	CPreferences::sesPartsSavedByICH;
uint64	CPreferences::cumDownData_EDONKEY;
uint64	CPreferences::cumDownData_EDONKEYHYBRID;
uint64	CPreferences::cumDownData_EMULE;
uint64	CPreferences::cumDownData_MLDONKEY;
uint64	CPreferences::cumDownData_AMULE;
uint64	CPreferences::cumDownData_EMULECOMPAT;
uint64	CPreferences::cumDownData_SHAREAZA;
uint64	CPreferences::cumDownData_URL;
uint64	CPreferences::sesDownData_EDONKEY;
uint64	CPreferences::sesDownData_EDONKEYHYBRID;
uint64	CPreferences::sesDownData_EMULE;
uint64	CPreferences::sesDownData_MLDONKEY;
uint64	CPreferences::sesDownData_AMULE;
uint64	CPreferences::sesDownData_EMULECOMPAT;
uint64	CPreferences::sesDownData_SHAREAZA;
uint64	CPreferences::sesDownData_URL;
uint64	CPreferences::cumDownDataPort_4662;
uint64	CPreferences::cumDownDataPort_OTHER;
uint64	CPreferences::sesDownDataPort_4662;
uint64	CPreferences::sesDownDataPort_OTHER;
float	CPreferences::cumConnAvgDownRate;
float	CPreferences::cumConnMaxAvgDownRate;
float	CPreferences::cumConnMaxDownRate;
float	CPreferences::cumConnAvgUpRate;
float	CPreferences::cumConnMaxAvgUpRate;
float	CPreferences::cumConnMaxUpRate;
time_t	CPreferences::cumConnRunTime;
uint32	CPreferences::cumConnNumReconnects;
uint32	CPreferences::cumConnAvgConnections;
uint32	CPreferences::cumConnMaxConnLimitReached;
uint32	CPreferences::cumConnPeakConnections;
uint32	CPreferences::cumConnTransferTime;
uint32	CPreferences::cumConnDownloadTime;
uint32	CPreferences::cumConnUploadTime;
uint32	CPreferences::cumConnServerDuration;
uint32	CPreferences::cumSrvrsMostWorkingServers;
uint32	CPreferences::cumSrvrsMostUsersOnline;
uint32	CPreferences::cumSrvrsMostFilesAvail;
uint32	CPreferences::cumSharedMostFilesShared;
uint64	CPreferences::cumSharedLargestShareSize;
uint64	CPreferences::cumSharedLargestAvgFileSize;
uint64	CPreferences::cumSharedLargestFileSize;
time_t	CPreferences::stat_datetimeLastReset;
UINT	CPreferences::statsConnectionsGraphRatio;
UINT	CPreferences::statsSaveInterval;
CString	CPreferences::m_strStatsExpandedTreeItems;
bool	CPreferences::m_bShowVerticalHourMarkers;
uint64	CPreferences::totalDownloadedBytes;
uint64	CPreferences::totalUploadedBytes;
LANGID	CPreferences::m_wLanguageID;
bool	CPreferences::transferDoubleclick;
EViewSharedFilesAccess CPreferences::m_iSeeShares;
UINT	CPreferences::m_iToolDelayTime;
bool	CPreferences::bringtoforeground;
UINT	CPreferences::splitterbarPosition;
UINT	CPreferences::splitterbarPositionSvr;
UINT	CPreferences::splitterbarPositionStat;
UINT	CPreferences::splitterbarPositionStat_HL;
UINT	CPreferences::splitterbarPositionStat_HR;
UINT	CPreferences::splitterbarPositionFriend;
UINT	CPreferences::splitterbarPositionShared;
UINT	CPreferences::m_uTransferWnd1;
UINT	CPreferences::m_uTransferWnd2;
UINT	CPreferences::m_uDeadServerRetries;
DWORD	CPreferences::m_dwServerKeepAliveTimeout;
DWORD	CPreferences::m_dwConnectionTimeout;
DWORD	CPreferences::m_dwDownloadTimeout;
UINT	CPreferences::statsMax;
UINT	CPreferences::statsAverageMinutes;
CString	CPreferences::notifierConfiguration;
bool	CPreferences::notifierOnDownloadFinished;
bool	CPreferences::notifierOnNewDownload;
bool	CPreferences::notifierOnChat;
bool	CPreferences::notifierOnLog;
bool	CPreferences::notifierOnImportantError;
bool	CPreferences::notifierOnEveryChatMsg;
ENotifierSoundType CPreferences::notifierSoundType = ntfstNoSound;
CString	CPreferences::notifierSoundFile;
bool	CPreferences::m_bMessageEnableSmileys;
bool	CPreferences::m_bRemove2bin;
bool	CPreferences::m_bShowCopyEd2kLinkCmd;
bool	CPreferences::m_bpreviewprio;
bool	CPreferences::m_bSmartServerIdCheck;
uint8	CPreferences::smartidstate;
bool	CPreferences::m_bSafeServerConnect;
bool	CPreferences::startMinimized;
bool	CPreferences::m_bAutoStart;
bool	CPreferences::m_bRestoreLastMainWndDlg;
int		CPreferences::m_iLastMainWndDlgID;
bool	CPreferences::m_bRestoreLastLogPane;
int		CPreferences::m_iLastLogPaneID;
UINT	CPreferences::MaxConperFive;
bool	CPreferences::checkDiskspace;
UINT	CPreferences::m_uMinFreeDiskSpace;
bool	CPreferences::m_bSparsePartFiles;
bool	CPreferences::m_bImportParts;
CString	CPreferences::m_strYourHostname;
CString	CPreferences::m_strNodesDatUpdateUrl;
bool	CPreferences::m_bEnableVerboseOptions;
bool	CPreferences::m_bVerbose;
bool	CPreferences::m_bFullVerbose;
bool	CPreferences::m_bDebugSourceExchange;
bool	CPreferences::m_bLogBannedClients;
bool	CPreferences::m_bLogRatingDescReceived;
bool	CPreferences::m_bLogSecureIdent;
bool	CPreferences::m_bLogFilteredIPs;
bool	CPreferences::m_bLogFileSaving;
bool	CPreferences::m_bLogA4AF; // ZZ:DownloadManager
bool	CPreferences::m_bLogUlDlEvents;
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
bool	CPreferences::m_bUseDebugDevice = true;
#else
bool	CPreferences::m_bUseDebugDevice = false;
#endif
int		CPreferences::m_iDebugServerTCPLevel;
int		CPreferences::m_iDebugServerUDPLevel;
int		CPreferences::m_iDebugServerSourcesLevel;
int		CPreferences::m_iDebugServerSearchesLevel;
int		CPreferences::m_iDebugClientTCPLevel;
int		CPreferences::m_iDebugClientUDPLevel;
int		CPreferences::m_iDebugClientKadUDPLevel;
int		CPreferences::m_iDebugSearchResultDetailLevel;
bool	CPreferences::m_bupdatequeuelist;
bool	CPreferences::m_bManualAddedServersHighPriority;
bool	CPreferences::m_btransferfullchunks;
int		CPreferences::m_istartnextfile;
bool	CPreferences::m_bshowoverhead;
bool	CPreferences::m_bDAP;
bool	CPreferences::m_bUAP;
bool	CPreferences::m_bDisableKnownClientList;
bool	CPreferences::m_bDisableQueueList;
bool	CPreferences::m_bExtControls;
bool	CPreferences::m_bTransflstRemain;
bool	CPreferences::showRatesInTitle;
CString	CPreferences::m_strTxtEditor;
CString	CPreferences::m_strVideoPlayer;
CString CPreferences::m_strVideoPlayerArgs;
bool	CPreferences::m_bMoviePreviewBackup;
int		CPreferences::m_iPreviewSmallBlocks;
bool	CPreferences::m_bPreviewCopiedArchives;
int		CPreferences::m_iInspectAllFileTypes;
bool	CPreferences::m_bPreviewOnIconDblClk;
bool	CPreferences::m_bCheckFileOpen;
bool	CPreferences::indicateratings;
bool	CPreferences::watchclipboard;
bool	CPreferences::filterserverbyip;
bool	CPreferences::m_bFirstStart;
bool	CPreferences::m_bBetaNaggingDone;
bool	CPreferences::m_bCreditSystem;
bool	CPreferences::log2disk;
bool	CPreferences::debug2disk;
int		CPreferences::iMaxLogBuff;
UINT	CPreferences::uMaxLogFileSize;
ELogFileFormat CPreferences::m_iLogFileFormat = Unicode;
bool	CPreferences::msgonlyfriends;
bool	CPreferences::msgsecure;
bool	CPreferences::m_bUseChatCaptchas;
UINT	CPreferences::filterlevel;
UINT	CPreferences::m_uFileBufferSize;
UINT	CPreferences::m_uUDPReceiveBufferSize;
UINT	CPreferences::m_uTCPSendBufferSize;
UINT	CPreferences::m_uUploadClientDataRate;
DWORD	CPreferences::m_uFileBufferTimeLimit;
INT_PTR	CPreferences::m_iQueueSize;
int		CPreferences::m_iCommitFiles;
UINT	CPreferences::maxmsgsessions;
CString	CPreferences::messageFilter;
CString	CPreferences::commentFilter;
CString	CPreferences::filenameCleanups;
CString	CPreferences::m_strDateTimeFormat;
CString	CPreferences::m_strDateTimeFormat4Log;
CString	CPreferences::m_strDateTimeFormat4Lists;
LOGFONT CPreferences::m_lfHyperText;
LOGFONT CPreferences::m_lfLogText;
COLORREF CPreferences::m_crLogError = RGB(255, 0, 0);
COLORREF CPreferences::m_crLogWarning = RGB(128, 0, 128);
COLORREF CPreferences::m_crLogSuccess = RGB(0, 0, 255);
int		CPreferences::m_iExtractMetaData;
bool	CPreferences::m_bAdjustNTFSDaylightFileTime = false; //'true' causes rehashing in XP and above when DST switches on/off
bool	CPreferences::m_bRearrangeKadSearchKeywords;
bool	CPreferences::m_bBanBadKadNodes;

ProxySettings CPreferences::proxy;
bool	CPreferences::showCatTabInfos;
bool	CPreferences::resumeSameCat;
bool	CPreferences::dontRecreateGraphs;
bool	CPreferences::autofilenamecleanup;
bool	CPreferences::m_bUseAutocompl;
bool	CPreferences::m_bShowDwlPercentage;
bool	CPreferences::m_bRemoveFinishedDownloads;
INT_PTR	CPreferences::m_iMaxChatHistory;
bool	CPreferences::m_bShowActiveDownloadsBold;
int		CPreferences::m_iSearchMethod;
bool	CPreferences::m_bAdvancedSpamfilter;
bool	CPreferences::m_bUseSecureIdent;
bool	CPreferences::networkkademlia;
bool	CPreferences::networked2k;
EToolbarLabelType CPreferences::m_nToolbarLabels;
CString	CPreferences::m_sToolbarBitmap;
CString	CPreferences::m_sToolbarBitmapFolder;
CString	CPreferences::m_sToolbarSettings;
bool	CPreferences::m_bReBarToolbar;
CSize	CPreferences::m_sizToolbarIconSize;
bool	CPreferences::m_bPreviewEnabled;
bool	CPreferences::m_bAutomaticArcPreviewStart;
bool    CPreferences::m_bAllocFull;
bool	CPreferences::m_bShowSharedFilesDetails;
bool	CPreferences::m_bShowUpDownIconInTaskbar;
bool	CPreferences::m_bShowTaskbarProgress;
bool	CPreferences::m_bForceSpeedsToKB;
bool	CPreferences::m_bAutoShowLookups;
bool	CPreferences::m_bExtraPreviewWithMenu;

// ZZ:DownloadManager -->
bool    CPreferences::m_bA4AFSaveCpu;
// ZZ:DownloadManager <--
bool    CPreferences::m_bHighresTimer;
bool	CPreferences::m_bResolveSharedShellLinks;
bool	CPreferences::m_bAutoRescanSharedFolders;
UINT	CPreferences::m_uAutoRescanSharedFoldersIntervalSec;
bool	CPreferences::m_bAutoShareNewSharedSubdirs;
bool	CPreferences::m_bKeepUnavailableFixedSharedDirs;
CStringList CPreferences::shareddir_list;
CStringList CPreferences::addresses_list;
CString CPreferences::m_strFileCommentsFilePath;
Preferences_Ext_Struct *CPreferences::prefsExt;
CArray<Category_Struct*, Category_Struct*> CPreferences::catArr;
UINT	CPreferences::m_nWebMirrorAlertLevel;
bool	CPreferences::m_bUseOldTimeRemaining;

bool	CPreferences::m_bRandomizePortsOnStartup;
int		CPreferences::m_byLogLevel;
bool	CPreferences::m_bTrustEveryHash;
bool	CPreferences::m_bRememberCancelledFiles;
bool	CPreferences::m_bRememberDownloadedFiles;
bool	CPreferences::m_bPartiallyPurgeOldKnownFiles;

bool	CPreferences::m_bWinaTransToolbar;
bool	CPreferences::m_bShowDownloadToolbar;

bool	CPreferences::m_bCryptLayerRequested;
bool	CPreferences::m_bCryptLayerSupported;
bool	CPreferences::m_bCryptLayerRequired;
uint32	CPreferences::m_dwKadUDPKey;
uint32	CPreferences::m_uKadPublishSourceThrottle;
uint8	CPreferences::m_byCryptTCPPaddingLength;

bool	CPreferences::m_bSkipWANIPSetup;
bool	CPreferences::m_bSkipWANPPPSetup;
bool	CPreferences::m_bEnableUPnP;
bool	CPreferences::m_bCloseUPnPOnExit;
bool	CPreferences::m_bIsWinServImplDisabled;
bool	CPreferences::m_bIsMinilibImplDisabled;
int		CPreferences::m_nLastWorkingImpl;

bool	CPreferences::m_bEnableSearchResultFilter;

BOOL	CPreferences::m_bIsRunningAeroGlass;
bool	CPreferences::m_bPreventStandby;
bool	CPreferences::m_bStoreSearches;

CPreferences::CPreferences()
{
#ifdef _DEBUG
	m_iDbgHeap = 1;
#endif
}

CPreferences::~CPreferences()
{
	delete prefsExt;
}

LPCTSTR CPreferences::GetConfigFile()
{
	return theApp.m_pszProfileName;
}

void CPreferences::MovePreferences(EDefaultDirectory eSrc, LPCTSTR const sFile, const CString &dst)
{
	const CString &src(GetMuleDirectory(eSrc));
	const CString &pathTxt(src + sFile);
	if (::PathFileExists(pathTxt))
		::MoveFile(pathTxt, dst + sFile);
}

void CPreferences::Init()
{
	//srand((unsigned)time(NULL)); // we need random numbers sometimes

	prefsExt = new Preferences_Ext_Struct{};

	const CString &sConfDir(GetMuleDirectory(EMULE_CONFIGDIR));
	m_strFileCommentsFilePath.Format(_T("%sfileinfo.ini"), (LPCTSTR)sConfDir);

	///////////////////////////////////////////////////////////////////////////
	// Move *.log files from application directory into 'log' directory
	//
	CFileFind ff;
	for (BOOL bFound = ff.FindFile(GetMuleDirectory(EMULE_EXECUTABLEDIR) + _T("eMule*.log")); bFound;) {
		bFound = ff.FindNextFile();
		if (!ff.IsDirectory() && !ff.IsSystem() && !ff.IsHidden())
			::MoveFile(ff.GetFilePath(), GetMuleDirectory(EMULE_LOGDIR) + ff.GetFileName());
	}
	ff.Close();

	///////////////////////////////////////////////////////////////////////////
	// Move 'downloads.txt/bak' files from application and/or database directory
	// into 'config' directory
	//
	static LPCTSTR const strDownloadsTxt = _T("downloads.txt");
	static LPCTSTR const strDownloadsBak = _T("downloads.bak");
	MovePreferences(EMULE_DATABASEDIR, strDownloadsTxt, sConfDir);
	MovePreferences(EMULE_EXECUTABLEDIR, strDownloadsTxt, sConfDir);
	MovePreferences(EMULE_DATABASEDIR, strDownloadsBak, sConfDir);
	MovePreferences(EMULE_EXECUTABLEDIR, strDownloadsBak, sConfDir);

	// load preferences.dat or set standard values
	CString strFullPath(sConfDir + strPreferencesDat);
	FILE *preffile = _tfsopen(strFullPath, _T("rb"), _SH_DENYWR);

	LoadPreferences();

	if (!preffile || fread(prefsExt, sizeof(Preferences_Ext_Struct), 1, preffile) != 1 || ferror(preffile))
		SetStandardValues();
	else {
		md4cpy(userhash, prefsExt->userhash);
		EmuleWindowPlacement = prefsExt->EmuleWindowPlacement;

		fclose(preffile);
		smartidstate = 0;
	}
	CreateUserHash();

	// shared directories
	strFullPath.Format(_T("%s") SHAREDDIRS, (LPCTSTR)sConfDir);
	bool bIsUnicodeFile = IsUnicodeFile(strFullPath); // check for BOM
	// open the text file either as ANSI (text) or Unicode (binary),
	// this way we can read old and new files with almost the same code.
	CStdioFile sdirfile;
	if (sdirfile.Open(strFullPath, CFile::modeRead | CFile::shareDenyWrite | (bIsUnicodeFile ? CFile::typeBinary : 0))) {
		try {
			if (bIsUnicodeFile)
				sdirfile.Seek(sizeof(WORD), CFile::begin); // skip BOM

			CString toadd;
			while (sdirfile.ReadString(toadd)) {
				toadd.Trim(_T(" \t\r\n")); // need to trim '\r' in binary mode
				if (!toadd.IsEmpty()) {
					MakeFoldername(toadd);
					// skip non-shareable directories
					// maybe skip non-existing directories on fixed disks only
					if (IsShareableDirectory(toadd) && (m_bKeepUnavailableFixedSharedDirs || DirAccsess(toadd)))
						shareddir_list.AddTail(toadd);
				}
			}
		} catch (CFileException *ex) {
			ASSERT(0);
			ex->Delete();
		}
		sdirfile.Close();
	}

	// server list addresses
	strFullPath.Format(_T("%s") _T("addresses.dat"), (LPCTSTR)sConfDir);
	bIsUnicodeFile = IsUnicodeFile(strFullPath);
	if (sdirfile.Open(strFullPath, CFile::modeRead | CFile::shareDenyWrite | (bIsUnicodeFile ? CFile::typeBinary : 0))) {
		try {
			if (bIsUnicodeFile)
				sdirfile.Seek(sizeof(WORD), CFile::current); // skip BOM

			CString toadd;
			while (sdirfile.ReadString(toadd)) {
				toadd.Trim(_T(" \t\r\n")); // need to trim '\r' in binary mode
				if (!toadd.IsEmpty())
					addresses_list.AddTail(toadd);
			}
		} catch (CFileException *ex) {
			ASSERT(0);
			ex->Delete();
		}
		sdirfile.Close();
	}

	// Explicitly inform the user about errors with incoming/temp folders!
	if (!::PathFileExists(GetMuleDirectory(EMULE_INCOMINGDIR)) && !::CreateDirectory(GetMuleDirectory(EMULE_INCOMINGDIR), 0)) {
		CString strError;
		strError.Format(GetResString(IDS_ERR_CREATE_DIR), (LPCTSTR)GetResString(IDS_PW_INCOMING), (LPCTSTR)GetMuleDirectory(EMULE_INCOMINGDIR), (LPCTSTR)GetErrorMessage(::GetLastError()));
		AfxMessageBox(strError, MB_ICONERROR);

		m_strIncomingDir = GetDefaultDirectory(EMULE_INCOMINGDIR, true); // will also try to create it if needed
		if (!::PathFileExists(GetMuleDirectory(EMULE_INCOMINGDIR))) {
			strError.Format(GetResString(IDS_ERR_CREATE_DIR), (LPCTSTR)GetResString(IDS_PW_INCOMING), (LPCTSTR)GetMuleDirectory(EMULE_INCOMINGDIR), (LPCTSTR)GetErrorMessage(::GetLastError()));
			AfxMessageBox(strError, MB_ICONERROR);
		}
	}
	if (!::PathFileExists(GetTempDir()) && !::CreateDirectory(GetTempDir(), 0)) {
		CString strError;
		strError.Format(GetResString(IDS_ERR_CREATE_DIR), (LPCTSTR)GetResString(IDS_PW_TEMP), GetTempDir(), (LPCTSTR)GetErrorMessage(::GetLastError()));
		AfxMessageBox(strError, MB_ICONERROR);

		tempdir[0] = GetDefaultDirectory(EMULE_TEMPDIR, true); // will also try to create it if needed;
		if (!::PathFileExists(GetTempDir())) {
			strError.Format(GetResString(IDS_ERR_CREATE_DIR), (LPCTSTR)GetResString(IDS_PW_TEMP), GetTempDir(), (LPCTSTR)GetErrorMessage(::GetLastError()));
			AfxMessageBox(strError, MB_ICONERROR);
		}
	}

	// Create 'skins' directory
	if (!::PathFileExists(GetMuleDirectory(EMULE_SKINDIR)) && !::CreateDirectory(GetMuleDirectory(EMULE_SKINDIR), 0))
		m_strSkinProfileDir = GetDefaultDirectory(EMULE_SKINDIR, true); // will also try to create it if needed

	// Create 'toolbars' directory
	if (!::PathFileExists(GetMuleDirectory(EMULE_TOOLBARDIR)) && !::CreateDirectory(GetMuleDirectory(EMULE_TOOLBARDIR), 0))
		m_sToolbarBitmapFolder = GetDefaultDirectory(EMULE_TOOLBARDIR, true); // will also try to create it if needed;
}

void CPreferences::Uninit()
{
	for (INT_PTR i = catArr.GetCount(); --i >= 0;) {
		delete catArr[i];
		catArr.RemoveAt(i);
	}
}

void CPreferences::SetStandardValues()
{
	WINDOWPLACEMENT defaultWPM;
	defaultWPM.length = sizeof(WINDOWPLACEMENT);
	defaultWPM.rcNormalPosition.left = 10;
	defaultWPM.rcNormalPosition.top = 10;
	defaultWPM.rcNormalPosition.right = 700;
	defaultWPM.rcNormalPosition.bottom = 500;
	// Start the main window maximized on first launch, while keeping the restore rectangle
	// as the fallback size once the user switches back to a normal window state.
	defaultWPM.showCmd = SW_SHOWMAXIMIZED;
	EmuleWindowPlacement = defaultWPM;
}

#pragma warning(push)
#pragma warning(disable:4774)
bool CPreferences::IsTempFile(const CString &rstrDirectory, const CString &rstrName)
{
	bool bFound = false;
	for (INT_PTR i = tempdir.GetCount(); --i >= 0;)
		if (EqualPaths(rstrDirectory, GetTempDir(i))) {
			bFound = true; //OK, found a directory
			break;
		}

	if (!bFound) //not found - not a tempfile...
		return false;

	// do not share a file from the temp directory, if it matches one of the following patterns
	CString strNameLower(rstrName);
	strNameLower.MakeLower();
	strNameLower += _T('|'); // append an EOS character which we can query for
	static LPCTSTR const _apszNotSharedExts[] = {
		_T("%u.part") _T("%c"),
		_T("%u.part.met") _T("%c"),
		_T("%u.part.met") PARTMET_BAK_EXT _T("%c"),
		_T("%u.part.met") PARTMET_TMP_EXT _T("%c")
	};
	for (unsigned i = 0; i < _countof(_apszNotSharedExts); ++i) {
		UINT uNum;
		TCHAR iChar;
		// "misuse" the 'scanf' function for a very simple pattern scanning.
		if (_stscanf(strNameLower, _apszNotSharedExts[i], &uNum, &iChar) == 2 && iChar == _T('|'))
			return true;
	}

	return false;
}
#pragma warning(pop)

uint32 CPreferences::GetMaxDownload()
{
	return (uint32)(GetMaxDownloadInBytesPerSec() / 1024);
}

uint64 CPreferences::GetMaxDownloadInBytesPerSec(bool dynamic)
{
	//don't be a Lam3r :)
	UNREFERENCED_PARAMETER(dynamic);
	uint64 maxup = GetMaxUpload() * 1024ull;

	uint64 maxdown = m_maxdownload * 1024ull;
	if (maxup >= 20 * 1024)
		return maxdown;

	uint32 coef;
	if (maxup < 4 * 1024)
		coef = 3;
	else if (maxup < 10 * 1024)
		coef = 4;
	else
		coef = 5;

	return min(coef * maxup, maxdown);
}

// -khaos--+++> A whole bunch of methods! Keep going until you reach the end tag.
void CPreferences::SaveStats(int bBackUp)
{
	// This function saves all of the new statistics in my addon.  It is also used to
	// save backups for the Reset Stats function, and the Restore Stats function (Which is actually LoadStats)
	// bBackUp = 0: DEFAULT; save to statistics.ini
	// bBackUp = 1: Save to statbkup.ini, which is used to restore after a reset
	// bBackUp = 2: Save to statbkuptmp.ini, which is temporarily created during a restore and then renamed to statbkup.ini

	LPCTSTR p;
	if (bBackUp == 1)
		p = _T("statbkup.ini");
	else if (bBackUp == 2)
		p = _T("statbkuptmp.ini");
	else
		p = _T("statistics.ini");
	const CString &strFullPath(GetMuleDirectory(EMULE_CONFIGDIR) + p);

	CIni ini(strFullPath, _T("Statistics"));

	// Save cumulative statistics to statistics.ini, going in the order they appear in CStatisticsDlg::ShowStatistics.
	// We do NOT SET the values in prefs struct here.

	// Save Cum Down Data
	ini.WriteUInt64(_T("TotalDownloadedBytes"), theStats.sessionReceivedBytes + GetTotalDownloaded());
	ini.WriteInt(_T("DownSuccessfulSessions"), cumDownSuccessfulSessions);
	ini.WriteInt(_T("DownFailedSessions"), cumDownFailedSessions);
	ini.WriteInt(_T("DownAvgTime"), GetDownC_AvgTime()); //never needed this
	ini.WriteUInt64(_T("LostFromCorruption"), cumLostFromCorruption + sesLostFromCorruption);
	ini.WriteUInt64(_T("SavedFromCompression"), sesSavedFromCompression + cumSavedFromCompression);
	ini.WriteInt(_T("PartsSavedByICH"), cumPartsSavedByICH + sesPartsSavedByICH);

	ini.WriteUInt64(_T("DownData_EDONKEY"), GetCumDownData_EDONKEY());
	ini.WriteUInt64(_T("DownData_EDONKEYHYBRID"), GetCumDownData_EDONKEYHYBRID());
	ini.WriteUInt64(_T("DownData_EMULE"), GetCumDownData_EMULE());
	ini.WriteUInt64(_T("DownData_MLDONKEY"), GetCumDownData_MLDONKEY());
	ini.WriteUInt64(_T("DownData_LMULE"), GetCumDownData_EMULECOMPAT());
	ini.WriteUInt64(_T("DownData_AMULE"), GetCumDownData_AMULE());
	ini.WriteUInt64(_T("DownData_SHAREAZA"), GetCumDownData_SHAREAZA());
	ini.WriteUInt64(_T("DownData_URL"), GetCumDownData_URL());
	ini.WriteUInt64(_T("DownDataPort_4662"), GetCumDownDataPort_4662());
	ini.WriteUInt64(_T("DownDataPort_OTHER"), GetCumDownDataPort_OTHER());

	ini.WriteUInt64(_T("DownOverheadTotal"), theStats.GetDownDataOverheadFileRequest()
		+ theStats.GetDownDataOverheadSourceExchange()
		+ theStats.GetDownDataOverheadServer()
		+ theStats.GetDownDataOverheadKad()
		+ theStats.GetDownDataOverheadOther()
		+ GetDownOverheadTotal());
	ini.WriteUInt64(_T("DownOverheadFileReq"), theStats.GetDownDataOverheadFileRequest() + GetDownOverheadFileReq());
	ini.WriteUInt64(_T("DownOverheadSrcEx"), theStats.GetDownDataOverheadSourceExchange() + GetDownOverheadSrcEx());
	ini.WriteUInt64(_T("DownOverheadServer"), theStats.GetDownDataOverheadServer() + GetDownOverheadServer());
	ini.WriteUInt64(_T("DownOverheadKad"), theStats.GetDownDataOverheadKad() + GetDownOverheadKad());

	ini.WriteUInt64(_T("DownOverheadTotalPackets"), theStats.GetDownDataOverheadFileRequestPackets()
		+ theStats.GetDownDataOverheadSourceExchangePackets()
		+ theStats.GetDownDataOverheadServerPackets()
		+ theStats.GetDownDataOverheadKadPackets()
		+ theStats.GetDownDataOverheadOtherPackets()
		+ GetDownOverheadTotalPackets());
	ini.WriteUInt64(_T("DownOverheadFileReqPackets"), theStats.GetDownDataOverheadFileRequestPackets() + GetDownOverheadFileReqPackets());
	ini.WriteUInt64(_T("DownOverheadSrcExPackets"), theStats.GetDownDataOverheadSourceExchangePackets() + GetDownOverheadSrcExPackets());
	ini.WriteUInt64(_T("DownOverheadServerPackets"), theStats.GetDownDataOverheadServerPackets() + GetDownOverheadServerPackets());
	ini.WriteUInt64(_T("DownOverheadKadPackets"), theStats.GetDownDataOverheadKadPackets() + GetDownOverheadKadPackets());

	// Save Cumulative Upline Statistics
	ini.WriteUInt64(_T("TotalUploadedBytes"), theStats.sessionSentBytes + GetTotalUploaded());
	ini.WriteInt(_T("UpSuccessfulSessions"), theApp.uploadqueue->GetSuccessfullUpCount() + GetUpSuccessfulSessions());
	ini.WriteInt(_T("UpFailedSessions"), theApp.uploadqueue->GetFailedUpCount() + GetUpFailedSessions());
	ini.WriteInt(_T("UpAvgTime"), GetUpAvgTime()); //never needed this
	ini.WriteUInt64(_T("UpData_EDONKEY"), GetCumUpData_EDONKEY());
	ini.WriteUInt64(_T("UpData_EDONKEYHYBRID"), GetCumUpData_EDONKEYHYBRID());
	ini.WriteUInt64(_T("UpData_EMULE"), GetCumUpData_EMULE());
	ini.WriteUInt64(_T("UpData_MLDONKEY"), GetCumUpData_MLDONKEY());
	ini.WriteUInt64(_T("UpData_LMULE"), GetCumUpData_EMULECOMPAT());
	ini.WriteUInt64(_T("UpData_AMULE"), GetCumUpData_AMULE());
	ini.WriteUInt64(_T("UpData_SHAREAZA"), GetCumUpData_SHAREAZA());
	ini.WriteUInt64(_T("UpDataPort_4662"), GetCumUpDataPort_4662());
	ini.WriteUInt64(_T("UpDataPort_OTHER"), GetCumUpDataPort_OTHER());
	ini.WriteUInt64(_T("UpData_File"), GetCumUpData_File());
	ini.WriteUInt64(_T("UpData_Partfile"), GetCumUpData_Partfile());

	ini.WriteUInt64(_T("UpOverheadTotal"), theStats.GetUpDataOverheadFileRequest()
		+ theStats.GetUpDataOverheadSourceExchange()
		+ theStats.GetUpDataOverheadServer()
		+ theStats.GetUpDataOverheadKad()
		+ theStats.GetUpDataOverheadOther()
		+ GetUpOverheadTotal());
	ini.WriteUInt64(_T("UpOverheadFileReq"), theStats.GetUpDataOverheadFileRequest() + GetUpOverheadFileReq());
	ini.WriteUInt64(_T("UpOverheadSrcEx"), theStats.GetUpDataOverheadSourceExchange() + GetUpOverheadSrcEx());
	ini.WriteUInt64(_T("UpOverheadServer"), theStats.GetUpDataOverheadServer() + GetUpOverheadServer());
	ini.WriteUInt64(_T("UpOverheadKad"), theStats.GetUpDataOverheadKad() + GetUpOverheadKad());

	ini.WriteUInt64(_T("UpOverheadTotalPackets"), theStats.GetUpDataOverheadFileRequestPackets()
		+ theStats.GetUpDataOverheadSourceExchangePackets()
		+ theStats.GetUpDataOverheadServerPackets()
		+ theStats.GetUpDataOverheadKadPackets()
		+ theStats.GetUpDataOverheadOtherPackets()
		+ GetUpOverheadTotalPackets());
	ini.WriteUInt64(_T("UpOverheadFileReqPackets"), theStats.GetUpDataOverheadFileRequestPackets() + GetUpOverheadFileReqPackets());
	ini.WriteUInt64(_T("UpOverheadSrcExPackets"), theStats.GetUpDataOverheadSourceExchangePackets() + GetUpOverheadSrcExPackets());
	ini.WriteUInt64(_T("UpOverheadServerPackets"), theStats.GetUpDataOverheadServerPackets() + GetUpOverheadServerPackets());
	ini.WriteUInt64(_T("UpOverheadKadPackets"), theStats.GetUpDataOverheadKadPackets() + GetUpOverheadKadPackets());

	// Save Cumulative Connection Statistics

	// Download Rate Average
	float tempRate = theStats.GetAvgDownloadRate(AVG_TOTAL);
	ini.WriteFloat(_T("ConnAvgDownRate"), tempRate);

	// Max Download Rate Average
	if (tempRate > GetConnMaxAvgDownRate())
		SetConnMaxAvgDownRate(tempRate);
	ini.WriteFloat(_T("ConnMaxAvgDownRate"), GetConnMaxAvgDownRate());

	// Max Download Rate
	tempRate = theApp.downloadqueue->GetDatarate() / 1024.0f;
	if (tempRate > GetConnMaxDownRate())
		SetConnMaxDownRate(tempRate);
	ini.WriteFloat(_T("ConnMaxDownRate"), GetConnMaxDownRate());

	// Upload Rate Average
	tempRate = theStats.GetAvgUploadRate(AVG_TOTAL);
	ini.WriteFloat(_T("ConnAvgUpRate"), tempRate);

	// Max Upload Rate Average
	if (tempRate > GetConnMaxAvgUpRate())
		SetConnMaxAvgUpRate(tempRate);
	ini.WriteFloat(_T("ConnMaxAvgUpRate"), GetConnMaxAvgUpRate());

	// Max Upload Rate
	tempRate = theApp.uploadqueue->GetDatarate() / 1024.0f;
	if (tempRate > GetConnMaxUpRate())
		SetConnMaxUpRate(tempRate);
	ini.WriteFloat(_T("ConnMaxUpRate"), GetConnMaxUpRate());

	// Overall Run Time
	ini.WriteInt(_T("ConnRunTime"), (UINT)((::GetTickCount() - theStats.starttime) / SEC2MS(1) + GetConnRunTime()));

	// Number of Reconnects
	ini.WriteInt(_T("ConnNumReconnects"), GetConnNumReconnects() + theStats.reconnects - static_cast<uint32>(theStats.reconnects > 0));

	// Average Connections
	if (theApp.serverconnect->IsConnected())
		ini.WriteInt(_T("ConnAvgConnections"), (UINT)((theApp.listensocket->GetAverageConnections() + cumConnAvgConnections) / 2));

	// Peak Connections
	if (theApp.listensocket->GetPeakConnections() > cumConnPeakConnections)
		cumConnPeakConnections = theApp.listensocket->GetPeakConnections();
	ini.WriteInt(_T("ConnPeakConnections"), cumConnPeakConnections);

	// Max Connection Limit Reached
	if (theApp.listensocket->GetMaxConnectionReached() > 0)
		ini.WriteInt(_T("ConnMaxConnLimitReached"), theApp.listensocket->GetMaxConnectionReached() + cumConnMaxConnLimitReached);

	// Time Stuff...
	ini.WriteInt(_T("ConnTransferTime"), GetConnTransferTime() + theStats.GetTransferTime());
	ini.WriteInt(_T("ConnUploadTime"), GetConnUploadTime() + theStats.GetUploadTime());
	ini.WriteInt(_T("ConnDownloadTime"), GetConnDownloadTime() + theStats.GetDownloadTime());
	ini.WriteInt(_T("ConnServerDuration"), GetConnServerDuration() + theStats.GetServerDuration());

	// Compare and Save Server Records
	uint32 servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile;
	float servocc;
	theApp.serverlist->GetStatus(servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile, servocc);

	if (servtotal - servfail > cumSrvrsMostWorkingServers)
		cumSrvrsMostWorkingServers = servtotal - servfail;
	ini.WriteInt(_T("SrvrsMostWorkingServers"), cumSrvrsMostWorkingServers);

	if (servtuser > cumSrvrsMostUsersOnline)
		cumSrvrsMostUsersOnline = servtuser;
	ini.WriteInt(_T("SrvrsMostUsersOnline"), cumSrvrsMostUsersOnline);

	if (servtfile > cumSrvrsMostFilesAvail)
		cumSrvrsMostFilesAvail = servtfile;
	ini.WriteInt(_T("SrvrsMostFilesAvail"), cumSrvrsMostFilesAvail);

	// Compare and Save Shared File Records
	if ((uint32)theApp.sharedfiles->GetCount() > cumSharedMostFilesShared)
		cumSharedMostFilesShared = (uint32)theApp.sharedfiles->GetCount();
	ini.WriteInt(_T("SharedMostFilesShared"), cumSharedMostFilesShared);

	uint64 bytesLargestFile = 0;
	uint64 allsize = theApp.sharedfiles->GetDatasize(bytesLargestFile);
	if (allsize > cumSharedLargestShareSize)
		cumSharedLargestShareSize = allsize;
	ini.WriteUInt64(_T("SharedLargestShareSize"), cumSharedLargestShareSize);
	if (bytesLargestFile > cumSharedLargestFileSize)
		cumSharedLargestFileSize = bytesLargestFile;
	ini.WriteUInt64(_T("SharedLargestFileSize"), cumSharedLargestFileSize);

	if (theApp.sharedfiles->GetCount() != 0) {
		uint64 tempint = allsize / theApp.sharedfiles->GetCount();
		if (tempint > cumSharedLargestAvgFileSize)
			cumSharedLargestAvgFileSize = tempint;
	}

	ini.WriteUInt64(_T("SharedLargestAvgFileSize"), cumSharedLargestAvgFileSize);
	ini.WriteInt(_T("statsDateTimeLastReset"), (int)stat_datetimeLastReset);

	// If we are saving a back-up or a temporary back-up, return now.
	//if (bBackUp != 0)
	//	return;
}

void CPreferences::SetRecordStructMembers()
{
	// The purpose of this function is to be called from CStatisticsDlg::ShowStatistics()
	// This was easier than making a bunch of functions to interface with the record
	// members of the prefs struct from ShowStatistics.

	// This function is going to compare current values with previously saved records, and if
	// the current values are greater, the corresponding member of prefs will be updated.
	// We will not write to INI here, because this code is going to be called a lot more often
	// than SaveStats()  - Khaos

	// Servers
	uint32 servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile;
	float servocc;
	theApp.serverlist->GetStatus(servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile, servocc);
	if ((servtotal - servfail) > cumSrvrsMostWorkingServers)
		cumSrvrsMostWorkingServers = (servtotal - servfail);
	if (servtuser > cumSrvrsMostUsersOnline)
		cumSrvrsMostUsersOnline = servtuser;
	if (servtfile > cumSrvrsMostFilesAvail)
		cumSrvrsMostFilesAvail = servtfile;

	// Shared Files
	if ((uint32)theApp.sharedfiles->GetCount() > cumSharedMostFilesShared)
		cumSharedMostFilesShared = (uint32)theApp.sharedfiles->GetCount();
	uint64 bytesLargestFile = 0;
	uint64 allsize = theApp.sharedfiles->GetDatasize(bytesLargestFile);
	if (allsize > cumSharedLargestShareSize)
		cumSharedLargestShareSize = allsize;
	if (bytesLargestFile > cumSharedLargestFileSize)
		cumSharedLargestFileSize = bytesLargestFile;
	if (theApp.sharedfiles->GetCount() != 0) {
		uint64 tempint = allsize / theApp.sharedfiles->GetCount();
		if (tempint > cumSharedLargestAvgFileSize)
			cumSharedLargestAvgFileSize = tempint;
	}
} // SetRecordStructMembers()

void CPreferences::SaveCompletedDownloadsStat()
{
	// This function saves the values for the completed
	// download members to INI.  It is called from
	// CPartfile::PerformFileComplete...   - Khaos

	CIni ini(GetMuleDirectory(EMULE_CONFIGDIR) + _T("statistics.ini"), _T("Statistics"));

	ini.WriteInt(_T("DownCompletedFiles"), GetDownCompletedFiles());
	ini.WriteInt(_T("DownSessionCompletedFiles"), GetDownSessionCompletedFiles());
} // SaveCompletedDownloadsStat()

void CPreferences::Add2SessionTransferData(UINT uClientID, UINT uClientPort, BOOL bFromPF,
	BOOL bUpDown, uint64 bytes, bool sentToFriend)
{
	//	This function adds the transferred bytes to the appropriate variables,
	//	as well as to the totals for all clients. - Khaos
	//	PARAMETERS:
	//	uClientID - The identifier for which client software sent or received this data, e.g. SO_EMULE
	//	uClientPort - The remote port of the client that sent or received this data, e.g. 4662
	//	bFromPF - Applies only to uploads.  True is from partfile, False is from non-partfile.
	//	bUpDown - True is Up, False is Down
	//	bytes - Number of bytes sent by the client.  Subtract header before calling.

	if (bUpDown) {
		//	Upline Data
		switch (uClientID) {
			// Update session client breakdown stats for sent bytes...
		case SO_EMULE:
		case SO_OLDEMULE:
			sesUpData_EMULE += bytes;
			break;
		case SO_EDONKEYHYBRID:
			sesUpData_EDONKEYHYBRID += bytes;
			break;
		case SO_EDONKEY:
			sesUpData_EDONKEY += bytes;
			break;
		case SO_MLDONKEY:
			sesUpData_MLDONKEY += bytes;
			break;
		case SO_AMULE:
			sesUpData_AMULE += bytes;
			break;
		case SO_SHAREAZA:
			sesUpData_SHAREAZA += bytes;
			break;
		case SO_CDONKEY:
		case SO_LPHANT:
		case SO_XMULE:
			sesUpData_EMULECOMPAT += bytes;
		}

		switch (uClientPort) {
			// Update session port breakdown stats for sent bytes...
		case 4662:
			sesUpDataPort_4662 += bytes;
			break;
		default:
			sesUpDataPort_OTHER += bytes;
		}

		if (bFromPF)
			sesUpData_Partfile += bytes;
		else
			sesUpData_File += bytes;

		//	Add to our total for sent bytes...
		theApp.UpdateSentBytes(bytes, sentToFriend);
	} else {
		// Downline Data
		switch (uClientID) {
			// Update session client breakdown stats for received bytes...
		case SO_EMULE:
		case SO_OLDEMULE:
			sesDownData_EMULE += bytes;
			break;
		case SO_EDONKEYHYBRID:
			sesDownData_EDONKEYHYBRID += bytes;
			break;
		case SO_EDONKEY:
			sesDownData_EDONKEY += bytes;
			break;
		case SO_MLDONKEY:
			sesDownData_MLDONKEY += bytes;
			break;
		case SO_AMULE:
			sesDownData_AMULE += bytes;
			break;
		case SO_SHAREAZA:
			sesDownData_SHAREAZA += bytes;
			break;
		case SO_CDONKEY:
		case SO_LPHANT:
		case SO_XMULE:
			sesDownData_EMULECOMPAT += bytes;
			break;
		case SO_URL:
			sesDownData_URL += bytes;
		}

		switch (uClientPort) {
			// Update session port breakdown stats for received bytes...
			// For now we are only going to break it down by default and non-default.
			// A statistical analysis of all data sent from every single port/domain is
			// beyond the scope of this add-on.
		case 4662:
			sesDownDataPort_4662 += bytes;
			break;
		default:
			sesDownDataPort_OTHER += bytes;
			break;
		}

		//	Add to our total for received bytes...
		theApp.UpdateReceivedBytes(bytes);
	}
}

// Reset Statistics by Khaos

void CPreferences::ResetCumulativeStatistics()
{

// Save a backup so that we can undo this action
	SaveStats(1);

	// SET ALL CUMULATIVE STAT VALUES TO 0  :'-(

	totalDownloadedBytes = 0;
	totalUploadedBytes = 0;
	cumDownOverheadTotal = 0;
	cumDownOverheadFileReq = 0;
	cumDownOverheadSrcEx = 0;
	cumDownOverheadServer = 0;
	cumDownOverheadKad = 0;
	cumDownOverheadTotalPackets = 0;
	cumDownOverheadFileReqPackets = 0;
	cumDownOverheadSrcExPackets = 0;
	cumDownOverheadServerPackets = 0;
	cumDownOverheadKadPackets = 0;
	cumUpOverheadTotal = 0;
	cumUpOverheadFileReq = 0;
	cumUpOverheadSrcEx = 0;
	cumUpOverheadServer = 0;
	cumUpOverheadKad = 0;
	cumUpOverheadTotalPackets = 0;
	cumUpOverheadFileReqPackets = 0;
	cumUpOverheadSrcExPackets = 0;
	cumUpOverheadServerPackets = 0;
	cumUpOverheadKadPackets = 0;
	cumUpSuccessfulSessions = 0;
	cumUpFailedSessions = 0;
	cumUpAvgTime = 0;
	cumUpData_EDONKEY = 0;
	cumUpData_EDONKEYHYBRID = 0;
	cumUpData_EMULE = 0;
	cumUpData_MLDONKEY = 0;
	cumUpData_AMULE = 0;
	cumUpData_EMULECOMPAT = 0;
	cumUpData_SHAREAZA = 0;
	cumUpDataPort_4662 = 0;
	cumUpDataPort_OTHER = 0;
	cumDownCompletedFiles = 0;
	cumDownSuccessfulSessions = 0;
	cumDownFailedSessions = 0;
	cumDownAvgTime = 0;
	cumLostFromCorruption = 0;
	cumSavedFromCompression = 0;
	cumPartsSavedByICH = 0;
	cumDownData_EDONKEY = 0;
	cumDownData_EDONKEYHYBRID = 0;
	cumDownData_EMULE = 0;
	cumDownData_MLDONKEY = 0;
	cumDownData_AMULE = 0;
	cumDownData_EMULECOMPAT = 0;
	cumDownData_SHAREAZA = 0;
	cumDownData_URL = 0;
	cumDownDataPort_4662 = 0;
	cumDownDataPort_OTHER = 0;
	cumConnAvgDownRate = 0;
	cumConnMaxAvgDownRate = 0;
	cumConnMaxDownRate = 0;
	cumConnAvgUpRate = 0;
	cumConnRunTime = 0;
	cumConnNumReconnects = 0;
	cumConnAvgConnections = 0;
	cumConnMaxConnLimitReached = 0;
	cumConnPeakConnections = 0;
	cumConnDownloadTime = 0;
	cumConnUploadTime = 0;
	cumConnTransferTime = 0;
	cumConnServerDuration = 0;
	cumConnMaxAvgUpRate = 0;
	cumConnMaxUpRate = 0;
	cumSrvrsMostWorkingServers = 0;
	cumSrvrsMostUsersOnline = 0;
	cumSrvrsMostFilesAvail = 0;
	cumSharedMostFilesShared = 0;
	cumSharedLargestShareSize = 0;
	cumSharedLargestAvgFileSize = 0;

	// Set the time of last reset...
	time_t timeNow;
	time(&timeNow);
	stat_datetimeLastReset = timeNow;

	// Save the reset stats
	SaveStats();
	theApp.emuledlg->statisticswnd->ShowStatistics(true);
}


// Load Statistics
// This used to be integrated in LoadPreferences, but it has been altered
// so that it can be used to load the backup created when the stats are reset.
// Last Modified: 2-22-03 by Khaos
bool CPreferences::LoadStats(int loadBackUp)
{
	// loadBackUp is 0 by default
	// loadBackUp = 0: Load the stats normally like we used to do in LoadPreferences
	// loadBackUp = 1: Load the stats from statbkup.ini and create a backup of the current stats.  Also, do not initialize session variables.
	const CString &sConfDir(GetMuleDirectory(EMULE_CONFIGDIR));
	CFileFind findBackUp;

	CString sINI(sConfDir);
	switch (loadBackUp) {
	case 1:
		sINI += _T("statbkup.ini");
		if (!findBackUp.FindFile(sINI))
			return false;
		SaveStats(2); // Save our temp backup of current values to statbkuptmp.ini, we will be renaming it at the end of this function.
		break;
	case 0:
	default:
		// for transition...
		if (::PathFileExists(sINI + _T("statistics.ini")))
			sINI += _T("statistics.ini");
		else
			sINI += _T("preferences.ini");
	}

	BOOL fileex = ::PathFileExists(sINI);
	CIni ini(sINI, _T("Statistics"));

	totalDownloadedBytes = ini.GetUInt64(_T("TotalDownloadedBytes"));
	totalUploadedBytes = ini.GetUInt64(_T("TotalUploadedBytes"));

	// Load stats for cumulative downline overhead
	cumDownOverheadTotal = ini.GetUInt64(_T("DownOverheadTotal"));
	cumDownOverheadFileReq = ini.GetUInt64(_T("DownOverheadFileReq"));
	cumDownOverheadSrcEx = ini.GetUInt64(_T("DownOverheadSrcEx"));
	cumDownOverheadServer = ini.GetUInt64(_T("DownOverheadServer"));
	cumDownOverheadKad = ini.GetUInt64(_T("DownOverheadKad"));
	cumDownOverheadTotalPackets = ini.GetUInt64(_T("DownOverheadTotalPackets"));
	cumDownOverheadFileReqPackets = ini.GetUInt64(_T("DownOverheadFileReqPackets"));
	cumDownOverheadSrcExPackets = ini.GetUInt64(_T("DownOverheadSrcExPackets"));
	cumDownOverheadServerPackets = ini.GetUInt64(_T("DownOverheadServerPackets"));
	cumDownOverheadKadPackets = ini.GetUInt64(_T("DownOverheadKadPackets"));

	// Load stats for cumulative upline overhead
	cumUpOverheadTotal = ini.GetUInt64(_T("UpOverheadTotal"));
	cumUpOverheadFileReq = ini.GetUInt64(_T("UpOverheadFileReq"));
	cumUpOverheadSrcEx = ini.GetUInt64(_T("UpOverheadSrcEx"));
	cumUpOverheadServer = ini.GetUInt64(_T("UpOverheadServer"));
	cumUpOverheadKad = ini.GetUInt64(_T("UpOverheadKad"));
	cumUpOverheadTotalPackets = ini.GetUInt64(_T("UpOverheadTotalPackets"));
	cumUpOverheadFileReqPackets = ini.GetUInt64(_T("UpOverheadFileReqPackets"));
	cumUpOverheadSrcExPackets = ini.GetUInt64(_T("UpOverheadSrcExPackets"));
	cumUpOverheadServerPackets = ini.GetUInt64(_T("UpOverheadServerPackets"));
	cumUpOverheadKadPackets = ini.GetUInt64(_T("UpOverheadKadPackets"));

	// Load stats for cumulative upline data
	cumUpSuccessfulSessions = ini.GetInt(_T("UpSuccessfulSessions"));
	cumUpFailedSessions = ini.GetInt(_T("UpFailedSessions"));
	cumUpAvgTime = ini.GetInt(_T("UpAvgTime"));

	// Load cumulative client breakdown stats for sent bytes
	cumUpData_EDONKEY = ini.GetUInt64(_T("UpData_EDONKEY"));
	cumUpData_EDONKEYHYBRID = ini.GetUInt64(_T("UpData_EDONKEYHYBRID"));
	cumUpData_EMULE = ini.GetUInt64(_T("UpData_EMULE"));
	cumUpData_MLDONKEY = ini.GetUInt64(_T("UpData_MLDONKEY"));
	cumUpData_EMULECOMPAT = ini.GetUInt64(_T("UpData_LMULE"));
	cumUpData_AMULE = ini.GetUInt64(_T("UpData_AMULE"));
	cumUpData_SHAREAZA = ini.GetUInt64(_T("UpData_SHAREAZA"));

	// Load cumulative port breakdown stats for sent bytes
	cumUpDataPort_4662 = ini.GetUInt64(_T("UpDataPort_4662"));
	cumUpDataPort_OTHER = ini.GetUInt64(_T("UpDataPort_OTHER"));

	// Load cumulative source breakdown stats for sent bytes
	cumUpData_File = ini.GetUInt64(_T("UpData_File"));
	cumUpData_Partfile = ini.GetUInt64(_T("UpData_Partfile"));

	// Load stats for cumulative downline data
	cumDownCompletedFiles = ini.GetInt(_T("DownCompletedFiles"));
	cumDownSuccessfulSessions = ini.GetInt(_T("DownSuccessfulSessions"));
	cumDownFailedSessions = ini.GetInt(_T("DownFailedSessions"));
	cumDownAvgTime = ini.GetInt(_T("DownAvgTime")); //never needed this

	// Cumulative statistics for saved due to compression/lost due to corruption
	cumLostFromCorruption = ini.GetUInt64(_T("LostFromCorruption"));
	cumSavedFromCompression = ini.GetUInt64(_T("SavedFromCompression"));
	cumPartsSavedByICH = ini.GetInt(_T("PartsSavedByICH"));

	// Load cumulative client breakdown stats for received bytes
	cumDownData_EDONKEY = ini.GetUInt64(_T("DownData_EDONKEY"));
	cumDownData_EDONKEYHYBRID = ini.GetUInt64(_T("DownData_EDONKEYHYBRID"));
	cumDownData_EMULE = ini.GetUInt64(_T("DownData_EMULE"));
	cumDownData_MLDONKEY = ini.GetUInt64(_T("DownData_MLDONKEY"));
	cumDownData_EMULECOMPAT = ini.GetUInt64(_T("DownData_LMULE"));
	cumDownData_AMULE = ini.GetUInt64(_T("DownData_AMULE"));
	cumDownData_SHAREAZA = ini.GetUInt64(_T("DownData_SHAREAZA"));
	cumDownData_URL = ini.GetUInt64(_T("DownData_URL"));

	// Load cumulative port breakdown stats for received bytes
	cumDownDataPort_4662 = ini.GetUInt64(_T("DownDataPort_4662"));
	cumDownDataPort_OTHER = ini.GetUInt64(_T("DownDataPort_OTHER"));

	// Load stats for cumulative connection data
	cumConnAvgDownRate = ini.GetFloat(_T("ConnAvgDownRate"));
	cumConnMaxAvgDownRate = ini.GetFloat(_T("ConnMaxAvgDownRate"));
	cumConnMaxDownRate = ini.GetFloat(_T("ConnMaxDownRate"));
	cumConnAvgUpRate = ini.GetFloat(_T("ConnAvgUpRate"));
	cumConnMaxAvgUpRate = ini.GetFloat(_T("ConnMaxAvgUpRate"));
	cumConnMaxUpRate = ini.GetFloat(_T("ConnMaxUpRate"));
	cumConnRunTime = ini.GetInt(_T("ConnRunTime"));
	cumConnTransferTime = ini.GetInt(_T("ConnTransferTime"));
	cumConnDownloadTime = ini.GetInt(_T("ConnDownloadTime"));
	cumConnUploadTime = ini.GetInt(_T("ConnUploadTime"));
	cumConnServerDuration = ini.GetInt(_T("ConnServerDuration"));
	cumConnNumReconnects = ini.GetInt(_T("ConnNumReconnects"));
	cumConnAvgConnections = ini.GetInt(_T("ConnAvgConnections"));
	cumConnMaxConnLimitReached = ini.GetInt(_T("ConnMaxConnLimitReached"));
	cumConnPeakConnections = ini.GetInt(_T("ConnPeakConnections"));

	// Load date/time of last reset
	stat_datetimeLastReset = ini.GetInt(_T("statsDateTimeLastReset"));

	// Smart Load For Restores - Don't overwrite records that are greater than the backed up ones
	if (loadBackUp == 1) {
		// Load records for servers / network
		if ((uint32)ini.GetInt(_T("SrvrsMostWorkingServers")) > cumSrvrsMostWorkingServers)
			cumSrvrsMostWorkingServers = ini.GetInt(_T("SrvrsMostWorkingServers"));

		if ((uint32)ini.GetInt(_T("SrvrsMostUsersOnline")) > cumSrvrsMostUsersOnline)
			cumSrvrsMostUsersOnline = ini.GetInt(_T("SrvrsMostUsersOnline"));

		if ((uint32)ini.GetInt(_T("SrvrsMostFilesAvail")) > cumSrvrsMostFilesAvail)
			cumSrvrsMostFilesAvail = ini.GetInt(_T("SrvrsMostFilesAvail"));

		// Load records for shared files
		if ((uint32)ini.GetInt(_T("SharedMostFilesShared")) > cumSharedMostFilesShared)
			cumSharedMostFilesShared = ini.GetInt(_T("SharedMostFilesShared"));

		uint64 temp64 = ini.GetUInt64(_T("SharedLargestShareSize"));
		if (temp64 > cumSharedLargestShareSize)
			cumSharedLargestShareSize = temp64;

		temp64 = ini.GetUInt64(_T("SharedLargestAvgFileSize"));
		if (temp64 > cumSharedLargestAvgFileSize)
			cumSharedLargestAvgFileSize = temp64;

		temp64 = ini.GetUInt64(_T("SharedLargestFileSize"));
		if (temp64 > cumSharedLargestFileSize)
			cumSharedLargestFileSize = temp64;

		// Check to make sure the backup of the values we just overwrote exists.  If so, rename it to the backup file.
		// This allows us to undo a restore, so to speak, just in case we don't like the restored values...
		CString sINIBackUp(sConfDir + _T("statbkuptmp.ini"));
		if (findBackUp.FindFile(sINIBackUp)) {
			::DeleteFile(sINI);				// Remove the backup that we just restored from
			::MoveFile(sINIBackUp, sINI);	// Rename our temporary backup to the normal statbkup.ini filename.
		}

		// Since we know this is a restore, now we should call ShowStatistics to update the data items to the new ones we just loaded.
		// Otherwise user is left waiting around for the tick counter to reach the next automatic update (Depending on setting in prefs)
		theApp.emuledlg->statisticswnd->ShowStatistics();
	} else {
		// Stupid Load -> Just load the values.
		// Load records for servers / network
		cumSrvrsMostWorkingServers = ini.GetInt(_T("SrvrsMostWorkingServers"));
		cumSrvrsMostUsersOnline = ini.GetInt(_T("SrvrsMostUsersOnline"));
		cumSrvrsMostFilesAvail = ini.GetInt(_T("SrvrsMostFilesAvail"));

		// Load records for shared files
		cumSharedMostFilesShared = ini.GetInt(_T("SharedMostFilesShared"));
		cumSharedLargestShareSize = ini.GetUInt64(_T("SharedLargestShareSize"));
		cumSharedLargestAvgFileSize = ini.GetUInt64(_T("SharedLargestAvgFileSize"));
		cumSharedLargestFileSize = ini.GetUInt64(_T("SharedLargestFileSize"));

		// Initialize new session statistic variables...
		sesDownCompletedFiles = 0;

		sesUpData_EDONKEY = 0;
		sesUpData_EDONKEYHYBRID = 0;
		sesUpData_EMULE = 0;
		sesUpData_MLDONKEY = 0;
		sesUpData_AMULE = 0;
		sesUpData_EMULECOMPAT = 0;
		sesUpData_SHAREAZA = 0;
		sesUpDataPort_4662 = 0;
		sesUpDataPort_OTHER = 0;

		sesDownData_EDONKEY = 0;
		sesDownData_EDONKEYHYBRID = 0;
		sesDownData_EMULE = 0;
		sesDownData_MLDONKEY = 0;
		sesDownData_AMULE = 0;
		sesDownData_EMULECOMPAT = 0;
		sesDownData_SHAREAZA = 0;
		sesDownData_URL = 0;
		sesDownDataPort_4662 = 0;
		sesDownDataPort_OTHER = 0;

		sesDownSuccessfulSessions = 0;
		sesDownFailedSessions = 0;
		sesPartsSavedByICH = 0;
	}

	if (!fileex || (stat_datetimeLastReset == 0 && totalDownloadedBytes == 0 && totalUploadedBytes == 0)) {
		time_t timeNow;
		time(&timeNow);
		stat_datetimeLastReset = timeNow;
	}

	return true;
}

// This formats the UTC long value that is saved for stat_datetimeLastReset
// If this value is 0 (Never reset), then it returns Unknown.
CString CPreferences::GetStatsLastResetStr(bool formatLong)
{
	// The format of the returned string depends on formatLong.
	// true: DateTime format from the .ini
	// false: DateTime format from the .ini for the log
	if (GetStatsLastResetLng()) {
		tm *statsReset;
		TCHAR szDateReset[128];
		time_t lastResetDateTime = GetStatsLastResetLng();
		statsReset = localtime(&lastResetDateTime);
		if (statsReset) {
			_tcsftime(szDateReset, _countof(szDateReset), formatLong ? (LPCTSTR)GetDateTimeFormat() : _T("%c"), statsReset);
			if (*szDateReset)
				return CString(szDateReset);
		}
	}
	return GetResString(IDS_UNKNOWN);
}

// <-----khaos-

bool CPreferences::Save()
{
	static LPCTSTR const stmp = _T(".tmp");
	const CString &sConfDir(GetMuleDirectory(EMULE_CONFIGDIR));
	const CString &strPrefPath(sConfDir + strPreferencesDat);
	bool error;

	FILE *preffile = _tfsopen(strPrefPath + stmp, _T("wb"), _SH_DENYWR); //keep contents
	if (preffile) {
		prefsExt->version = PREFFILE_VERSION;
		md4cpy(prefsExt->userhash, userhash);
		prefsExt->EmuleWindowPlacement = EmuleWindowPlacement;
		error = (fwrite(prefsExt, sizeof(Preferences_Ext_Struct), 1, preffile) != 1);
		error |= (fclose(preffile) != 0);
		if (!error)
			error = !MoveFileEx(strPrefPath + stmp, strPrefPath, MOVEFILE_REPLACE_EXISTING);
	} else
		error = true;

	SavePreferences();
	SaveStats();

	const CString &strSharesPath(sConfDir + SHAREDDIRS);
	CStdioFile file;
	if (file.Open(strSharesPath + stmp, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeBinary)) {
		try {
			// write UTF-16LE byte order mark 0xFEFF
			static const WORD wBOM = u'\xFEFF';
			file.Write(&wBOM, sizeof wBOM);
			for (POSITION pos = shareddir_list.GetHeadPosition(); pos != NULL;) {
				file.WriteString(shareddir_list.GetNext(pos));
				file.Write(_T("\r\n"), 2 * sizeof(TCHAR));
			}
			file.Close();
			error |= (MoveFileEx(strSharesPath + stmp, strSharesPath, MOVEFILE_REPLACE_EXISTING) == 0);
		} catch (CFileException *ex) {
			if (thePrefs.GetVerbose())
				AddDebugLogLine(true, _T("Failed to save %s%s"), (LPCTSTR)strSharesPath, (LPCTSTR)CExceptionStrDash(*ex));
			ex->Delete();
		}
	} else
		error = true;

	::CreateDirectory(GetMuleDirectory(EMULE_INCOMINGDIR), 0);
	::CreateDirectory(GetTempDir(), 0);
	return error;
}

void CPreferences::CreateUserHash()
{
	while (isbadhash(userhash)) {
		CryptoPP::AutoSeededRandomPool rng;
		rng.GenerateBlock(userhash, sizeof userhash);
	}
	// mark as emule client. this will be needed in later version
	userhash[5] = 14;	//0x0e
	userhash[14] = 111;	//0x6f
}

/** Modern Windows no longer imposes a practical OS TCP cap for this preference UI. */
UINT CPreferences::GetRecommendedMaxConnections()
{
	return 500;
}

void CPreferences::SavePreferences()
{
	CIni ini(GetConfigFile(), _T("eMule"));
	//---
	ini.WriteString(_T("AppVersion"), theApp.m_strCurVersionLong);
	//---
#ifdef _BETA
	if (m_bBetaNaggingDone)
		ini.WriteString(_T("BetaVersionNotified"), theApp.m_strCurVersionLong);
#endif
#ifdef _DEBUG
	ini.WriteInt(_T("DebugHeap"), m_iDbgHeap);
#endif

	ini.WriteStringUTF8(_T("Nick"), strNick);
	ini.WriteString(_T("IncomingDir"), m_strIncomingDir);

	ini.WriteString(_T("TempDir"), tempdir[0]);
	CString tempdirs;
	for (INT_PTR i = 1; i < tempdir.GetCount(); ++i) {
		if (i > 1)
			tempdirs += _T('|');
		tempdirs += tempdir[i];
	}
	ini.WriteString(_T("TempDirs"), tempdirs);

	ini.WriteInt(_T("BBMaxUpClientsAllowed"), m_maxUpClientsAllowed);
	ini.WriteUInt64(_T("BBSessionMaxTrans"), m_bbSessionMaxTrans);
	ini.WriteUInt64(_T("BBSessionMaxTime"), m_bbSessionMaxTime);
	ini.WriteFloat(_T("BBBoostLowRatioFiles"), m_bbBoostLowRatioFiles);
	ini.WriteFloat(_T("BBBoostLowRatioFilesBy"), m_bbBoostLowRatioFilesBy);
	ini.WriteInt(_T("BBDeboostLowIDs"), m_bbDeboostLowIDs);
	ini.WriteInt(_T("MaxUpload"), SerializeBandwidthLimit(m_maxupload));
	ini.WriteInt(_T("MaxDownload"), SerializeBandwidthLimit(m_maxdownload));
	ini.WriteInt(_T("MaxConnections"), maxconnections);
	ini.WriteInt(_T("MaxHalfConnections"), maxhalfconnections);
	ini.WriteBool(_T("ConditionalTCPAccept"), m_bConditionalTCPAccept);
	ini.WriteInt(_T("Port"), port);
	ini.WriteInt(_T("UDPPort"), udpport);
	ini.WriteString(_T("BindInterface"), m_strBindInterface);
	ini.WriteString(_T("BindInterfaceName"), m_strBindInterfaceName);
	ini.WriteString(_T("BindAddr"), m_strConfiguredBindAddr);
	ini.WriteBool(_T("BlockNetworkWhenBindUnavailableAtStartup"), m_bBlockNetworkWhenBindUnavailableAtStartup);
	ini.WriteBool(_T("RandomizePortsOnStartup"), m_bRandomizePortsOnStartup);
	ini.WriteInt(_T("ServerUDPPort"), nServerUDPPort);
	ini.WriteInt(_T("MaxSourcesPerFile"), maxsourceperfile);
	ini.WriteWORD(_T("Language"), m_wLanguageID);
	ini.WriteInt(_T("SeeShare"), m_iSeeShares);
	ini.WriteInt(_T("ToolTipDelay"), m_iToolDelayTime);
	ini.WriteInt(_T("StatGraphsInterval"), trafficOMeterInterval);
	ini.WriteInt(_T("StatsInterval"), statsInterval);
	ini.WriteBool(_T("StatsFillGraphs"), m_bFillGraphs);
	ini.WriteInt(_T("DeadServerRetry"), m_uDeadServerRetries);
	ini.WriteInt(_T("ServerKeepAliveTimeout"), m_dwServerKeepAliveTimeout);
	ini.WriteInt(_T("ConnectionTimeout"), static_cast<int>(ModernLimits::TimeoutMsToSeconds(m_dwConnectionTimeout)));
	ini.WriteInt(_T("DownloadTimeout"), static_cast<int>(ModernLimits::TimeoutMsToSeconds(m_dwDownloadTimeout)));
	ini.WriteInt(_T("SplitterbarPosition"), splitterbarPosition);
	ini.WriteInt(_T("SplitterbarPositionServer"), splitterbarPositionSvr);
	ini.WriteInt(_T("SplitterbarPositionStat"), splitterbarPositionStat + 1);
	ini.WriteInt(_T("SplitterbarPositionStat_HL"), splitterbarPositionStat_HL + 1);
	ini.WriteInt(_T("SplitterbarPositionStat_HR"), splitterbarPositionStat_HR + 1);
	ini.WriteInt(_T("SplitterbarPositionFriend"), splitterbarPositionFriend);
	ini.WriteInt(_T("SplitterbarPositionShared"), splitterbarPositionShared);
	ini.WriteInt(_T("TransferWnd1"), m_uTransferWnd1);
	ini.WriteInt(_T("TransferWnd2"), m_uTransferWnd2);
	ini.WriteInt(_T("VariousStatisticsMaxValue"), statsMax);
	ini.WriteInt(_T("StatsAverageMinutes"), statsAverageMinutes);
	ini.WriteInt(_T("MaxConnectionsPerFiveSeconds"), MaxConperFive);

	ini.WriteBool(_T("Reconnect"), reconnect);
	ini.WriteBool(_T("Scoresystem"), m_bUseServerPriorities);
	ini.WriteBool(_T("Serverlist"), m_bAutoUpdateServerList);
	if (IsRunningAeroGlassTheme())
		ini.WriteBool(_T("MinToTray_Aero"), mintotray);
	else
		ini.WriteBool(_T("MinToTray"), mintotray);
	ini.WriteBool(_T("PreventStandby"), m_bPreventStandby);
	ini.WriteBool(_T("StoreSearches"), m_bStoreSearches);
	ini.WriteBool(_T("AddServersFromServer"), m_bAddServersFromServer);
	ini.WriteBool(_T("AddServersFromClient"), m_bAddServersFromClients);
	ini.WriteBool(_T("BringToFront"), bringtoforeground);
	ini.WriteBool(_T("TransferDoubleClick"), transferDoubleclick);
	ini.WriteBool(_T("ConfirmExit"), confirmExit);
	ini.WriteBool(_T("FilterBadIPs"), filterLANIPs);
	ini.WriteBool(_T("Autoconnect"), autoconnect);
	ini.WriteBool(_T("OnlineSignature"), onlineSig);
	ini.WriteBool(_T("StartupMinimized"), startMinimized);
	ini.WriteBool(_T("AutoStart"), m_bAutoStart);
	ini.WriteInt(_T("LastMainWndDlgID"), m_iLastMainWndDlgID);
	ini.WriteInt(_T("LastLogPaneID"), m_iLastLogPaneID);
	ini.WriteBool(_T("SafeServerConnect"), m_bSafeServerConnect);
	ini.WriteBool(_T("ShowRatesOnTitle"), showRatesInTitle);
	ini.WriteBool(_T("IndicateRatings"), indicateratings);
	ini.WriteBool(_T("WatchClipboard4ED2kFilelinks"), watchclipboard);
	ini.WriteInt(_T("SearchMethod"), m_iSearchMethod);
	ini.WriteBool(_T("CheckDiskspace"), checkDiskspace);
	ini.WriteInt(_T("MinFreeDiskSpace"), m_uMinFreeDiskSpace);
	ini.WriteBool(_T("SparsePartFiles"), m_bSparsePartFiles);
	ini.WriteBool(_T("ResolveSharedShellLinks"), m_bResolveSharedShellLinks);
	ini.WriteBool(_T("AutoRescanSharedFolders"), m_bAutoRescanSharedFolders);
	ini.WriteInt(_T("AutoRescanSharedFoldersIntervalSec"), max(600u, m_uAutoRescanSharedFoldersIntervalSec));
	ini.WriteBool(_T("AutoShareNewSharedSubdirs"), m_bAutoShareNewSharedSubdirs);
	ini.WriteString(_T("YourHostname"), m_strYourHostname);
	ini.WriteString(_T("NodesDatUpdateUrl"), m_strNodesDatUpdateUrl);
	ini.WriteBool(_T("CheckFileOpen"), m_bCheckFileOpen);
	ini.WriteBool(_T("ShowTaskbarProgress"), m_bShowTaskbarProgress);

	// Barry - New properties...
	ini.WriteBool(_T("AutoConnectStaticOnly"), m_bAutoConnectToStaticServersOnly);
	ini.WriteBool(_T("AutoTakeED2KLinks"), autotakeed2klinks);
	ini.WriteBool(_T("AddNewFilesPaused"), addnewfilespaused);
	ini.WriteInt(_T("3DDepth"), depth3D);
	ini.WriteString(_T("NotifierConfiguration"), notifierConfiguration);
	ini.WriteBool(_T("NotifyOnDownload"), notifierOnDownloadFinished);
	ini.WriteBool(_T("NotifyOnNewDownload"), notifierOnNewDownload);
	ini.WriteBool(_T("NotifyOnChat"), notifierOnChat);
	ini.WriteBool(_T("NotifyOnLog"), notifierOnLog);
	ini.WriteBool(_T("NotifyOnImportantError"), notifierOnImportantError);
	ini.WriteBool(_T("NotifierPopEveryChatMessage"), notifierOnEveryChatMsg);
	ini.WriteInt(_T("NotifierUseSound"), (int)notifierSoundType);
	ini.WriteString(_T("NotifierSoundPath"), notifierSoundFile);

	ini.WriteString(_T("TxtEditor"), m_strTxtEditor);
	ini.WriteString(_T("VideoPlayer"), m_strVideoPlayer);
	ini.WriteString(_T("VideoPlayerArgs"), m_strVideoPlayerArgs);
	ini.WriteString(_T("MessageFilter"), messageFilter);
	ini.WriteString(_T("CommentFilter"), commentFilter);
	ini.WriteString(_T("DateTimeFormat"), GetDateTimeFormat());
	ini.WriteString(_T("DateTimeFormat4Log"), GetDateTimeFormat4Log());
	ini.WriteString(_T("DateTimeFormat4Lists"), m_strDateTimeFormat4Lists);
	ini.WriteString(_T("FilenameCleanups"), filenameCleanups);
	ini.WriteInt(_T("ExtractMetaData"), m_iExtractMetaData);
	ini.WriteBool(_T("MessageEnableSmileys"), m_bMessageEnableSmileys);

	ini.WriteBool(_T("SmartIdCheck"), m_bSmartServerIdCheck);
	ini.WriteBool(_T("Verbose"), m_bVerbose);
	ini.WriteBool(_T("DebugSourceExchange"), m_bDebugSourceExchange);	// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogBannedClients"), m_bLogBannedClients);			// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogRatingDescReceived"), m_bLogRatingDescReceived);// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogSecureIdent"), m_bLogSecureIdent);				// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogFilteredIPs"), m_bLogFilteredIPs);				// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogFileSaving"), m_bLogFileSaving);				// do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogA4AF"), m_bLogA4AF);                           // do *not* use the according 'Get...' function here!
	ini.WriteBool(_T("LogUlDlEvents"), m_bLogUlDlEvents);
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	// following options are for debugging or when using an external debug device viewer only.
	ini.WriteInt(_T("DebugServerTCP"), m_iDebugServerTCPLevel);
	ini.WriteInt(_T("DebugServerUDP"), m_iDebugServerUDPLevel);
	ini.WriteInt(_T("DebugServerSources"), m_iDebugServerSourcesLevel);
	ini.WriteInt(_T("DebugServerSearches"), m_iDebugServerSearchesLevel);
	ini.WriteInt(_T("DebugClientTCP"), m_iDebugClientTCPLevel);
	ini.WriteInt(_T("DebugClientUDP"), m_iDebugClientUDPLevel);
	ini.WriteInt(_T("DebugClientKadUDP"), m_iDebugClientKadUDPLevel);
#endif
	ini.WriteBool(_T("PreviewPrio"), m_bpreviewprio);
	ini.WriteBool(_T("ManualHighPrio"), m_bManualAddedServersHighPriority);
	ini.WriteBool(_T("FullChunkTransfers"), m_btransferfullchunks);
	ini.WriteBool(_T("ShowOverhead"), m_bshowoverhead);
	ini.WriteBool(_T("VideoPreviewBackupped"), m_bMoviePreviewBackup);
	ini.WriteInt(_T("StartNextFile"), m_istartnextfile);

	ini.WriteInt(_T("FileBufferSize"), m_uFileBufferSize);
	ini.WriteInt(_T("UDPReceiveBufferSize"), m_uUDPReceiveBufferSize);
	ini.WriteInt(_T("BigSendBufferSize"), m_uTCPSendBufferSize);
	ini.WriteInt(_T("UploadClientDataRate"), m_uUploadClientDataRate);

	ini.WriteInt(_T("QueueSize"), (int)m_iQueueSize);

	ini.WriteInt(_T("CommitFiles"), m_iCommitFiles);
	ini.WriteBool(_T("DAPPref"), m_bDAP);
	ini.WriteBool(_T("UAPPref"), m_bUAP);
	ini.WriteBool(_T("FilterServersByIP"), filterserverbyip);
	ini.WriteBool(_T("DisableKnownClientList"), m_bDisableKnownClientList);
	ini.WriteBool(_T("DisableQueueList"), m_bDisableQueueList);
	ini.WriteBool(_T("UseCreditSystem"), m_bCreditSystem);
	ini.WriteBool(_T("SaveLogToDisk"), log2disk);
	ini.WriteBool(_T("SaveDebugToDisk"), debug2disk);
	ini.WriteBool(_T("MessagesFromFriendsOnly"), msgonlyfriends);
	ini.WriteBool(_T("MessageFromValidSourcesOnly"), msgsecure);
	ini.WriteBool(_T("MessageUseCaptchas"), m_bUseChatCaptchas);
	ini.WriteBool(_T("ShowInfoOnCatTabs"), showCatTabInfos);
	ini.WriteBool(_T("AutoFilenameCleanup"), autofilenamecleanup);
	ini.WriteBool(_T("ShowExtControls"), m_bExtControls);
	ini.WriteBool(_T("UseAutocompletion"), m_bUseAutocompl);
	ini.WriteBool(_T("NetworkKademlia"), networkkademlia);
	ini.WriteBool(_T("NetworkED2K"), networked2k);
	ini.WriteBool(_T("AutoClearCompleted"), m_bRemoveFinishedDownloads);
	ini.WriteBool(_T("TransflstRemainOrder"), m_bTransflstRemain);
	ini.WriteBool(_T("UseSimpleTimeRemainingcomputation"), m_bUseOldTimeRemaining);
	ini.WriteBool(_T("AllocateFullFile"), m_bAllocFull);
	ini.WriteBool(_T("ShowSharedFilesDetails"), m_bShowSharedFilesDetails);
	ini.WriteBool(_T("AutoShowLookups"), m_bAutoShowLookups);
	ini.WriteInt(_T("FilterLevel"), filterlevel);

	ini.WriteBool(_T("SecureIdent"), m_bUseSecureIdent);// change the name in future version to enable it by default
	ini.WriteBool(_T("AdvancedSpamFilter"), m_bAdvancedSpamfilter);
	ini.WriteBool(_T("ShowDwlPercentage"), m_bShowDwlPercentage);
	ini.WriteBool(_T("RemoveFilesToBin"), m_bRemove2bin);
	//ini.WriteBool(_T("ShowCopyEd2kLinkCmd"),m_bShowCopyEd2kLinkCmd);
	ini.WriteBool(_T("AutoArchivePreviewStart"), m_bAutomaticArcPreviewStart);
	// Hidden runtime preferences exposed in the advanced tree.
	ini.WriteBool(_T("RestoreLastMainWndDlg"), m_bRestoreLastMainWndDlg);
	ini.WriteBool(_T("RestoreLastLogPane"), m_bRestoreLastLogPane);
	ini.WriteInt(_T("FileBufferTimeLimit"), static_cast<int>(m_uFileBufferTimeLimit / SEC2MS(1)));
	ini.WriteBool(_T("PreviewCopiedArchives"), m_bPreviewCopiedArchives);
	ini.WriteInt(_T("InspectAllFileTypes"), m_iInspectAllFileTypes);
	ini.WriteBool(_T("PreviewOnIconDblClk"), m_bPreviewOnIconDblClk);
	ini.WriteBool(_T("ShowActiveDownloadsBold"), m_bShowActiveDownloadsBold);
	ini.WriteBool(_T("UseSystemFontForMainControls"), m_bUseSystemFontForMainControls);
	ini.WriteBool(_T("ReBarToolbar"), m_bReBarToolbar);
	ini.WriteBool(_T("ShowUpDownIconInTaskbar"), m_bShowUpDownIconInTaskbar);
	ini.WriteBool(_T("ShowVerticalHourMarkers"), m_bShowVerticalHourMarkers);
	ini.WriteBool(_T("ForceSpeedsToKB"), m_bForceSpeedsToKB);
	ini.WriteBool(_T("ExtraPreviewWithMenu"), m_bExtraPreviewWithMenu);
	ini.WriteBool(_T("KeepUnavailableFixedSharedDirs"), m_bKeepUnavailableFixedSharedDirs);
	ini.WriteBool(_T("PartiallyPurgeOldKnownFiles"), m_bPartiallyPurgeOldKnownFiles);
	ini.WriteBool(_T("AdjustNTFSDaylightFileTime"), m_bAdjustNTFSDaylightFileTime);
	ini.WriteBool(_T("RearrangeKadSearchKeywords"), m_bRearrangeKadSearchKeywords);
	ini.WriteBool(_T("BanBadKadNodes"), m_bBanBadKadNodes);

	// Toolbar
	ini.WriteString(_T("ToolbarSetting"), m_sToolbarSettings);
	ini.WriteString(_T("ToolbarBitmap"), m_sToolbarBitmap);
	ini.WriteString(_T("ToolbarBitmapFolder"), m_sToolbarBitmapFolder);
	ini.WriteInt(_T("ToolbarLabels"), m_nToolbarLabels);
	ini.WriteInt(_T("ToolbarIconSize"), m_sizToolbarIconSize.cx);
	ini.WriteString(_T("SkinProfile"), m_strSkinProfile);
	ini.WriteString(_T("SkinProfileDir"), m_strSkinProfileDir);

	ini.WriteBinary(_T("HyperTextFont"), (LPBYTE)&m_lfHyperText, sizeof m_lfHyperText);
	ini.WriteBinary(_T("LogTextFont"), (LPBYTE)&m_lfLogText, sizeof m_lfLogText);

	ini.WriteBool(_T("A4AFSaveCpu"), m_bA4AFSaveCpu); // ZZ:DownloadManager
	ini.WriteBool(_T("HighresTimer"), m_bHighresTimer);
	ini.WriteInt(_T("WebMirrorAlertLevel"), m_nWebMirrorAlertLevel);
	ini.WriteInt(_T("DebugLogLevel"), m_byLogLevel);
	ini.WriteBool(_T("RememberCancelledFiles"), m_bRememberCancelledFiles);
	ini.WriteBool(_T("RememberDownloadedFiles"), m_bRememberDownloadedFiles);

	ini.WriteBool(_T("WinaTransToolbar"), m_bWinaTransToolbar);
	ini.WriteBool(_T("ShowDownloadToolbar"), m_bShowDownloadToolbar);

	ini.WriteBool(_T("CryptLayerRequested"), m_bCryptLayerRequested);
	ini.WriteBool(_T("CryptLayerRequired"), m_bCryptLayerRequired);
	ini.WriteBool(_T("CryptLayerSupported"), m_bCryptLayerSupported);
	ini.WriteInt(_T("KadUDPKey"), m_dwKadUDPKey);
	ini.WriteInt(_T("KadPublishSourceThrottle"), m_uKadPublishSourceThrottle);

	ini.WriteBool(_T("EnableSearchResultSpamFilter"), m_bEnableSearchResultFilter);


	///////////////////////////////////////////////////////////////////////////
	// Section: "Proxy"
	//
	ini.WriteBool(_T("ProxyEnablePassword"), proxy.bEnablePassword, _T("Proxy"));
	ini.WriteBool(_T("ProxyEnableProxy"), proxy.bUseProxy, _T("Proxy"));
	ini.WriteString(_T("ProxyName"), proxy.host, _T("Proxy"));
	ini.WriteString(_T("ProxyPassword"), proxy.password, _T("Proxy"));
	ini.WriteString(_T("ProxyUser"), proxy.user, _T("Proxy"));
	ini.WriteInt(_T("ProxyPort"), proxy.port, _T("Proxy"));
	ini.WriteInt(_T("ProxyType"), proxy.type, _T("Proxy"));


	///////////////////////////////////////////////////////////////////////////
	// Section: "Statistics"
	//
	ini.WriteInt(_T("statsConnectionsGraphRatio"), statsConnectionsGraphRatio, _T("Statistics"));
	ini.WriteString(_T("statsExpandedTreeItems"), m_strStatsExpandedTreeItems);
	CString sValue, sKey;
	for (int i = 0; i < 15; ++i) {
		sValue.Format(_T("0x%06lx"), GetStatsColor(i));
		sKey.Format(_T("StatColor%i"), i);
		ini.WriteString(sKey, sValue, _T("Statistics"));
	}
	ini.WriteBool(_T("HasCustomTaskIconColor"), m_bHasCustomTaskIconColor, _T("Statistics"));


	///////////////////////////////////////////////////////////////////////////
	// Section: "UPnP"
	//
	ini.WriteBool(_T("EnableUPnP"), m_bEnableUPnP, _T("UPnP"));
	ini.WriteBool(_T("SkipWANIPSetup"), m_bSkipWANIPSetup);
	ini.WriteBool(_T("SkipWANPPPSetup"), m_bSkipWANPPPSetup);
	ini.WriteBool(_T("CloseUPnPOnExit"), m_bCloseUPnPOnExit);
	ini.WriteInt(_T("LastWorkingImplementation"), m_nLastWorkingImpl);
}

void CPreferences::ResetStatsColor(int index)
{
	static const COLORREF defcol[15] =
	{
		RGB(0, 0, 64),		RGB(192, 192, 255),	RGB(128, 255, 128),	RGB(0, 210, 0),		RGB(0, 128, 0),
		RGB(255, 128, 128),	RGB(200, 0, 0),		RGB(140, 0, 0),		RGB(150, 150, 255),	RGB(192, 0, 192),
		RGB(255, 255, 128),	RGB(0, 0, 0), /**/	RGB(255, 255, 255),	RGB(255, 255, 255),	RGB(255, 190, 190)
	};
	if (index >= 0 && index < (int)_countof(defcol)) {
		m_adwStatsColors[index] = defcol[index];
		if (index == 11) /**/
			m_bHasCustomTaskIconColor = false;
	}
}

void CPreferences::GetAllStatsColors(int iCount, LPDWORD pdwColors)
{
	const size_t cnt = iCount * sizeof(*pdwColors);
	memcpy(pdwColors, m_adwStatsColors, min(sizeof m_adwStatsColors, cnt));
	if (cnt > sizeof m_adwStatsColors)
		memset(&pdwColors[sizeof m_adwStatsColors], 0, cnt - sizeof m_adwStatsColors);
}

bool CPreferences::SetAllStatsColors(int iCount, const LPDWORD pdwColors)
{
	bool bModified = false;
	int iMin = min((int)_countof(m_adwStatsColors), iCount);
	for (int i = 0; i < iMin; ++i)
		if (m_adwStatsColors[i] != pdwColors[i]) {
			m_adwStatsColors[i] = pdwColors[i];
			bModified = true;
			if (i == 11)
				m_bHasCustomTaskIconColor = true;
		}

	return bModified;
}

void CPreferences::IniCopy(const CString &si, const CString &di)
{
	CIni ini(GetConfigFile(), _T("eMule"));
	const CString &sValue(ini.GetString(si));
	// Do NOT write empty settings, this will mess up reading of default settings in case
	// there were no settings available at all (fresh emule install)!
	if (!sValue.IsEmpty())
		ini.WriteString(di, sValue, _T("ListControlSetup"));
}

void CPreferences::LoadPreferences()
{
	CIni ini(GetConfigFile(), _T("eMule"));
	ini.SetSection(_T("eMule"));

	m_bFirstStart = ini.GetString(_T("AppVersion")).IsEmpty();

#ifdef _BETA
	CString strCurrVersion(theApp.m_strCurVersionLong);
	m_bBetaNaggingDone = (ini.GetString(_T("BetaVersionNotified"), _T("")) == strCurrVersion);
#endif

#ifdef _DEBUG
	m_iDbgHeap = ini.GetInt(_T("DebugHeap"), 1);
#else
	m_iDbgHeap = 0;
#endif

	m_nWebMirrorAlertLevel = ini.GetInt(_T("WebMirrorAlertLevel"), 0);

	SetUserNick(ini.GetStringUTF8(_T("Nick"), DEFAULT_NICK));
	if (strNick.IsEmpty() || IsDefaultNick(strNick))
		SetUserNick(DEFAULT_NICK);

	m_strIncomingDir = ini.GetString(_T("IncomingDir"), _T(""));
	if (m_strIncomingDir.IsEmpty()) // We want GetDefaultDirectory to also create the folder, so we have to know if we use the default or not
		m_strIncomingDir = GetDefaultDirectory(EMULE_INCOMINGDIR, true);
	MakeFoldername(m_strIncomingDir);

	// load tempdir(s) setting
	CString sTempdirs(ini.GetString(_T("TempDir"), _T("")));
	if (sTempdirs.IsEmpty()) // We want GetDefaultDirectory to also create the folder, so we have to know if we use the default or not
		sTempdirs = GetDefaultDirectory(EMULE_TEMPDIR, true);
	sTempdirs.AppendFormat(_T("|%s"), (LPCTSTR)ini.GetString(_T("TempDirs")));

	for (int iPos = 0; iPos >= 0;) {
		CString sTmp(sTempdirs.Tokenize(_T("|"), iPos));
		if (sTmp.Trim().IsEmpty())
			continue;
		MakeFoldername(sTmp);
		bool bDup = false;
		for (INT_PTR i = tempdir.GetCount(); --i >= 0;)	// avoid duplicate tempdirs
			if (sTmp.CompareNoCase(GetTempDir(i)) == 0) {
				bDup = true;
				break;
			}

		if (!bDup && (::PathFileExists(sTmp) || ::CreateDirectory(sTmp, NULL)) || tempdir.IsEmpty())
			tempdir.Add(sTmp);
	}

	maxGraphDownloadRateEstimated = 0;
	maxGraphUploadRateEstimated = 0;

	// Hidden broadband knob: keep the normal upload-slot target low on modern uplinks.
	m_maxUpClientsAllowed = max((uint32)MIN_UP_CLIENTS_ALLOWED, min((uint32)MAX_UP_CLIENTS_ALLOWED, (uint32)ini.GetInt(_T("BBMaxUpClientsAllowed"), 12)));
	// Broadband session limits override the stock one-chunk / one-hour rotation defaults when configured.
	// A stored value of 0 explicitly disables that rotation path.
	m_bbSessionMaxTrans = ini.GetUInt64(_T("BBSessionMaxTrans"), SESSIONMAXTRANS);
	m_bbSessionMaxTime = ini.GetUInt64(_T("BBSessionMaxTime"), SESSIONMAXTIME);
	// Seeder-biased queue policy is opt-in. A zero threshold or bonus disables the low-ratio boost.
	m_bbBoostLowRatioFiles = max(0.0f, ini.GetFloat(_T("BBBoostLowRatioFiles"), 0.0f));
	m_bbBoostLowRatioFilesBy = max(0.0f, ini.GetFloat(_T("BBBoostLowRatioFilesBy"), 0.0f));
	// A divisor of 0 or 1 is treated as disabled to avoid surprising no-op math in GetScore.
	m_bbDeboostLowIDs = max(0, ini.GetInt(_T("BBDeboostLowIDs"), 0));
	m_maxupload = NormalizeBandwidthLimit(ini.GetInt(_T("MaxUpload"), s_uDefaultUploadLimit));
	m_maxdownload = NormalizeBandwidthLimit(ini.GetInt(_T("MaxDownload"), s_uDefaultDownloadLimit));
	maxGraphUploadRate = (m_maxupload != UNLIMITED) ? m_maxupload : s_uDefaultUploadLimit;
	maxGraphDownloadRate = (m_maxdownload != UNLIMITED) ? m_maxdownload : s_uDefaultDownloadLimit;
	maxconnections = ini.GetInt(_T("MaxConnections"), GetRecommendedMaxConnections());
	maxhalfconnections = ini.GetInt(_T("MaxHalfConnections"), 50);
	m_bConditionalTCPAccept = ini.GetBool(_T("ConditionalTCPAccept"), false);

	m_strBindInterface = ini.GetString(_T("BindInterface")).Trim();
	m_strBindInterfaceName = ini.GetString(_T("BindInterfaceName")).Trim();
	m_strConfiguredBindAddr = ini.GetString(_T("BindAddr")).Trim();
	m_bBlockNetworkWhenBindUnavailableAtStartup = ini.GetBool(_T("BlockNetworkWhenBindUnavailableAtStartup"), false);
	m_eBindAddrResolveResult = ResolveConfiguredBinding(_T("P2P"), m_strBindInterface, m_strBindInterfaceName, m_strConfiguredBindAddr
		, m_strBindAddrW, m_pszBindAddrW, m_strBindAddrA, m_pszBindAddrA);
	m_bRandomizePortsOnStartup = ini.GetBool(_T("RandomizePortsOnStartup"), false);

	const uint16 nConfiguredTCPPort = static_cast<uint16>(ini.GetInt(_T("Port"), 0));
	const int iConfiguredUDPPort = ini.GetInt(_T("UDPPort"), INT_MAX/*invalid port value*/);
	if (m_bRandomizePortsOnStartup) {
		// Startup randomization always picks a fresh TCP port, while preserving explicit UDP-disable.
		const bool bRandomizeUDP = (iConfiguredUDPPort != 0);
		port = GetRandomStartupTCPPort(bRandomizeUDP);
		udpport = bRandomizeUDP ? static_cast<uint16>(port + s_uStartupUdpPortOffset) : 0;
	} else {
		port = nConfiguredTCPPort;
		if (port == 0) {
			// Keep the fresh-install default TCP port inside the requested startup range.
			if (m_bFirstStart)
				port = GetRandomStartupTCPPort(iConfiguredUDPPort == INT_MAX);
			else
				port = thePrefs.GetRandomTCPPort();
		}

		// 0 is a valid value for the UDP port setting, as it is used for disabling it.
		if (iConfiguredUDPPort == INT_MAX) {
			// Keep the first-start UDP default paired to TCP unless the user explicitly configured otherwise.
			if (m_bFirstStart && port <= static_cast<uint16>(_UI16_MAX - s_uStartupUdpPortOffset))
				udpport = static_cast<uint16>(port + s_uStartupUdpPortOffset);
			else
				udpport = thePrefs.GetRandomUDPPort();
		} else
			udpport = static_cast<uint16>(iConfiguredUDPPort);
	}

	nServerUDPPort = (uint16)ini.GetInt(_T("ServerUDPPort"), -1); // 0 = Don't use UDP port for servers, -1 = use a random port (for backward compatibility)
	maxsourceperfile = ini.GetInt(_T("MaxSourcesPerFile"), ModernLimits::kDefaultMaxSourcesPerFile);
	m_wLanguageID = ini.GetWORD(_T("Language"), 0);
	m_iSeeShares = (EViewSharedFilesAccess)ini.GetInt(_T("SeeShare"), vsfaNobody);
	m_iToolDelayTime = ini.GetInt(_T("ToolTipDelay"), 1);
	trafficOMeterInterval = ini.GetInt(_T("StatGraphsInterval"), 3);
	statsInterval = ini.GetInt(_T("statsInterval"), 5);
	m_bFillGraphs = ini.GetBool(_T("StatsFillGraphs"));

	m_uDeadServerRetries = ini.GetInt(_T("DeadServerRetry"), 5);
	if (m_uDeadServerRetries > MAX_SERVERFAILCOUNT)
		m_uDeadServerRetries = MAX_SERVERFAILCOUNT;
	m_dwServerKeepAliveTimeout = ini.GetInt(_T("ServerKeepAliveTimeout"), 0);
	m_dwConnectionTimeout = ModernLimits::NormalizeTimeoutSeconds(ini.GetInt(_T("ConnectionTimeout"), ModernLimits::kDefaultConnectionTimeoutSeconds), ModernLimits::kDefaultConnectionTimeoutSeconds);
	m_dwDownloadTimeout = ModernLimits::NormalizeTimeoutSeconds(ini.GetInt(_T("DownloadTimeout"), ModernLimits::kDefaultDownloadTimeoutSeconds), ModernLimits::kDefaultDownloadTimeoutSeconds);
	splitterbarPosition = ini.GetInt(_T("SplitterbarPosition"), 75);
	if (splitterbarPosition < 9)
		splitterbarPosition = 9;
	else if (splitterbarPosition > 93)
		splitterbarPosition = 93;
	splitterbarPositionStat = ini.GetInt(_T("SplitterbarPositionStat"), 30);
	splitterbarPositionStat_HL = ini.GetInt(_T("SplitterbarPositionStat_HL"), 66);
	splitterbarPositionStat_HR = ini.GetInt(_T("SplitterbarPositionStat_HR"), 33);
	if (splitterbarPositionStat_HR + 1 >= splitterbarPositionStat_HL) {
		splitterbarPositionStat_HL = 66;
		splitterbarPositionStat_HR = 33;
	}
	splitterbarPositionFriend = ini.GetInt(_T("SplitterbarPositionFriend"), 170);
	splitterbarPositionShared = ini.GetInt(_T("SplitterbarPositionShared"), 179);
	splitterbarPositionSvr = ini.GetInt(_T("SplitterbarPositionServer"), 75);
	if (splitterbarPositionSvr > 90 || splitterbarPositionSvr < 10)
		splitterbarPositionSvr = 75;

	m_uTransferWnd1 = ini.GetInt(_T("TransferWnd1"), 0);
	m_uTransferWnd2 = ini.GetInt(_T("TransferWnd2"), 1);

	statsMax = ini.GetInt(_T("VariousStatisticsMaxValue"), 100);
	statsAverageMinutes = ini.GetInt(_T("StatsAverageMinutes"), 5);
	MaxConperFive = ini.GetInt(_T("MaxConnectionsPerFiveSeconds"), GetDefaultMaxConperFive());

	reconnect = ini.GetBool(_T("Reconnect"), true);
	m_bUseServerPriorities = ini.GetBool(_T("Scoresystem"), true);
	m_bUseUserSortedServerList = ini.GetBool(_T("UserSortedServerList"), false);
	ICH = ini.GetBool(_T("ICH"), true);
	m_bAutoUpdateServerList = ini.GetBool(_T("Serverlist"), false);

	// Keep minimize-to-tray opt-in across all themes so fresh installs stay visible by default.
	if (IsRunningAeroGlassTheme())
		mintotray = ini.GetBool(_T("MinToTray_Aero"), false);
	else
		mintotray = ini.GetBool(_T("MinToTray"), false);

	m_bPreventStandby = ini.GetBool(_T("PreventStandby"), false);
	m_bStoreSearches = ini.GetBool(_T("StoreSearches"), true);
	m_bAddServersFromServer = ini.GetBool(_T("AddServersFromServer"), true);
	m_bAddServersFromClients = ini.GetBool(_T("AddServersFromClient"), false);
	bringtoforeground = ini.GetBool(_T("BringToFront"), true);
	transferDoubleclick = ini.GetBool(_T("TransferDoubleClick"), true);
	beepOnError = ini.GetBool(_T("BeepOnError"), true);
	confirmExit = ini.GetBool(_T("ConfirmExit"), false);
	filterLANIPs = ini.GetBool(_T("FilterBadIPs"), true);
	m_bAllocLocalHostIP = ini.GetBool(_T("AllowLocalHostIP"), false);
	autoconnect = ini.GetBool(_T("Autoconnect"), false);
	showRatesInTitle = ini.GetBool(_T("ShowRatesOnTitle"), true);
	m_bIconflashOnNewMessage = ini.GetBool(_T("IconflashOnNewMessage"), false);

	onlineSig = ini.GetBool(_T("OnlineSignature"), false);
	startMinimized = ini.GetBool(_T("StartupMinimized"), false);
	m_bAutoStart = ini.GetBool(_T("AutoStart"), false);
	m_bRestoreLastMainWndDlg = ini.GetBool(_T("RestoreLastMainWndDlg"), false);
	m_iLastMainWndDlgID = ini.GetInt(_T("LastMainWndDlgID"), 0);
	m_bRestoreLastLogPane = ini.GetBool(_T("RestoreLastLogPane"), false);
	m_iLastLogPaneID = ini.GetInt(_T("LastLogPaneID"), 0);
	m_bSafeServerConnect = ini.GetBool(_T("SafeServerConnect"), false);

	m_bTransflstRemain = ini.GetBool(_T("TransflstRemainOrder"), false);
	filterserverbyip = ini.GetBool(_T("FilterServersByIP"), false);
	filterlevel = ini.GetInt(_T("FilterLevel"), 127);
	checkDiskspace = ini.GetBool(_T("CheckDiskspace"), false);
	m_uMinFreeDiskSpace = ini.GetInt(_T("MinFreeDiskSpace"), 5 * 1024 * 1024 * 1024);
	m_bSparsePartFiles = ini.GetBool(_T("SparsePartFiles"), false);
	m_bResolveSharedShellLinks = ini.GetBool(_T("ResolveSharedShellLinks"), false);
	m_bAutoRescanSharedFolders = ini.GetBool(_T("AutoRescanSharedFolders"), true);
	m_uAutoRescanSharedFoldersIntervalSec = max(600, ini.GetInt(_T("AutoRescanSharedFoldersIntervalSec"), 1200));
	m_bAutoShareNewSharedSubdirs = ini.GetBool(_T("AutoShareNewSharedSubdirs"), true);
	m_bKeepUnavailableFixedSharedDirs = ini.GetBool(_T("KeepUnavailableFixedSharedDirs"), false);
	m_strYourHostname = ini.GetString(_T("YourHostname"), _T(""));
	m_strNodesDatUpdateUrl = ini.GetString(_T("NodesDatUpdateUrl"), _T("http://upd.emule-security.org/nodes.dat"));
	m_bImportParts = false; //enable on demand for the current session only

	// Barry - New properties...
	m_bAutoConnectToStaticServersOnly = ini.GetBool(_T("AutoConnectStaticOnly"), false);
	autotakeed2klinks = ini.GetBool(_T("AutoTakeED2KLinks"), true);
	addnewfilespaused = ini.GetBool(_T("AddNewFilesPaused"), false);
	depth3D = ini.GetInt(_T("3DDepth"), 5);
	// Notifier
	notifierConfiguration = ini.GetString(_T("NotifierConfiguration"), GetMuleDirectory(EMULE_CONFIGDIR) + _T("Notifier.ini"));
	notifierOnDownloadFinished = ini.GetBool(_T("NotifyOnDownload"));
	notifierOnNewDownload = ini.GetBool(_T("NotifyOnNewDownload"));
	notifierOnChat = ini.GetBool(_T("NotifyOnChat"));
	notifierOnLog = ini.GetBool(_T("NotifyOnLog"));
	notifierOnImportantError = ini.GetBool(_T("NotifyOnImportantError"));
	notifierOnEveryChatMsg = ini.GetBool(_T("NotifierPopEveryChatMessage"));
	notifierSoundType = (ENotifierSoundType)ini.GetInt(_T("NotifierUseSound"), ntfstNoSound);
	notifierSoundFile = ini.GetString(_T("NotifierSoundPath"));

	m_strDateTimeFormat = ini.GetString(_T("DateTimeFormat"), _T("%A, %c"));
	m_strDateTimeFormat4Log = ini.GetString(_T("DateTimeFormat4Log"), _T("%c"));
	m_strDateTimeFormat4Lists = ini.GetString(_T("DateTimeFormat4Lists"), _T("%c"));
	m_bMessageEnableSmileys = ini.GetBool(_T("MessageEnableSmileys"), true);

	m_bSmartServerIdCheck = ini.GetBool(_T("SmartIdCheck"), true);
	log2disk = ini.GetBool(_T("SaveLogToDisk"), false);
	uMaxLogFileSize = ini.GetInt(_T("MaxLogFileSize"), 1024 * 1024);
	iMaxLogBuff = ini.GetInt(_T("MaxLogBuff"), 64) * 1024;
	m_iLogFileFormat = (ELogFileFormat)ini.GetInt(_T("LogFileFormat"), Unicode);
	m_bEnableVerboseOptions = ini.GetBool(_T("VerboseOptions"), true);
	if (m_bEnableVerboseOptions) {
		m_bVerbose = ini.GetBool(_T("Verbose"), false);
		m_bFullVerbose = ini.GetBool(_T("FullVerbose"), false);
		debug2disk = ini.GetBool(_T("SaveDebugToDisk"), false);
		m_bDebugSourceExchange = ini.GetBool(_T("DebugSourceExchange"), false);
		m_bLogBannedClients = ini.GetBool(_T("LogBannedClients"), true);
		m_bLogRatingDescReceived = ini.GetBool(_T("LogRatingDescReceived"), true);
		m_bLogSecureIdent = ini.GetBool(_T("LogSecureIdent"), true);
		m_bLogFilteredIPs = ini.GetBool(_T("LogFilteredIPs"), true);
		m_bLogFileSaving = ini.GetBool(_T("LogFileSaving"), false);
		m_bLogA4AF = ini.GetBool(_T("LogA4AF"), false); // ZZ:DownloadManager
		m_bLogUlDlEvents = ini.GetBool(_T("LogUlDlEvents"), true);
	} else {
		if (m_bRestoreLastLogPane && m_iLastLogPaneID >= 2)
			m_iLastLogPaneID = 1;
	}

#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
	// following options are for debugging or when using an external debug device viewer only.
	m_iDebugServerTCPLevel = ini.GetInt(_T("DebugServerTCP"), 0);
	m_iDebugServerUDPLevel = ini.GetInt(_T("DebugServerUDP"), 0);
	m_iDebugServerSourcesLevel = ini.GetInt(_T("DebugServerSources"), 0);
	m_iDebugServerSearchesLevel = ini.GetInt(_T("DebugServerSearches"), 0);
	m_iDebugClientTCPLevel = ini.GetInt(_T("DebugClientTCP"), 0);
	m_iDebugClientUDPLevel = ini.GetInt(_T("DebugClientUDP"), 0);
	m_iDebugClientKadUDPLevel = ini.GetInt(_T("DebugClientKadUDP"), 0);
	m_iDebugSearchResultDetailLevel = ini.GetInt(_T("DebugSearchResultDetailLevel"), 0);
#else
	// for normal release builds ensure that all these options are turned off
	m_iDebugServerTCPLevel = 0;
	m_iDebugServerUDPLevel = 0;
	m_iDebugServerSourcesLevel = 0;
	m_iDebugServerSearchesLevel = 0;
	m_iDebugClientTCPLevel = 0;
	m_iDebugClientUDPLevel = 0;
	m_iDebugClientKadUDPLevel = 0;
	m_iDebugSearchResultDetailLevel = 0;
#endif

	m_bpreviewprio = ini.GetBool(_T("PreviewPrio"), true);
	m_bupdatequeuelist = ini.GetBool(_T("UpdateQueueListPref"), false);
	m_bManualAddedServersHighPriority = ini.GetBool(_T("ManualHighPrio"), false);
	m_btransferfullchunks = ini.GetBool(_T("FullChunkTransfers"), true);
	m_istartnextfile = ini.GetInt(_T("StartNextFile"), 0);
	m_bshowoverhead = ini.GetBool(_T("ShowOverhead"), false);
	m_bMoviePreviewBackup = ini.GetBool(_T("VideoPreviewBackupped"), true);
	m_iPreviewSmallBlocks = ini.GetInt(_T("PreviewSmallBlocks"), 0);
	m_bPreviewCopiedArchives = ini.GetBool(_T("PreviewCopiedArchives"), true);
	m_iInspectAllFileTypes = ini.GetInt(_T("InspectAllFileTypes"), 0);
	m_bAllocFull = ini.GetBool(_T("AllocateFullFile"), 0);
	m_bAutomaticArcPreviewStart = ini.GetBool(_T("AutoArchivePreviewStart"), true);
	m_bShowSharedFilesDetails = ini.GetBool(_T("ShowSharedFilesDetails"), true);
	m_bAutoShowLookups = ini.GetBool(_T("AutoShowLookups"), true);
	m_bShowUpDownIconInTaskbar = ini.GetBool(_T("ShowUpDownIconInTaskbar"), false);
	m_bShowTaskbarProgress = ini.GetBool(_T("ShowTaskbarProgress"), true);
	m_bForceSpeedsToKB = ini.GetBool(_T("ForceSpeedsToKB"), false);
	m_bExtraPreviewWithMenu = ini.GetBool(_T("ExtraPreviewWithMenu"), false);

	// Get file buffer size.
	m_uFileBufferSize = ModernLimits::kDefaultFileBufferSize;
	m_uFileBufferSize = ini.GetInt(_T("FileBufferSize"), m_uFileBufferSize);
	m_uUDPReceiveBufferSize = max(64 * 1024u, (UINT)ini.GetInt(_T("UDPReceiveBufferSize"), ModernLimits::kDefaultUdpReceiveBufferSize));
	m_uTCPSendBufferSize = max(64 * 1024u, (UINT)ini.GetInt(_T("BigSendBufferSize"), ModernLimits::kDefaultTcpSendBufferSize));
	m_uUploadClientDataRate = max(3u * 1024u, (UINT)ini.GetInt(_T("UploadClientDataRate"), ModernLimits::kDefaultUploadClientDataRate));
	m_uFileBufferTimeLimit = ModernLimits::SecondsToMs(ini.GetInt(_T("FileBufferTimeLimit"), ModernLimits::kDefaultFileBufferTimeLimitSeconds));

	// Get queue size.
	m_iQueueSize = ModernLimits::kDefaultQueueSize;
	m_iQueueSize = ini.GetInt(_T("QueueSize"), (int)m_iQueueSize);

	m_iCommitFiles = ini.GetInt(_T("CommitFiles"), 1); // 1 = "commit" on application shutdown; 2 = "commit" on each file saving
	m_bDAP = ini.GetBool(_T("DAPPref"), true);
	m_bUAP = ini.GetBool(_T("UAPPref"), true);
	m_bPreviewOnIconDblClk = ini.GetBool(_T("PreviewOnIconDblClk"), false);
	m_bCheckFileOpen = ini.GetBool(_T("CheckFileOpen"), true);
	indicateratings = ini.GetBool(_T("IndicateRatings"), true);
	watchclipboard = ini.GetBool(_T("WatchClipboard4ED2kFilelinks"), false);
	m_iSearchMethod = ini.GetInt(_T("SearchMethod"), 0);

	showCatTabInfos = ini.GetBool(_T("ShowInfoOnCatTabs"), false);
	//resumeSameCat = ini.GetBool(_T("ResumeNextFromSameCat"), false);
	dontRecreateGraphs = ini.GetBool(_T("DontRecreateStatGraphsOnResize"), false);
	m_bExtControls = ini.GetBool(_T("ShowExtControls"), true);
	m_bDisableKnownClientList = ini.GetBool(_T("DisableKnownClientList"), false);
	m_bDisableQueueList = ini.GetBool(_T("DisableQueueList"), false);
	m_bCreditSystem = ini.GetBool(_T("UseCreditSystem"), true);
	msgonlyfriends = ini.GetBool(_T("MessagesFromFriendsOnly"), false);
	msgsecure = ini.GetBool(_T("MessageFromValidSourcesOnly"), true);
	m_bUseChatCaptchas = ini.GetBool(_T("MessageUseCaptchas"), true);
	autofilenamecleanup = ini.GetBool(_T("AutoFilenameCleanup"), false);
	m_bUseAutocompl = ini.GetBool(_T("UseAutocompletion"), true);
	m_bShowDwlPercentage = ini.GetBool(_T("ShowDwlPercentage"), false);
	networkkademlia = ini.GetBool(_T("NetworkKademlia"), true);
	networked2k = ini.GetBool(_T("NetworkED2K"), true);
	m_bRemove2bin = ini.GetBool(_T("RemoveFilesToBin"), true);
	m_bShowCopyEd2kLinkCmd = ini.GetBool(_T("ShowCopyEd2kLinkCmd"), false);

	m_iMaxChatHistory = ini.GetInt(_T("MaxChatHistoryLines"), 100);
	if (m_iMaxChatHistory < 1)
		m_iMaxChatHistory = 100;
	maxmsgsessions = ini.GetInt(_T("MaxMessageSessions"), 50);
	m_bShowActiveDownloadsBold = ini.GetBool(_T("ShowActiveDownloadsBold"), false);

	m_strTxtEditor = ini.GetString(_T("TxtEditor"), _T("notepad.exe"));
	m_strVideoPlayer = ini.GetString(_T("VideoPlayer"), _T(""));
	m_strVideoPlayerArgs = ini.GetString(_T("VideoPlayerArgs"), _T(""));

	messageFilter = ini.GetStringLong(_T("MessageFilter"), _T("fastest download speed|fastest eMule"));
	commentFilter = ini.GetStringLong(_T("CommentFilter"), _T("http://|https://|ftp://|www.|ftp."));
	commentFilter.MakeLower();
	filenameCleanups = ini.GetStringLong(_T("FilenameCleanups"), _T("http|www.|.com|.de|.org|.net|shared|powered|sponsored|sharelive|filedonkey|"));
	m_iExtractMetaData = ini.GetInt(_T("ExtractMetaData"), 1); // 0=disable, 1=enabled
	if (m_iExtractMetaData > 1)
		m_iExtractMetaData = 1;
	m_bAdjustNTFSDaylightFileTime = ini.GetBool(_T("AdjustNTFSDaylightFileTime"), false);
	m_bRearrangeKadSearchKeywords = ini.GetBool(_T("RearrangeKadSearchKeywords"), true);
	m_bBanBadKadNodes = ini.GetBool(_T("BanBadKadNodes"), true);

	m_bUseSecureIdent = ini.GetBool(_T("SecureIdent"), true);
	m_bAdvancedSpamfilter = ini.GetBool(_T("AdvancedSpamFilter"), true);
	m_bRemoveFinishedDownloads = ini.GetBool(_T("AutoClearCompleted"), false);
	m_bUseOldTimeRemaining = ini.GetBool(_T("UseSimpleTimeRemainingcomputation"), false);

	// Toolbar
	m_sToolbarSettings = ini.GetString(_T("ToolbarSetting"), strDefaultToolbar);
	m_sToolbarBitmap = ini.GetString(_T("ToolbarBitmap"), _T(""));
	m_sToolbarBitmapFolder = ini.GetString(_T("ToolbarBitmapFolder"), _T(""));
	if (m_sToolbarBitmapFolder.IsEmpty()) // We want GetDefaultDirectory to also create the folder, so we have to know if we use the default or not
		m_sToolbarBitmapFolder = GetDefaultDirectory(EMULE_TOOLBARDIR, true);
	else
		slosh(m_sToolbarBitmapFolder);
	m_nToolbarLabels = (EToolbarLabelType)ini.GetInt(_T("ToolbarLabels"), CMuleToolbarCtrl::GetDefaultLabelType());
	m_bReBarToolbar = ini.GetBool(_T("ReBarToolbar"), 1);
	m_sizToolbarIconSize.cx = m_sizToolbarIconSize.cy = ini.GetInt(_T("ToolbarIconSize"), 32);
	m_iStraightWindowStyles = ini.GetInt(_T("StraightWindowStyles"), 0);
	m_bUseSystemFontForMainControls = ini.GetBool(_T("UseSystemFontForMainControls"), 0);
	m_bRTLWindowsLayout = ini.GetBool(_T("RTLWindowsLayout"));
	m_strSkinProfile = ini.GetString(_T("SkinProfile"), _T(""));
	m_strSkinProfileDir = ini.GetString(_T("SkinProfileDir"), _T(""));
	if (m_strSkinProfileDir.IsEmpty()) // We want GetDefaultDirectory to also create the folder, so we have to know if we use the default or not
		m_strSkinProfileDir = GetDefaultDirectory(EMULE_SKINDIR, true);
	else
		slosh(m_strSkinProfileDir);

	LPBYTE pData = NULL;
	UINT uSize = sizeof m_lfHyperText;
	if (ini.GetBinary(_T("HyperTextFont"), &pData, &uSize) && uSize == sizeof m_lfHyperText)
		memcpy(&m_lfHyperText, pData, sizeof m_lfHyperText);
	else
		memset(&m_lfHyperText, 0, sizeof m_lfHyperText);
	delete[] pData;

	pData = NULL;
	uSize = sizeof m_lfLogText;
	if (ini.GetBinary(_T("LogTextFont"), &pData, &uSize) && uSize == sizeof m_lfLogText)
		memcpy(&m_lfLogText, pData, sizeof m_lfLogText);
	else
		memset(&m_lfLogText, 0, sizeof m_lfLogText);
	delete[] pData;

	m_crLogError = ini.GetColRef(_T("LogErrorColor"), m_crLogError);
	m_crLogWarning = ini.GetColRef(_T("LogWarningColor"), m_crLogWarning);
	m_crLogSuccess = ini.GetColRef(_T("LogSuccessColor"), m_crLogSuccess);

	if (statsAverageMinutes < 1)
		statsAverageMinutes = 5;

	m_bA4AFSaveCpu = ini.GetBool(_T("A4AFSaveCpu"), false); // ZZ:DownloadManager
	m_bHighresTimer = ini.GetBool(_T("HighresTimer"), false);
	m_byLogLevel = ini.GetInt(_T("DebugLogLevel"), DLP_VERYLOW);
	m_bTrustEveryHash = ini.GetBool(_T("AICHTrustEveryHash"), false);
	m_bRememberCancelledFiles = ini.GetBool(_T("RememberCancelledFiles"), true);
	m_bRememberDownloadedFiles = ini.GetBool(_T("RememberDownloadedFiles"), true);
	m_bPartiallyPurgeOldKnownFiles = ini.GetBool(_T("PartiallyPurgeOldKnownFiles"), true);

	m_bWinaTransToolbar = ini.GetBool(_T("WinaTransToolbar"), true);
	m_bShowDownloadToolbar = ini.GetBool(_T("ShowDownloadToolbar"), true);

	m_bCryptLayerRequested = ini.GetBool(_T("CryptLayerRequested"), true);
	m_bCryptLayerRequired = ini.GetBool(_T("CryptLayerRequired"), false);
	m_bCryptLayerSupported = ini.GetBool(_T("CryptLayerSupported"), true);
	m_dwKadUDPKey = ini.GetInt(_T("KadUDPKey"), GetRandomUInt32());
	m_uKadPublishSourceThrottle = ini.GetInt(_T("KadPublishSourceThrottle"), 10);

	uint32 nTmp = ini.GetInt(_T("CryptTCPPaddingLength"), 128);
	m_byCryptTCPPaddingLength = (uint8)min(nTmp, 254);

	m_bEnableSearchResultFilter = ini.GetBool(_T("EnableSearchResultSpamFilter"), true);

	///////////////////////////////////////////////////////////////////////////
	// Section: "Proxy"
	//
	proxy.bEnablePassword = ini.GetBool(_T("ProxyEnablePassword"), false, _T("Proxy"));
	proxy.bUseProxy = ini.GetBool(_T("ProxyEnableProxy"), false);
	proxy.host = ini.GetString(_T("ProxyName"), _T(""));
	proxy.user = ini.GetString(_T("ProxyUser"), _T(""));
	proxy.password = ini.GetString(_T("ProxyPassword"), _T(""));
	proxy.port = (uint16)ini.GetInt(_T("ProxyPort"), 1080);
	proxy.type = (uint16)ini.GetInt(_T("ProxyType"), PROXYTYPE_NOPROXY);


	///////////////////////////////////////////////////////////////////////////
	// Section: "Statistics"
	//
	statsSaveInterval = ini.GetInt(_T("SaveInterval"), 60, _T("Statistics"));
	statsConnectionsGraphRatio = ini.GetInt(_T("statsConnectionsGraphRatio"), 3);
	m_strStatsExpandedTreeItems = ini.GetString(_T("statsExpandedTreeItems"), _T("111000000100000110000010000011110000010010"));
	CString buffer;
	for (unsigned i = 0; i < _countof(m_adwStatsColors); ++i) {
		buffer.Format(_T("StatColor%u"), i);
		m_adwStatsColors[i] = 0;
		if (_stscanf(ini.GetString(buffer, _T("")), _T("%li"), (long*)&m_adwStatsColors[i]) != 1)
			ResetStatsColor(i);
	}
	m_bHasCustomTaskIconColor = ini.GetBool(_T("HasCustomTaskIconColor"), false);
	m_bShowVerticalHourMarkers = ini.GetBool(_T("ShowVerticalHourMarkers"), true);

	// -khaos--+++> Load Stats
	// I changed this to a separate function because it is now also used
	// to load the stats backup and to load stats from preferences.ini.old.
	LoadStats();
	// <-----khaos-

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	// Section: "UPnP"
	//
	m_bEnableUPnP = ini.GetBool(_T("EnableUPnP"), true, _T("UPnP"));
	m_bSkipWANIPSetup = ini.GetBool(_T("SkipWANIPSetup"), false);
	m_bSkipWANPPPSetup = ini.GetBool(_T("SkipWANPPPSetup"), false);
	m_bCloseUPnPOnExit = ini.GetBool(_T("CloseUPnPOnExit"), true);
	m_nLastWorkingImpl = ini.GetInt(_T("LastWorkingImplementation"), 1 /*MiniUPnPLib*/);
	m_bIsMinilibImplDisabled = ini.GetBool(_T("DisableMiniUPNPLibImpl"), false);
	m_bIsWinServImplDisabled = ini.GetBool(_T("DisableWinServImpl"), false);

	LoadCats();
	SetLanguage();
}

UINT CPreferences::GetDefaultMaxConperFive()
{
	return MAXCONPER5SEC;
}

//////////////////////////////////////////////////////////
// category implementations
//////////////////////////////////////////////////////////

void CPreferences::SaveCats()
{
	CString strCatIniFilePath;
	strCatIniFilePath.Format(_T("%s") _T("Category.ini"), (LPCTSTR)GetMuleDirectory(EMULE_CONFIGDIR));
	(void)_tremove(strCatIniFilePath);
	CIni ini(strCatIniFilePath);
	ini.WriteInt(_T("Count"), (int)catArr.GetCount() - 1, _T("General"));
	for (INT_PTR i = 0; i < catArr.GetCount(); ++i) {
		CString strSection;
		strSection.Format(_T("Cat#%i"), (int)i);
		ini.SetSection(strSection);

		const Category_Struct *cmap = catArr[i];

		ini.WriteStringUTF8(_T("Title"), cmap->strTitle);
		ini.WriteStringUTF8(_T("Incoming"), cmap->strIncomingPath);
		ini.WriteStringUTF8(_T("Comment"), cmap->strComment);
		ini.WriteStringUTF8(_T("RegularExpression"), cmap->regexp);
		ini.WriteInt(_T("Color"), (int)cmap->color);
		ini.WriteInt(_T("a4afPriority"), cmap->prio); // ZZ:DownloadManager
		ini.WriteStringUTF8(_T("AutoCat"), cmap->autocat);
		ini.WriteInt(_T("Filter"), cmap->filter);
		ini.WriteBool(_T("FilterNegator"), cmap->filterNeg);
		ini.WriteBool(_T("AutoCatAsRegularExpression"), cmap->ac_regexpeval);
		ini.WriteBool(_T("downloadInAlphabeticalOrder"), cmap->downloadInAlphabeticalOrder != FALSE);
		ini.WriteBool(_T("Care4All"), cmap->care4all);
	}
}

void CPreferences::LoadCats()
{
	CString strCatIniFilePath;
	strCatIniFilePath.Format(_T("%s") _T("Category.ini"), (LPCTSTR)GetMuleDirectory(EMULE_CONFIGDIR));
	CIni ini(strCatIniFilePath);
	int iNumCategories = ini.GetInt(_T("Count"), 0, _T("General"));
	for (INT_PTR i = 0; i <= iNumCategories; ++i) {
		CString strSection;
		strSection.Format(_T("Cat#%i"), (int)i);
		ini.SetSection(strSection);

		Category_Struct *newcat = new Category_Struct;
		newcat->filter = 0;
		newcat->strTitle = ini.GetStringUTF8(_T("Title"));
		if (i != 0) { // All category
			newcat->strIncomingPath = ini.GetStringUTF8(_T("Incoming"));
			MakeFoldername(newcat->strIncomingPath);
			if (!IsShareableDirectory(newcat->strIncomingPath)
				|| (!::PathFileExists(newcat->strIncomingPath) && !::CreateDirectory(newcat->strIncomingPath, 0)))
			{
				newcat->strIncomingPath = GetMuleDirectory(EMULE_INCOMINGDIR);
			}
		} else
			newcat->strIncomingPath.Empty();
		newcat->strComment = ini.GetStringUTF8(_T("Comment"));
		newcat->prio = ini.GetInt(_T("a4afPriority"), PR_NORMAL); // ZZ:DownloadManager
		newcat->filter = ini.GetInt(_T("Filter"), 0);
		newcat->filterNeg = ini.GetBool(_T("FilterNegator"), FALSE);
		newcat->ac_regexpeval = ini.GetBool(_T("AutoCatAsRegularExpression"), FALSE);
		newcat->care4all = ini.GetBool(_T("Care4All"), FALSE);
		newcat->regexp = ini.GetStringUTF8(_T("RegularExpression"));
		newcat->autocat = ini.GetStringUTF8(_T("Autocat"));
		newcat->downloadInAlphabeticalOrder = ini.GetBool(_T("downloadInAlphabeticalOrder"), FALSE); // ZZ:DownloadManager
		newcat->color = (COLORREF)ini.GetInt(_T("Color"), -1);
		AddCat(newcat);
	}
}

void CPreferences::RemoveCat(INT_PTR index)
{
	if (index >= 0 && index < catArr.GetCount()) {
		const Category_Struct *delcat = catArr[index];
		catArr.RemoveAt(index);
		delete delcat;
	}
}

bool CPreferences::SetCatFilter(INT_PTR index, int filter)
{
	if (index >= 0 && index < catArr.GetCount()) {
		catArr[index]->filter = filter;
		return true;
	}
	return false;
}

int CPreferences::GetCatFilter(INT_PTR index)
{
	if (index >= 0 && index < catArr.GetCount())
		return catArr[index]->filter;
	return 0;
}

bool CPreferences::GetCatFilterNeg(INT_PTR index)
{
	if (index >= 0 && index < catArr.GetCount())
		return catArr[index]->filterNeg;
	return false;
}

void CPreferences::SetCatFilterNeg(INT_PTR index, bool val)
{
	if (index >= 0 && index < catArr.GetCount())
		catArr[index]->filterNeg = val;
}

bool CPreferences::MoveCat(INT_PTR from, INT_PTR to)
{
	if (from >= catArr.GetCount() || to >= catArr.GetCount() + 1 || from == to)
		return false;

	Category_Struct *tomove = catArr[from];
	if (from < to) {
		catArr.RemoveAt(from);
		catArr.InsertAt(to - 1, tomove);
	} else {
		catArr.InsertAt(to, tomove);
		catArr.RemoveAt(from + 1);
	}
	SaveCats();
	return true;
}

DWORD CPreferences::GetCatColor(INT_PTR index, int nDefault)
{
	if (index >= 0 && index < catArr.GetCount()) {
		const COLORREF c = catArr[index]->color;
		if (c != CLR_NONE)
			return c;
	}

	return ::GetSysColor(nDefault);
}


///////////////////////////////////////////////////////

bool CPreferences::IsInstallationDirectory(const CString &rstrDir)
{
	// skip sharing of several special eMule folders
	return EqualPaths(rstrDir, GetMuleDirectory(EMULE_EXECUTABLEDIR))
		|| EqualPaths(rstrDir, GetMuleDirectory(EMULE_CONFIGDIR))
		|| EqualPaths(rstrDir, GetMuleDirectory(EMULE_INSTLANGDIR))
		|| EqualPaths(rstrDir, GetMuleDirectory(EMULE_LOGDIR));
}

bool CPreferences::IsShareableDirectory(const CString &rstrDir)
{
	// skip sharing of several special eMule folders
	if (IsInstallationDirectory(rstrDir))
		return false;
	if (EqualPaths(rstrDir, GetMuleDirectory(EMULE_INCOMINGDIR)))
		return false;
	for (INT_PTR i = GetTempDirCount(); --i >= 0;)
		if (EqualPaths(rstrDir, GetTempDir(i)))		// ".\eMule\temp"
			return false;

	return true;
}

void CPreferences::SetBindNetworkSelection(const CString &strInterfaceId, const CString &strInterfaceName, const CString &strAddress)
{
	m_strBindInterface = strInterfaceId;
	m_strBindInterface.Trim();
	m_strBindInterfaceName = strInterfaceName;
	m_strBindInterfaceName.Trim();
	m_strConfiguredBindAddr = strAddress;
	m_strConfiguredBindAddr.Trim();
}

void CPreferences::SetMaxUpload(uint32 val)
{
	m_maxupload = val ? val : UNLIMITED;
	if (m_maxupload != UNLIMITED)
		maxGraphUploadRate = m_maxupload;
}

void CPreferences::SetMaxDownload(uint32 val)
{
	m_maxdownload = val ? val : UNLIMITED;
	if (m_maxdownload != UNLIMITED)
		maxGraphDownloadRate = m_maxdownload;
}

CString CPreferences::GetHomepageBaseURLForLevel(int nLevel)
{
	CString tmp;
	if (nLevel == 0)
		tmp = _T("https://emule-project.net");
	else if (nLevel == 1)
		tmp = _T("https://www.emule-project.org");
	else if (nLevel == 2)
		tmp = _T("https://www.emule-project.com");
	else if (nLevel < 100)
		tmp.Format(_T("https://www%i.emule-project.net"), nLevel - 2);
	else if (nLevel < 150)
		tmp.Format(_T("https://www%i.emule-project.org"), nLevel);
	else if (nLevel < 200)
		tmp.Format(_T("https://www%i.emule-project.com"), nLevel);
	else if (nLevel == 200)
		tmp = _T("https://emule.sf.net");
	else if (nLevel == 201)
		tmp = _T("https://www.emuleproject.net");
	else if (nLevel == 202)
		tmp = _T("https://sourceforge.net/projects/emule/");
	else
		tmp = _T("https://www.emule-project.net");
	return tmp;
}

bool CPreferences::IsDefaultNick(const CString &strCheck)
{
// not fast, but this function is not called often
	for (int i = 0; i < 255; ++i)
		if (GetHomepageBaseURLForLevel(i) == strCheck)
			return true;

	return strCheck == _T("http://emule-project.net") || strCheck == _T("https://emule-project.net");
}

void CPreferences::SetUserNick(LPCTSTR pszNick)
{
	strNick = pszNick;
}

UINT CPreferences::GetWebMirrorAlertLevel()
{
	// Known upcoming DDoS Attacks
	if (m_nWebMirrorAlertLevel == 0) {
		// no threats known at this time
	}
	// end
	return m_nWebMirrorAlertLevel;
}

bool CPreferences::GetUseReBarToolbar()
{
	return GetReBarToolbar();
}

uint32 CPreferences::GetMaxGraphDownloadRate()
{
	if (m_maxdownload != UNLIMITED)
		return m_maxdownload;
	return GetEstimatedGraphRange(maxGraphDownloadRate, maxGraphDownloadRateEstimated);
}

void CPreferences::EstimateMaxDownloadCap(uint32 nCurrentDownload)
{
	if (maxGraphDownloadRateEstimated + 1 < nCurrentDownload) {
		maxGraphDownloadRateEstimated = nCurrentDownload;
		if (m_maxdownload == UNLIMITED && theApp.emuledlg->statisticswnd)
			theApp.emuledlg->statisticswnd->SetARange(true, thePrefs.GetMaxGraphDownloadRate());
	}
}

uint32 CPreferences::GetMaxGraphUploadRate(bool bEstimateIfUnlimited)
{
	if (m_maxupload != UNLIMITED)
		return m_maxupload;
	if (!bEstimateIfUnlimited)
		return maxGraphUploadRate;
	return GetEstimatedGraphRange(maxGraphUploadRate, maxGraphUploadRateEstimated);
}

void CPreferences::EstimateMaxUploadCap(uint32 nCurrentUpload)
{
	if (maxGraphUploadRateEstimated + 1 < nCurrentUpload) {
		maxGraphUploadRateEstimated = nCurrentUpload;
		if (m_maxupload == UNLIMITED && theApp.emuledlg->statisticswnd)
			theApp.emuledlg->statisticswnd->SetARange(false, thePrefs.GetMaxGraphUploadRate(true));
	}
}

bool CPreferences::CanFSHandleLargeFiles(int nForCat)
{
	for (INT_PTR i = tempdir.GetCount(); --i >= 0;)
		if (!IsFileOnFATVolume(tempdir[i]))
			return !IsFileOnFATVolume((nForCat > 0) ? GetCatPath(nForCat) : GetMuleDirectory(EMULE_INCOMINGDIR));

	return false;
}

uint16 CPreferences::GetRandomTCPPort()
{
	// Get table of currently used TCP ports.
	PMIB_TCPTABLE pTCPTab = NULL;
	ULONG dwSize = 0;
	if (GetTcpTable(pTCPTab, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
		// Allocate more memory in case the number of TCP entries increased
		// between the function calls.
		dwSize += sizeof(MIB_TCPROW) * 50;
		pTCPTab = (PMIB_TCPTABLE)malloc(dwSize);
		if (pTCPTab && GetTcpTable(pTCPTab, &dwSize, TRUE) != ERROR_SUCCESS) {
			free(pTCPTab);
			pTCPTab = NULL;
		}
	}

	static const UINT uValidPortRange = 61000;
	int iMaxTests = uValidPortRange; // just in case, avoid endless loop
	uint16 nPort;
	bool bPortIsFree;
	do {
		// Get random port
		nPort = 4096 + (GetRandomUInt16() % uValidPortRange);

		// The port is assumed to be available by default. If we got a table of currently
		// used TCP ports, verify that the port is not in this table.
		bPortIsFree = true;
		if (pTCPTab) {
			uint16 nPortBE = htons(nPort);
			for (DWORD e = pTCPTab->dwNumEntries; e-- > 0;) {
				// If there is a TCP entry in the table (regardless of its state), the port
				// is treated as unavailable.
				if (pTCPTab->table[e].dwLocalPort == nPortBE) {
					bPortIsFree = false;
					break;
				}
			}
		}
	} while (!bPortIsFree && --iMaxTests > 0);
	free(pTCPTab);
	return nPort;
}

uint16 CPreferences::GetRandomUDPPort()
{
	// Get table of currently used UDP ports.
	PMIB_UDPTABLE pUDPTab = NULL;
	ULONG dwSize = 0;
	if (GetUdpTable(NULL, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
		// Allocate more memory in case the number of UDP entries increased
		// between the function calls.
		dwSize += sizeof(MIB_UDPROW) * 50;
		pUDPTab = (PMIB_UDPTABLE)malloc(dwSize);
		if (pUDPTab && GetUdpTable(pUDPTab, &dwSize, TRUE) != ERROR_SUCCESS) {
			free(pUDPTab);
			pUDPTab = NULL;
		}
	}

	static const UINT uValidPortRange = 61000;
	int iMaxTests = uValidPortRange; // just in case, avoid endless loop
	uint16 nPort;
	bool bPortIsFree;
	do {
		// Get random port
		nPort = 4096 + (GetRandomUInt16() % uValidPortRange);

		// The port is assumed to be available by default. If we got a table of currently
		// used UDP ports, verify that the port is not in this table.
		bPortIsFree = true;
		if (pUDPTab) {
			uint16 nPortBE = htons(nPort);
			for (DWORD e = pUDPTab->dwNumEntries; e-- > 0;) {
				if (pUDPTab->table[e].dwLocalPort == nPortBE) {
					bPortIsFree = false;
					break;
				}
			}
		}
	} while (!bPortIsFree && --iMaxTests > 0);
	free(pUDPTab);
	return nPort;
}

/**
 * Resolve the default directory layout for the Windows 10/11-only branch.
 *
 * The modern layout still supports the existing three user-selectable storage modes:
 * multi-user, public-user, and executable-directory. What is gone here is the old
 * pre-Vista fallback maze which no longer applies to supported systems.
 */
CString CPreferences::GetDefaultDirectory(EDefaultDirectory eDirectory, bool bCreate)
{
	if (m_astrDefaultDirs[0].IsEmpty()) { // already have all directories fetched and stored?

		// Keep the executable directory available for portable installs and as the final fallback.
		TCHAR tchBuffer[MAX_PATH];
		::GetModuleFileName(NULL, tchBuffer, _countof(tchBuffer));
		LPTSTR pszFileName = _tcsrchr(tchBuffer, _T('\\')) + 1;
		*pszFileName = _T('\0');
		m_astrDefaultDirs[EMULE_EXECUTABLEDIR] = tchBuffer;

		// Start with the executable directory so unsupported or incomplete shell-folder state
		// still leaves us with a usable configuration instead of a partially initialized path set.
		CString strSelectedDataBaseDirectory(m_astrDefaultDirs[EMULE_EXECUTABLEDIR]);
		CString strSelectedConfigBaseDirectory(m_astrDefaultDirs[EMULE_EXECUTABLEDIR]);
		CString strSelectedExpansionBaseDirectory(m_astrDefaultDirs[EMULE_EXECUTABLEDIR]);
		m_nCurrentUserDirMode = 2;

		// check if preferences.ini exists already in our default / fallback dir
		bool bConfigAvailableExecutable = (::PathFileExists(strSelectedConfigBaseDirectory + CONFIGFOLDER _T("preferences.ini")) != FALSE);

		// 0 = multi-user, 1 = public-user, 2 = executable-directory.
		DWORD nRegistrySetting = _UI32_MAX;
		CRegKey rkEMuleRegKey;
		if (rkEMuleRegKey.Open(HKEY_CURRENT_USER, _T("Software\\eMule"), KEY_READ) == ERROR_SUCCESS) {
			rkEMuleRegKey.QueryDWORDValue(_T("UsePublicUserDirectories"), nRegistrySetting);
			rkEMuleRegKey.Close();
		}
		if (nRegistrySetting > 2)
			nRegistrySetting = _UI32_MAX;

		PWSTR pszLocalAppData = NULL;
		PWSTR pszPersonalDownloads = NULL;
		PWSTR pszPublicDownloads = NULL;
		PWSTR pszProgramData = NULL;
		if (::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pszLocalAppData) == S_OK
			&& ::SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &pszPersonalDownloads) == S_OK
			&& ::SHGetKnownFolderPath(FOLDERID_PublicDownloads, 0, NULL, &pszPublicDownloads) == S_OK
			&& ::SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &pszProgramData) == S_OK)
		{
			if (   _tcsclen(pszLocalAppData) < MAX_PATH - 30
				&& _tcsclen(pszPersonalDownloads) < MAX_PATH - 40
				&& _tcsclen(pszProgramData) < MAX_PATH - 30
				&& _tcsclen(pszPublicDownloads) < MAX_PATH - 40)
			{
				CString strLocalAppData(pszLocalAppData);
				CString strPersonalDownloads(pszPersonalDownloads);
				CString strPublicDownloads(pszPublicDownloads);
				CString strProgramData(pszProgramData);
				slosh(strLocalAppData);
				slosh(strPersonalDownloads);
				slosh(strPublicDownloads);
				slosh(strProgramData);

				if (nRegistrySetting == _UI32_MAX) {
					if (::PathFileExists(strLocalAppData + _T("eMule\\") CONFIGFOLDER _T("preferences.ini")))
						m_nCurrentUserDirMode = 0;
					else if (::PathFileExists(strProgramData + _T("eMule\\") CONFIGFOLDER _T("preferences.ini")))
						m_nCurrentUserDirMode = 1;
					else if (bConfigAvailableExecutable)
						m_nCurrentUserDirMode = 2;
					else
						m_nCurrentUserDirMode = 0;
				} else {
					m_nCurrentUserDirMode = nRegistrySetting;
				}

				switch (m_nCurrentUserDirMode) {
				case 0:
					strSelectedDataBaseDirectory = strPersonalDownloads + _T("eMule\\");
					strSelectedConfigBaseDirectory = strLocalAppData + _T("eMule\\");
					strSelectedExpansionBaseDirectory = strProgramData + _T("eMule\\");
					break;
				case 1:
					strSelectedDataBaseDirectory = strPublicDownloads + _T("eMule\\");
					strSelectedConfigBaseDirectory = strProgramData + _T("eMule\\");
					strSelectedExpansionBaseDirectory = strProgramData + _T("eMule\\");
					break;
				case 2:
					break;
				default:
					ASSERT(0);
				}
			} else {
				ASSERT(0);
			}
		} else {
			DebugLogError(_T("Unable to retrieve modern shell folders; using executable-directory fallbacks"));
		}
		::CoTaskMemFree(pszLocalAppData);
		::CoTaskMemFree(pszPersonalDownloads);
		::CoTaskMemFree(pszPublicDownloads);
		::CoTaskMemFree(pszProgramData);

		// All the directories (categories also) should have a trailing backslash
		m_astrDefaultDirs[EMULE_CONFIGDIR] = strSelectedConfigBaseDirectory + CONFIGFOLDER;
		m_astrDefaultDirs[EMULE_TEMPDIR] = strSelectedDataBaseDirectory + _T("Temp\\");
		m_astrDefaultDirs[EMULE_INCOMINGDIR] = strSelectedDataBaseDirectory + _T("Incoming\\");
		m_astrDefaultDirs[EMULE_LOGDIR] = strSelectedConfigBaseDirectory + _T("logs\\");
		m_astrDefaultDirs[EMULE_ADDLANGDIR] = strSelectedExpansionBaseDirectory + _T("lang\\");
		m_astrDefaultDirs[EMULE_INSTLANGDIR] = m_astrDefaultDirs[EMULE_EXECUTABLEDIR] + _T("lang\\");
		m_astrDefaultDirs[EMULE_SKINDIR] = strSelectedExpansionBaseDirectory + _T("skins\\");
		m_astrDefaultDirs[EMULE_DATABASEDIR] = strSelectedDataBaseDirectory;
		m_astrDefaultDirs[EMULE_CONFIGBASEDIR] = strSelectedConfigBaseDirectory;
		//               [EMULE_EXECUTABLEDIR] - has been set already
		m_astrDefaultDirs[EMULE_TOOLBARDIR] = m_astrDefaultDirs[EMULE_SKINDIR];
		m_astrDefaultDirs[EMULE_EXPANSIONDIR] = strSelectedExpansionBaseDirectory;

		/*CString strDebug;
		for (int i = 0; i < 12; ++i)
			strDebug += m_astrDefaultDirs[i] + _T('\n');
		AfxMessageBox(strDebug, MB_ICONINFORMATION);*/
	}
	if (bCreate && !m_abDefaultDirsCreated[eDirectory]) {
		switch (eDirectory) { // create the underlying directory first - be sure to adjust this if changing default directories
		case EMULE_CONFIGDIR:
		case EMULE_LOGDIR:
			::CreateDirectory(m_astrDefaultDirs[EMULE_CONFIGBASEDIR], NULL);
			break;
		case EMULE_TEMPDIR:
		case EMULE_INCOMINGDIR:
			::CreateDirectory(m_astrDefaultDirs[EMULE_DATABASEDIR], NULL);
			break;
		case EMULE_ADDLANGDIR:
		case EMULE_SKINDIR:
		case EMULE_TOOLBARDIR:
			::CreateDirectory(m_astrDefaultDirs[EMULE_EXPANSIONDIR], NULL);
		}
		::CreateDirectory(m_astrDefaultDirs[eDirectory], NULL);
		m_abDefaultDirsCreated[eDirectory] = true;
	}
	return m_astrDefaultDirs[eDirectory];
}

CString CPreferences::GetMuleDirectory(EDefaultDirectory eDirectory, bool bCreate)
{
	switch (eDirectory) {
	case EMULE_INCOMINGDIR:
		return m_strIncomingDir;
	case EMULE_TEMPDIR:
		ASSERT(0); // This function can return only the first temp. directory. Use GetTempDir() instead!
		return GetTempDir(0);
	case EMULE_SKINDIR:
		return m_strSkinProfileDir;
	case EMULE_TOOLBARDIR:
		return m_sToolbarBitmapFolder;
	}
	return GetDefaultDirectory(eDirectory, bCreate);
}

void CPreferences::SetMuleDirectory(EDefaultDirectory eDirectory, const CString &strNewDir)
{
	switch (eDirectory) {
	case EMULE_INCOMINGDIR:
		m_strIncomingDir = strNewDir;
		break;
	case EMULE_SKINDIR:
		m_strSkinProfileDir = strNewDir;
		slosh(m_strSkinProfileDir);
		break;
	case EMULE_TOOLBARDIR:
		m_sToolbarBitmapFolder = strNewDir;
		slosh(m_sToolbarBitmapFolder);
		break;
	default:
		ASSERT(0);
	}
}

void CPreferences::ChangeUserDirMode(int nNewMode)
{
	if (m_nCurrentUserDirMode == nNewMode)
		return;
	// 0 = multi-user, 1 = public-user, 2 = executable-directory.
	CRegKey rkEMuleRegKey;
	if (rkEMuleRegKey.Create(HKEY_CURRENT_USER, _T("Software\\eMule")) == ERROR_SUCCESS) {
		if (rkEMuleRegKey.SetDWORDValue(_T("UsePublicUserDirectories"), nNewMode) != ERROR_SUCCESS)
			DebugLogError(_T("Failed to write registry key to switch UserDirMode"));
		else
			m_nCurrentUserDirMode = nNewMode;
		rkEMuleRegKey.Close();
	}
}

bool CPreferences::GetSparsePartFiles()
{
	// The old Vista-only sparse-file blacklist is obsolete on the Windows 10/11
	// baseline used by this branch, so the stored preference can be honored directly.
	return m_bSparsePartFiles;
}

bool CPreferences::IsRunningAeroGlassTheme()
{
	// This is important for all functions which need to draw in the NC-Area (glass style)
	// Aero by default does not allow this, any drawing will not be visible. This can be turned off,
	// but Vista will not deliver the Glass style then as background when calling the default draw
	// function. In other words, draw all or nothing yourself - eMule chooses currently nothing
	static bool bAeroAlreadyDetected = false;
	if (!bAeroAlreadyDetected) {
		bAeroAlreadyDetected = true;
		m_bIsRunningAeroGlass = TRUE;
	}
	return m_bIsRunningAeroGlass != FALSE;
}

