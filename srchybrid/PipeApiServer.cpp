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
#include "PipeApiServer.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <vector>

#include "ClientStateDefs.h"
#include "ClientList.h"
#include "ED2KLink.h"
#include "Emule.h"
#include "EmuleDlg.h"
#include "Log.h"
#include "OtherFunctions.h"
#include "PartFile.h"
#include "PipeApiCommandSeams.h"
#include "PipeApiSurfaceSeams.h"
#include "Preferences.h"
#include "SearchDlg.h"
#include "SearchFile.h"
#include "SearchList.h"
#include "SearchParams.h"
#include "SearchResultsWnd.h"
#include "Server.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "ServerWnd.h"
#include "SharedFileList.h"
#include "SharedFilesWnd.h"
#include "Statistics.h"
#include "StringConversion.h"
#include "TransferDlg.h"
#include "TransferWnd.h"
#include "UpDownClient.h"
#include "UploadQueue.h"
#include "UserMsgs.h"
#include "DownloadQueue.h"
#include "Opcodes.h"
#include "kademlia/kademlia/Kademlia.h"

#pragma warning(push, 0)
#include "nlohmann/json.hpp"
#pragma warning(pop)

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using json = nlohmann::json;

namespace
{
static constexpr LPCWSTR PIPE_API_NAME = L"\\\\.\\pipe\\emule-api";
static constexpr DWORD PIPE_API_BUFFER_SIZE = 64 * 1024;
static constexpr DWORD PIPE_API_COMMAND_TIMEOUT_MS = SEC2MS(30);
static constexpr DWORD PIPE_API_COMMAND_POLL_MS = 100;
static constexpr DWORD PIPE_API_WARNING_THROTTLE_MS = SEC2MS(5);

struct SPipeApiError
{
	CStringA strId;
	CStringA strCode;
	CString strMessage;
};

struct SPipeApiServerEndpoint
{
	CString strAddress;
	uint16 uPort;
};

struct SPipeApiClientSelector
{
	SPipeApiClientSelector()
		: bHasUserHash(false)
		, bHasEndpoint(false)
		, dwIp(0)
		, uPort(0)
	{
		md4clr(aucUserHash);
	}

	uchar aucUserHash[MDX_DIGEST_SIZE];
	bool bHasUserHash;
	bool bHasEndpoint;
	uint32 dwIp;
	uint16 uPort;
};

/**
 * Suppresses repeated transport warnings while the pipe remains unhealthy.
 */
bool ShouldLogPipeWarning(std::atomic<DWORD> &rLastTick)
{
	const DWORD dwNow = ::GetTickCount();
	const DWORD dwPrevious = rLastTick.load();
	if (dwPrevious != 0 && dwNow - dwPrevious < PIPE_API_WARNING_THROTTLE_MS)
		return false;

	rLastTick.store(dwNow);
	return true;
}

/**
 * Converts CString text to a UTF-8 std::string for JSON payloads.
 */
std::string StdUtf8FromCString(const CString &rText)
{
	const CStringA utf8(StrToUtf8(rText));
	return std::string((LPCSTR)utf8, utf8.GetLength());
}

/**
 * Converts a UTF-8 std::string into a Unicode CString.
 */
CString CStringFromStdUtf8(const std::string &rText)
{
	return OptUtf8ToStr(CStringA(rText.c_str(), static_cast<int>(rText.size())));
}

/**
 * Converts a narrow CString into a std::string without re-encoding.
 */
std::string StdStringFromCStringA(const CStringA &rText)
{
	return std::string((LPCSTR)rText, rText.GetLength());
}

/**
 * Normalizes a raw MD4 hash into the public 32-character lowercase hex form.
 */
CString HashToHex(const uchar *pHash)
{
	CString strHash(EncodeBase16(pHash, MDX_DIGEST_SIZE));
	strHash.MakeLower();
	return strHash;
}

/**
 * Returns a JSON hash string or null when the caller has no valid MD4.
 */
json JsonHashOrNull(const uchar *pHash)
{
	if (pHash == NULL || isnulmd4(pHash))
		return json(nullptr);
	return StdUtf8FromCString(HashToHex(pHash));
}

/**
 * Maps the existing CPartFile states to the stable transfer state names.
 */
CString GetTransferStateName(const CPartFile &rPartFile)
{
	switch (rPartFile.GetStatus()) {
	case PS_WAITINGFORHASH:
	case PS_HASHING:
		return _T("checking");
	case PS_ERROR:
		return _T("error");
	case PS_INSUFFICIENT:
		return _T("missing_files");
	case PS_PAUSED:
		return _T("paused");
	case PS_COMPLETING:
		return _T("completing");
	case PS_COMPLETE:
		return _T("complete");
	case PS_READY:
	case PS_EMPTY:
	default:
		if (rPartFile.IsStopped())
			return _T("paused");
		return rPartFile.GetTransferringSrcCount() > 0 ? _T("downloading") : _T("stalled");
	}
}

/**
 * Maps the numeric download priority into the stable transfer vocabulary.
 */
CString GetTransferPriorityName(const CPartFile &rPartFile)
{
	if (rPartFile.IsAutoDownPriority())
		return _T("auto");

	switch (rPartFile.GetDownPriority()) {
	case PR_VERYLOW:
		return _T("very_low");
	case PR_LOW:
		return _T("low");
	case PR_HIGH:
		return _T("high");
	case PR_VERYHIGH:
		return _T("very_high");
	case PR_NORMAL:
	default:
		return _T("normal");
	}
}

/**
 * Maps the shared-file upload priority into the public API string.
 */
CString GetUploadPriorityName(const CKnownFile &rKnownFile)
{
	if (rKnownFile.IsAutoUpPriority())
		return _T("auto");

	switch (rKnownFile.GetUpPriority()) {
	case PR_VERYLOW:
		return _T("very_low");
	case PR_LOW:
		return _T("low");
	case PR_HIGH:
		return _T("high");
	case PR_VERYHIGH:
		return _T("very_high");
	case PR_NORMAL:
	default:
		return _T("normal");
	}
}

/**
 * Maps the current upload state into a stable API string.
 */
CString GetUploadStateName(const CUpDownClient &rClient)
{
	return CString(PipeApiSurfaceSeams::GetUploadStateName(static_cast<uint8_t>(rClient.GetUploadState())));
}

/**
 * Returns a JSON timestamp or null when the source value is not available.
 */
json JsonTimeOrNull(time_t value)
{
	return value > 0 ? json(static_cast<int64_t>(value)) : json(nullptr);
}

/**
 * Returns the most useful printable IP string for a source.
 */
CString GetClientIpString(const CUpDownClient &rClient)
{
	const uint32 dwIp = rClient.GetIP() != 0 ? rClient.GetIP() : rClient.GetConnectIP();
	return dwIp != 0 ? ipstr(dwIp) : CString();
}

/**
 * Serializes one server entry together with current connection flags.
 */
json BuildServerJson(const CServer &rServer)
{
	const CServer *const pCurrentServer = theApp.serverconnect->GetCurrentServer();
	const bool bIsCurrent = pCurrentServer == &rServer;
	const bool bConnected = bIsCurrent && theApp.serverconnect->IsConnected();
	const bool bConnecting = bIsCurrent && theApp.serverconnect->IsConnecting();
	return json{
		{"name", StdUtf8FromCString(rServer.GetListName())},
		{"address", StdUtf8FromCString(rServer.GetAddress())},
		{"port", rServer.GetPort()},
		{"ip", StdUtf8FromCString(rServer.GetIP() != 0 ? ipstr(rServer.GetIP()) : CString())},
		{"dynIp", StdUtf8FromCString(rServer.GetDynIP())},
		{"description", StdUtf8FromCString(rServer.GetDescription())},
		{"version", StdUtf8FromCString(rServer.GetVersion())},
		{"users", rServer.GetUsers()},
		{"files", rServer.GetFiles()},
		{"softFiles", rServer.GetSoftFiles()},
		{"hardFiles", rServer.GetHardFiles()},
		{"ping", rServer.GetPing()},
		{"failedCount", rServer.GetFailedCount()},
		{"priority", PipeApiSurfaceSeams::GetServerPriorityName(rServer.GetPreference())},
		{"static", rServer.IsStaticMember()},
		{"current", bIsCurrent},
		{"connected", bConnected},
		{"connecting", bConnecting}
	};
}

/**
 * Serializes the current eD2K connection state and active server details.
 */
json BuildServerStatusJson()
{
	CServer *const pCurrentServer = theApp.serverconnect->GetCurrentServer();
	return json{
		{"connected", theApp.serverconnect->IsConnected()},
		{"connecting", theApp.serverconnect->IsConnecting()},
		{"lowId", theApp.serverconnect->IsConnected() ? json(theApp.serverconnect->IsLowID()) : json(nullptr)},
		{"serverCount", static_cast<int64_t>(theApp.serverlist->GetServerCount())},
		{"currentServer", pCurrentServer != NULL ? BuildServerJson(*pCurrentServer) : json(nullptr)}
	};
}

/**
 * Serializes the current Kad runtime status.
 */
json BuildKadStatusJson()
{
	const bool bRunning = Kademlia::CKademlia::IsRunning();
	const bool bConnected = Kademlia::CKademlia::IsConnected();
	const bool bBootstrapping = Kademlia::CKademlia::IsBootstrapping();
	return json{
		{"running", bRunning},
		{"connected", bConnected},
		{"firewalled", bConnected ? json(Kademlia::CKademlia::IsFirewalled()) : json(nullptr)},
		{"bootstrapping", bBootstrapping},
		{"bootstrapProgress", bBootstrapping ? json(Kademlia::CKademlia::GetBootstrapProgressPercent()) : json(0)},
		{"users", bConnected ? json(Kademlia::CKademlia::GetKademliaUsers()) : json(nullptr)},
		{"files", bConnected ? json(Kademlia::CKademlia::GetKademliaFiles()) : json(nullptr)}
	};
}

/**
 * Serializes one part file into the stable transfer payload.
 */
json BuildTransferJson(const CPartFile &rPartFile)
{
	const time_t eta = rPartFile.getTimeRemaining();
	return json{
		{"hash", StdUtf8FromCString(HashToHex(rPartFile.GetFileHash()))},
		{"name", StdUtf8FromCString(rPartFile.GetFileName())},
		{"size", static_cast<uint64>(rPartFile.GetFileSize())},
		{"sizeDone", static_cast<uint64>(rPartFile.GetCompletedSize())},
		{"progress", rPartFile.GetPercentCompleted() / 100.0},
		{"state", StdUtf8FromCString(GetTransferStateName(rPartFile))},
		{"priority", StdUtf8FromCString(GetTransferPriorityName(rPartFile))},
		{"autoPriority", rPartFile.IsAutoDownPriority()},
		{"category", const_cast<CPartFile&>(rPartFile).GetCategory()},
		{"downloadSpeed", rPartFile.GetDatarate()},
		{"uploadSpeed", 0},
		{"sources", rPartFile.GetSourceCount()},
		{"sourcesTransferring", rPartFile.GetTransferringSrcCount()},
		{"eta", eta >= 0 ? json(static_cast<int64_t>(eta)) : json(nullptr)},
		{"addedAt", JsonTimeOrNull(rPartFile.GetCrFileDate())},
		{"completedAt", JsonTimeOrNull(rPartFile.GetStatus() == PS_COMPLETE ? rPartFile.GetFileDate() : static_cast<time_t>(-1))},
		{"partsTotal", rPartFile.GetPartCount()},
		{"partsAvailable", rPartFile.GetAvailablePartCount()},
		{"stopped", rPartFile.IsStopped()}
	};
}

/**
 * Serializes one active source for a download details request.
 */
json BuildSourceJson(const CUpDownClient &rClient)
{
	const CString strUserName(rClient.GetUserName() != NULL ? rClient.GetUserName() : _T(""));
	return json{
		{"userName", StdUtf8FromCString(strUserName)},
		{"userHash", StdUtf8FromCString(HashToHex(rClient.GetUserHash()))},
		{"clientSoftware", StdUtf8FromCString(rClient.GetClientSoftVer())},
		{"downloadState", StdUtf8FromCString(rClient.DbgGetDownloadState())},
		{"downloadRate", rClient.GetDownloadDatarate()},
		{"availableParts", rClient.GetAvailablePartCount()},
		{"partCount", rClient.GetPartCount()},
		{"ip", StdUtf8FromCString(GetClientIpString(rClient))},
		{"port", rClient.GetUserPort()},
		{"serverIp", StdUtf8FromCString(rClient.GetServerIP() != 0 ? ipstr(rClient.GetServerIP()) : CString())},
		{"serverPort", rClient.GetServerPort()},
		{"lowId", rClient.HasLowID()},
		{"queueRank", rClient.GetRemoteQueueRank()}
	};
}

/**
 * Serializes one shared file with non-localized counters and share metadata.
 */
json BuildSharedFileJson(const CKnownFile &rKnownFile)
{
	return json{
		{"hash", StdUtf8FromCString(HashToHex(rKnownFile.GetFileHash()))},
		{"name", StdUtf8FromCString(rKnownFile.GetFileName())},
		{"path", StdUtf8FromCString(rKnownFile.GetFilePath())},
		{"directory", StdUtf8FromCString(rKnownFile.GetPath())},
		{"size", static_cast<uint64>(rKnownFile.GetFileSize())},
		{"uploadPriority", StdUtf8FromCString(GetUploadPriorityName(rKnownFile))},
		{"autoUploadPriority", rKnownFile.IsAutoUpPriority()},
		{"requests", static_cast<int64_t>(rKnownFile.statistic.GetRequests())},
		{"accepts", static_cast<int64_t>(rKnownFile.statistic.GetAccepts())},
		{"transferred", static_cast<uint64>(rKnownFile.statistic.GetTransferred())},
		{"allTimeRequests", static_cast<int64_t>(rKnownFile.statistic.GetAllTimeRequests())},
		{"allTimeAccepts", static_cast<int64_t>(rKnownFile.statistic.GetAllTimeAccepts())},
		{"allTimeTransferred", static_cast<uint64>(rKnownFile.statistic.GetAllTimeTransferred())},
		{"partCount", rKnownFile.GetPartCount()},
		{"partFile", rKnownFile.IsPartFile()},
		{"complete", !rKnownFile.IsPartFile()},
		{"publishedEd2k", rKnownFile.GetPublishedED2K()},
		{"sharedByRule", theApp.sharedfiles->ShouldBeShared(rKnownFile.GetPath(), rKnownFile.GetFilePath(), false)}
	};
}

/**
 * Serializes one upload or waiting-queue client together with its file hash.
 */
json BuildUploadJson(const CUpDownClient &rClient, const bool bWaitingQueue)
{
	const CString strUserName(rClient.GetUserName() != NULL ? rClient.GetUserName() : _T(""));
	const uchar *const pUploadFileHash = rClient.GetUploadFileID();
	const CKnownFile *const pUploadFile = pUploadFileHash != NULL ? theApp.sharedfiles->GetFileByID(pUploadFileHash) : NULL;
	return json{
		{"userName", StdUtf8FromCString(strUserName)},
		{"userHash", JsonHashOrNull(rClient.HasValidHash() ? rClient.GetUserHash() : NULL)},
		{"clientSoftware", StdUtf8FromCString(rClient.GetClientSoftVer())},
		{"clientMod", StdUtf8FromCString(rClient.GetClientModVer())},
		{"uploadState", StdUtf8FromCString(GetUploadStateName(rClient))},
		{"uploadSpeed", rClient.GetUploadDatarate()},
		{"sessionUploaded", static_cast<uint64>(rClient.GetSessionUp())},
		{"queueSessionUploaded", static_cast<uint64>(rClient.GetQueueSessionPayloadUp())},
		{"payloadBuffered", static_cast<uint64>(rClient.GetPayloadInBuffer())},
		{"waitTimeMs", static_cast<uint64>(rClient.GetWaitTime())},
		{"waitStartedTick", static_cast<uint64>(rClient.GetWaitStartTime())},
		{"score", static_cast<int64_t>(rClient.GetScore(false, rClient.IsDownloading()))},
		{"ip", StdUtf8FromCString(GetClientIpString(rClient))},
		{"port", rClient.GetUserPort()},
		{"serverIp", StdUtf8FromCString(rClient.GetServerIP() != 0 ? ipstr(rClient.GetServerIP()) : CString())},
		{"serverPort", rClient.GetServerPort()},
		{"lowId", rClient.HasLowID()},
		{"friendSlot", rClient.GetFriendSlot()},
		{"uploading", rClient.IsDownloading()},
		{"waitingQueue", bWaitingQueue},
		{"requestedFileHash", JsonHashOrNull(pUploadFileHash)},
		{"requestedFileName", pUploadFile != NULL ? json(StdUtf8FromCString(pUploadFile->GetFileName())) : json(nullptr)},
		{"requestedFileSize", pUploadFile != NULL ? json(static_cast<uint64>(pUploadFile->GetFileSize())) : json(nullptr)}
	};
}

/**
 * Builds the shared global stats payload used by polling and push events.
 */
json BuildGlobalStatsJson()
{
	return json{
		{"connected", theApp.IsConnected()},
		{"downloadSpeed", theApp.downloadqueue->GetDatarate()},
		{"uploadSpeed", theApp.uploadqueue->GetDatarate()},
		{"sessionDownloaded", theStats.sessionReceivedBytes},
		{"sessionUploaded", theStats.sessionSentBytes},
		{"activeUploads", static_cast<int64_t>(theApp.uploadqueue->GetActiveUploadsCount())},
		{"waitingUploads", static_cast<int64_t>(theApp.uploadqueue->GetWaitingUserCount())},
		{"downloadCount", static_cast<int64_t>(theApp.downloadqueue->GetFileCount())},
		{"ed2kConnected", theApp.serverconnect->IsConnected()},
		{"ed2kHighId", theApp.serverconnect->IsConnected() && !theApp.serverconnect->IsLowID()},
		{"kadRunning", Kademlia::CKademlia::IsRunning()},
		{"kadConnected", Kademlia::CKademlia::IsConnected()},
		{"kadFirewalled", Kademlia::CKademlia::IsConnected() ? json(Kademlia::CKademlia::IsFirewalled()) : json(nullptr)}
	};
}

/**
 * Serializes the curated runtime preferences exposed over the local pipe.
 */
json BuildPreferencesJson()
{
	return json{
		{"maxUploadKiB", thePrefs.GetMaxUpload()},
		{"maxDownloadKiB", thePrefs.GetMaxDownload()},
		{"maxConnections", thePrefs.GetMaxConnections()},
		{"maxConPerFive", thePrefs.GetMaxConperFive()},
		{"maxSourcesPerFile", thePrefs.GetMaxSourcePerFileDefault()},
		{"uploadClientDataRate", thePrefs.GetUploadClientDataRate()},
		{"maxUploadSlots", thePrefs.GetMaxUpClientsAllowed()},
		{"queueSize", static_cast<int64_t>(thePrefs.GetQueueSize())},
		{"autoConnect", thePrefs.DoAutoConnect()},
		{"newAutoUp", thePrefs.GetNewAutoUp()},
		{"newAutoDown", thePrefs.GetNewAutoDown()},
		{"creditSystem", thePrefs.UseCreditSystem()},
		{"safeServerConnect", thePrefs.IsSafeServerConnectEnabled()},
		{"networkKademlia", thePrefs.GetNetworkKademlia()},
		{"networkEd2k", thePrefs.GetNetworkED2K()}
	};
}

/**
 * Applies the curated mutable preferences and persists them through the
 * normal preferences save path.
 */
bool ApplyPreferencesJson(const json &rPrefs, SPipeApiError &rError)
{
	if (!rPrefs.is_object()) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("prefs must be an object");
		return false;
	}

	for (json::const_iterator it = rPrefs.begin(); it != rPrefs.end(); ++it) {
		if (PipeApiSurfaceSeams::ParseMutablePreferenceName(it.key().c_str()) == PipeApiSurfaceSeams::EMutablePreference::Invalid) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage.Format(_T("unsupported preference key: %hs"), it.key().c_str());
			return false;
		}
	}

	if (rPrefs.contains("maxUploadKiB")) {
		if (!rPrefs["maxUploadKiB"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxUploadKiB must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxUpload(rPrefs["maxUploadKiB"].get<uint32>());
	}

	if (rPrefs.contains("maxDownloadKiB")) {
		if (!rPrefs["maxDownloadKiB"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxDownloadKiB must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxDownload(rPrefs["maxDownloadKiB"].get<uint32>());
	}

	if (rPrefs.contains("maxConnections")) {
		if (!rPrefs["maxConnections"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxConnections must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxConnections(static_cast<UINT>(rPrefs["maxConnections"].get<unsigned>()));
	}

	if (rPrefs.contains("maxConPerFive")) {
		if (!rPrefs["maxConPerFive"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxConPerFive must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxConsPerFive(static_cast<UINT>(rPrefs["maxConPerFive"].get<unsigned>()));
	}

	if (rPrefs.contains("maxSourcesPerFile")) {
		if (!rPrefs["maxSourcesPerFile"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxSourcesPerFile must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxSourcesPerFile(static_cast<UINT>(rPrefs["maxSourcesPerFile"].get<unsigned>()));
	}

	if (rPrefs.contains("uploadClientDataRate")) {
		if (!rPrefs["uploadClientDataRate"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("uploadClientDataRate must be an unsigned number");
			return false;
		}
		thePrefs.SetUploadClientDataRate(static_cast<UINT>(rPrefs["uploadClientDataRate"].get<unsigned>()));
	}

	if (rPrefs.contains("maxUploadSlots")) {
		if (!rPrefs["maxUploadSlots"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("maxUploadSlots must be an unsigned number");
			return false;
		}
		thePrefs.SetMaxUpClientsAllowed(rPrefs["maxUploadSlots"].get<uint32>());
	}

	if (rPrefs.contains("queueSize")) {
		if (!rPrefs["queueSize"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("queueSize must be an unsigned number");
			return false;
		}
		thePrefs.SetQueueSize(static_cast<INT_PTR>(rPrefs["queueSize"].get<unsigned>()));
	}

	if (rPrefs.contains("autoConnect")) {
		if (!rPrefs["autoConnect"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("autoConnect must be a boolean");
			return false;
		}
		thePrefs.SetAutoConnect(rPrefs["autoConnect"].get<bool>());
	}

	if (rPrefs.contains("newAutoUp")) {
		if (!rPrefs["newAutoUp"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("newAutoUp must be a boolean");
			return false;
		}
		thePrefs.SetNewAutoUp(rPrefs["newAutoUp"].get<bool>());
	}

	if (rPrefs.contains("newAutoDown")) {
		if (!rPrefs["newAutoDown"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("newAutoDown must be a boolean");
			return false;
		}
		thePrefs.SetNewAutoDown(rPrefs["newAutoDown"].get<bool>());
	}

	if (rPrefs.contains("creditSystem")) {
		if (!rPrefs["creditSystem"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("creditSystem must be a boolean");
			return false;
		}
		thePrefs.SetCreditSystem(rPrefs["creditSystem"].get<bool>());
	}

	if (rPrefs.contains("safeServerConnect")) {
		if (!rPrefs["safeServerConnect"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("safeServerConnect must be a boolean");
			return false;
		}
		thePrefs.SetSafeServerConnectEnabled(rPrefs["safeServerConnect"].get<bool>());
	}

	if (rPrefs.contains("networkKademlia")) {
		if (!rPrefs["networkKademlia"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("networkKademlia must be a boolean");
			return false;
		}
		thePrefs.SetNetworkKademlia(rPrefs["networkKademlia"].get<bool>());
	}

	if (rPrefs.contains("networkEd2k")) {
		if (!rPrefs["networkEd2k"].is_boolean()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("networkEd2k must be a boolean");
			return false;
		}
		thePrefs.SetNetworkED2K(rPrefs["networkEd2k"].get<bool>());
	}

	thePrefs.Save();
	theApp.emuledlg->ShowTransferRate(true);
	theApp.emuledlg->ShowConnectionState();
	return true;
}

/**
 * Formats one internal search identifier for the public pipe payload.
 */
CString FormatSearchId(const uint32 uSearchID)
{
	CString strSearchId;
	strSearchId.Format(_T("%u"), uSearchID);
	return strSearchId;
}

/**
 * Parses the public decimal search id into the native uint32 identifier.
 */
bool TryGetSearchId(const json &rValue, uint32 &ruSearchID, SPipeApiError &rError)
{
	std::string strError;
	uint32_t uSearchID = 0;
	if (!PipeApiCommandSeams::TryParseSearchId(rValue, uSearchID, strError)) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = CStringFromStdUtf8(strError);
		return false;
	}

	ruSearchID = static_cast<uint32>(uSearchID);
	return true;
}

/**
 * Maps search result known-file state into stable pipe vocabulary.
 */
CString GetSearchKnownTypeName(const CSearchFile &rSearchFile)
{
	switch (rSearchFile.GetKnownType()) {
	case CSearchFile::Shared:
		return _T("shared");
	case CSearchFile::Downloading:
		return _T("downloading");
	case CSearchFile::Downloaded:
		return _T("downloaded");
	case CSearchFile::Cancelled:
		return _T("cancelled");
	case CSearchFile::Unknown:
		return _T("unknown");
	case CSearchFile::NotDetermined:
	default:
		return _T("undetermined");
	}
}

/**
 * Serializes one top-level search result entry into the pipe payload shape.
 */
json BuildSearchResultJson(const CSearchFile &rSearchFile)
{
	const int iComplete = rSearchFile.IsComplete();
	return json{
		{"searchId", StdUtf8FromCString(FormatSearchId(rSearchFile.GetSearchID()))},
		{"hash", StdUtf8FromCString(HashToHex(rSearchFile.GetFileHash()))},
		{"name", StdUtf8FromCString(rSearchFile.GetFileName())},
		{"size", static_cast<uint64>(rSearchFile.GetFileSize())},
		{"fileType", StdUtf8FromCString(rSearchFile.GetFileType())},
		{"sources", rSearchFile.GetSourceCount()},
		{"completeSources", rSearchFile.GetCompleteSourceCount()},
		{"complete", iComplete >= 0 ? json(iComplete != 0) : json(nullptr)},
		{"knownType", StdUtf8FromCString(GetSearchKnownTypeName(rSearchFile))},
		{"directory", rSearchFile.GetDirectory() != NULL ? json(StdUtf8FromCString(rSearchFile.GetDirectory())) : json(nullptr)},
		{"clientIp", StdUtf8FromCString(rSearchFile.GetClientID() != 0 ? ipstr(rSearchFile.GetClientID()) : CString())},
		{"clientPort", rSearchFile.GetClientPort()},
		{"serverIp", StdUtf8FromCString(rSearchFile.GetClientServerIP() != 0 ? ipstr(rSearchFile.GetClientServerIP()) : CString())},
		{"serverPort", rSearchFile.GetClientServerPort()},
		{"clientCount", rSearchFile.GetClientsCount()},
		{"serverCount", rSearchFile.GetServers().GetSize()},
		{"kadPublishInfo", rSearchFile.GetKadPublishInfo()},
		{"spam", rSearchFile.IsConsideredSpam()}
	};
}

/**
 * Converts a parsed request into the standard success envelope.
 */
CStringA SerializeResponseLine(const CStringA &rId, const json &rResult)
{
	const json response{
		{"id", StdStringFromCStringA(rId)},
		{"result", rResult}
	};
	const std::string strSerialized = response.dump(-1, ' ', false, json::error_handler_t::replace);
	return CStringA(strSerialized.c_str(), static_cast<int>(strSerialized.size()));
}

/**
 * Converts an error description into the standard error envelope.
 */
CStringA SerializeErrorLine(const SPipeApiError &rError)
{
	const json response{
		{"id", StdStringFromCStringA(rError.strId)},
		{"error", {
			{"code", StdStringFromCStringA(rError.strCode)},
			{"message", StdUtf8FromCString(rError.strMessage)}
		}}
	};
	const std::string strSerialized = response.dump(-1, ' ', false, json::error_handler_t::replace);
	return CStringA(strSerialized.c_str(), static_cast<int>(strSerialized.size()));
}

/**
 * Creates one event line ready to be sent over the pipe.
 */
CStringA SerializeEventLine(LPCSTR pszEventName, const json &rData)
{
	const json event{
		{"event", pszEventName},
		{"data", rData}
	};
	const std::string strSerialized = event.dump(-1, ' ', false, json::error_handler_t::replace);
	return CStringA(strSerialized.c_str(), static_cast<int>(strSerialized.size()));
}

/**
 * Extracts the optional request id early so transport-level failures can still
 * be correlated by the sidecar.
 */
CStringA TryGetRequestId(const CStringA &rLine)
{
	try {
		const json request = json::parse(StdStringFromCStringA(rLine), nullptr, false);
		if (request.is_object() && request.contains("id") && request["id"].is_string())
			return CStringA(request["id"].get<std::string>().c_str());
	} catch (const json::exception &) {
	}

	return CStringA();
}

/**
 * Parses and validates a 32-character MD4 hash parameter.
 */
bool TryDecodeHash(const json &rValue, uchar *pOutHash, SPipeApiError &rError)
{
	if (!rValue.is_string()) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("hash must be a 32-character lowercase hex string");
		return false;
	}

	const CString strHash(CStringFromStdUtf8(rValue.get<std::string>()));
	if (strHash.GetLength() != MDX_DIGEST_SIZE * 2 || !DecodeBase16(strHash, strHash.GetLength(), pOutHash, MDX_DIGEST_SIZE)) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("hash must be a valid 32-character lowercase hex string");
		return false;
	}

	return true;
}

/**
 * Looks up a transfer by hash and populates a standard NOT_FOUND error when
 * the entry does not exist.
 */
CPartFile* FindPartFileByHash(const json &rValue, SPipeApiError &rError)
{
	uchar hash[MDX_DIGEST_SIZE];
	if (!TryDecodeHash(rValue, hash, rError))
		return NULL;

	CPartFile *pPartFile = theApp.downloadqueue->GetFileByID(hash);
	if (pPartFile == NULL) {
		rError.strCode = "NOT_FOUND";
		rError.strMessage = _T("transfer not found");
	}
	return pPartFile;
}

/**
 * Parses one non-empty UTF-8 path parameter from a pipe command.
 */
bool TryGetPathParam(const json &rValue, const char *pszFieldName, CString &rPath, SPipeApiError &rError)
{
	if (!rValue.is_string()) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage.Format(_T("%hs must be a non-empty string path"), pszFieldName);
		return false;
	}

	rPath = CStringFromStdUtf8(rValue.get<std::string>());
	rPath.Trim();
	if (rPath.IsEmpty()) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage.Format(_T("%hs must not be empty"), pszFieldName);
		return false;
	}

	return true;
}

/**
 * Finds one shared file by full normalized path.
 */
CKnownFile* FindSharedFileByPath(const CString &rFilePath)
{
	CKnownFilesMap sharedFiles;
	theApp.sharedfiles->CopySharedFileMap(sharedFiles);
	for (const CKnownFilesMap::CPair *pair = sharedFiles.PGetFirstAssoc(); pair != NULL; pair = sharedFiles.PGetNextAssoc(pair)) {
		if (pair->value != NULL && pair->value->GetFilePath().CompareNoCase(rFilePath) == 0)
			return pair->value;
	}
	return NULL;
}

/**
 * Refreshes the shared-files widgets after one explicit share mutation.
 */
void RefreshSharedFilesUi()
{
	if (theApp.emuledlg != NULL && theApp.emuledlg->sharedfileswnd != NULL) {
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.ReloadFileList();
		theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged();
	}
}

/**
 * Parses one upload-control client selector from a pipe payload.
 */
bool TryGetUploadClientSelector(const json &rParams, SPipeApiClientSelector &rSelector, SPipeApiError &rError)
{
	const bool bHasUserHash = rParams.contains("userHash");
	const bool bHasIp = rParams.contains("ip");
	const bool bHasPort = rParams.contains("port");
	if (!bHasUserHash && !bHasIp && !bHasPort) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("userHash or ip and port are required");
		return false;
	}

	if (bHasIp != bHasPort) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("ip and port must be provided together");
		return false;
	}

	if (bHasUserHash) {
		if (!TryDecodeHash(rParams["userHash"], rSelector.aucUserHash, rError)) {
			rError.strMessage = _T("userHash must be a valid 32-character lowercase hex string");
			return false;
		}
		rSelector.bHasUserHash = true;
	}

	if (bHasIp) {
		if (!rParams["ip"].is_string() || !rParams["port"].is_number_unsigned()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("ip must be a string and port must be an unsigned number");
			return false;
		}

		const std::string strIp = rParams["ip"].get<std::string>();
		const unsigned uPort = rParams["port"].get<unsigned>();
		if (uPort == 0 || uPort > 0xFFFFu || !ParseIPv4Address(strIp.c_str(), rSelector.dwIp)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("ip must be a valid IPv4 address and port must be in the range 1..65535");
			return false;
		}

		rSelector.bHasEndpoint = true;
		rSelector.uPort = static_cast<uint16>(uPort);
	}

	return true;
}

/**
 * Resolves one upload-control selector to a live client instance.
 */
CUpDownClient* FindClientForUploadControl(const SPipeApiClientSelector &rSelector, SPipeApiError &rError)
{
	CUpDownClient *pClient = NULL;
	if (rSelector.bHasUserHash)
		pClient = theApp.clientlist->FindClientByUserHash(rSelector.aucUserHash, rSelector.bHasEndpoint ? rSelector.dwIp : 0, rSelector.bHasEndpoint ? rSelector.uPort : 0);
	else if (rSelector.bHasEndpoint)
		pClient = theApp.clientlist->FindClientByIP(rSelector.dwIp, rSelector.uPort);

	if (pClient == NULL) {
		rError.strCode = "NOT_FOUND";
		rError.strMessage = _T("upload client not found");
	}
	return pClient;
}

/**
 * Parses one optional { addr, port } server endpoint from a command payload.
 */
bool TryGetServerEndpoint(const json &rParams, SPipeApiServerEndpoint &rEndpoint, bool &rbHasEndpoint, SPipeApiError &rError)
{
	const bool bHasAddress = rParams.contains("addr");
	const bool bHasPort = rParams.contains("port");
	if (!bHasAddress && !bHasPort) {
		rbHasEndpoint = false;
		return true;
	}

	if (!bHasAddress || !bHasPort || !rParams["addr"].is_string() || !rParams["port"].is_number_unsigned()) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("addr and port must be provided together");
		return false;
	}

	const unsigned uPort = rParams["port"].get<unsigned>();
	rEndpoint.strAddress = CStringFromStdUtf8(rParams["addr"].get<std::string>());
	rEndpoint.strAddress.Trim();
	if (rEndpoint.strAddress.IsEmpty() || uPort == 0 || uPort > 0xFFFFu) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("addr must be non-empty and port must be in the range 1..65535");
		return false;
	}

	rEndpoint.uPort = static_cast<uint16>(uPort);
	rbHasEndpoint = true;
	return true;
}

/**
 * Resolves one server endpoint against the current server list.
 */
CServer* FindServerByEndpoint(const SPipeApiServerEndpoint &rEndpoint)
{
	CServer *pServer = theApp.serverlist->GetServerByAddress(rEndpoint.strAddress, rEndpoint.uPort);
	if (pServer != NULL)
		return pServer;

	const CStringA strAddressA(CT2A(rEndpoint.strAddress));
	IN_ADDR address = {};
	const uint32 dwIp = InetPtonA(AF_INET, strAddressA, &address) == 1 ? address.s_addr : INADDR_NONE;
	return dwIp != INADDR_NONE ? theApp.serverlist->GetServerByIPTCP(dwIp, rEndpoint.uPort) : NULL;
}

/**
 * Starts the existing eMule rehash flow for a part file.
 */
bool StartPartFileRecheck(CPartFile &rPartFile, SPipeApiError &rError)
{
	if (inSet(rPartFile.GetStatus(), PS_WAITINGFORHASH, PS_HASHING, PS_COMPLETING)) {
		rError.strCode = "INVALID_ARGUMENT";
		rError.strMessage = _T("transfer is already being hashed or completed");
		return false;
	}

	CAddFileThread *pAddFileThread = static_cast<CAddFileThread*>(AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
	if (pAddFileThread == NULL) {
		rError.strCode = "EMULE_ERROR";
		rError.strMessage = _T("failed to start recheck thread");
		return false;
	}

	rPartFile.SetStatus(PS_WAITINGFORHASH);
	rPartFile.SetFileOp(PFOP_HASHING);
	rPartFile.SetFileOpProgress(0);
	pAddFileThread->SetValues(NULL, rPartFile.GetPath(), rPartFile.GetPartMetFileName(), &rPartFile);
	rPartFile.SetStatus(PS_HASHING);
	pAddFileThread->ResumeThread();
	return true;
}

/**
 * Builds one per-hash bulk-mutation result row.
 */
json BuildMutationResult(const CString &rHash, bool bOk, LPCTSTR pszError = NULL)
{
	json result{
		{"hash", StdUtf8FromCString(rHash)},
		{"ok", bOk}
	};
	if (!bOk && pszError != NULL)
		result["error"] = StdUtf8FromCString(pszError);
	return result;
}

/**
 * Dispatches one parsed request against the eMule data model on the UI thread.
 */
json HandleUiCommand(const json &rRequest, SPipeApiError &rError)
{
	const std::string strCommand = rRequest.value("cmd", std::string());
	const json params = rRequest.value("params", json::object());
#ifdef _DEBUG
	/** Reports the compile-time build flavor for the Pipe API version response. */
	const char *const pszBuildFlavor = "debug";
#else
	/** Reports the compile-time build flavor for the Pipe API version response. */
	const char *const pszBuildFlavor = "release";
#endif

	if (strCommand == "app/version") {
		return json{
			{"appName", "eMule"},
			{"version", StdUtf8FromCString(theApp.m_strCurVersionLong)},
			{"build", pszBuildFlavor},
#if defined(_M_ARM64)
			{"platform", "arm64"}
#else
			{"platform", "x64"}
#endif
		};
	}

	if (strCommand == "app/preferences/get")
		return BuildPreferencesJson();

	if (strCommand == "app/preferences/set") {
		if (!ApplyPreferencesJson(params.contains("prefs") ? params["prefs"] : json(), rError))
			return json();
		return json{{"ok", true}};
	}

	if (strCommand == "app/shutdown") {
		theApp.emuledlg->PostMessage(WM_CLOSE);
		return json{{"ok", true}};
	}

	if (strCommand == "stats/global")
		return BuildGlobalStatsJson();

	if (strCommand == "servers/list") {
		json result = json::array();
		for (INT_PTR i = 0; i < theApp.serverlist->GetServerCount(); ++i) {
			CServer *const pServer = theApp.serverlist->GetServerAt(i);
			if (pServer != NULL)
				result.push_back(BuildServerJson(*pServer));
		}
		return result;
	}

	if (strCommand == "servers/status")
		return BuildServerStatusJson();

	if (strCommand == "servers/connect") {
		SPipeApiServerEndpoint endpoint;
		bool bHasEndpoint = false;
		if (!TryGetServerEndpoint(params, endpoint, bHasEndpoint, rError))
			return json();

		if (bHasEndpoint) {
			CServer *const pServer = FindServerByEndpoint(endpoint);
			if (pServer == NULL) {
				rError.strCode = "NOT_FOUND";
				rError.strMessage = _T("server not found");
				return json();
			}
			theApp.serverconnect->ConnectToServer(pServer);
		} else
			theApp.serverconnect->ConnectToAnyServer();

		theApp.emuledlg->ShowConnectionState();
		return BuildServerStatusJson();
	}

	if (strCommand == "servers/disconnect") {
		theApp.serverconnect->Disconnect();
		theApp.emuledlg->ShowConnectionState();
		return BuildServerStatusJson();
	}

	if (strCommand == "servers/add") {
		SPipeApiServerEndpoint endpoint;
		bool bHasEndpoint = false;
		if (!TryGetServerEndpoint(params, endpoint, bHasEndpoint, rError))
			return json();
		if (!bHasEndpoint) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("addr and port are required");
			return json();
		}

		CString strName;
		if (params.contains("name")) {
			if (!params["name"].is_string()) {
				rError.strCode = "INVALID_ARGUMENT";
				rError.strMessage = _T("name must be a string when provided");
				return json();
			}
			strName = CStringFromStdUtf8(params["name"].get<std::string>());
		}

		if (theApp.emuledlg->serverwnd == NULL || !theApp.emuledlg->serverwnd->AddServer(endpoint.uPort, endpoint.strAddress, strName, false)) {
			rError.strCode = "EMULE_ERROR";
			rError.strMessage = _T("failed to add server");
			return json();
		}

		CServer *const pServer = FindServerByEndpoint(endpoint);
		return pServer != NULL ? BuildServerJson(*pServer) : json{
			{"name", StdUtf8FromCString(strName)},
			{"address", StdUtf8FromCString(endpoint.strAddress)},
			{"port", endpoint.uPort}
		};
	}

	if (strCommand == "servers/remove") {
		SPipeApiServerEndpoint endpoint;
		bool bHasEndpoint = false;
		if (!TryGetServerEndpoint(params, endpoint, bHasEndpoint, rError))
			return json();
		if (!bHasEndpoint) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("addr and port are required");
			return json();
		}

		CServer *const pServer = FindServerByEndpoint(endpoint);
		if (pServer == NULL) {
			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("server not found");
			return json();
		}

		const json result = BuildServerJson(*pServer);
		theApp.emuledlg->serverwnd->serverlistctrl.RemoveServer(pServer);
		theApp.emuledlg->ShowConnectionState();
		return result;
	}

	if (strCommand == "kad/status")
		return BuildKadStatusJson();

	if (strCommand == "kad/connect") {
		Kademlia::CKademlia::Start();
		theApp.emuledlg->ShowConnectionState();
		return BuildKadStatusJson();
	}

	if (strCommand == "kad/disconnect") {
		Kademlia::CKademlia::Stop();
		theApp.emuledlg->ShowConnectionState();
		return BuildKadStatusJson();
	}

	if (strCommand == "kad/recheck_firewall") {
		Kademlia::CKademlia::RecheckFirewalled();
		theApp.emuledlg->ShowConnectionState();
		return BuildKadStatusJson();
	}

	if (strCommand == "shared/list") {
		CKnownFilesMap sharedFiles;
		theApp.sharedfiles->CopySharedFileMap(sharedFiles);
		json result = json::array();
		for (const CKnownFilesMap::CPair *pair = sharedFiles.PGetFirstAssoc(); pair != NULL; pair = sharedFiles.PGetNextAssoc(pair)) {
			if (pair->value != NULL)
				result.push_back(BuildSharedFileJson(*pair->value));
		}
		return result;
	}

	if (strCommand == "shared/add") {
		CString strFilePath;
		if (!TryGetPathParam(params.contains("path") ? params["path"] : json(), "path", strFilePath, rError))
			return json();

		const int iSlash = strFilePath.ReverseFind(_T('\\'));
		const CString strDirectory = iSlash >= 0 ? strFilePath.Left(iSlash) : CString();
		if (strDirectory.IsEmpty() || !thePrefs.IsShareableDirectory(strDirectory)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("path must point to a file inside a shareable directory");
			return json();
		}

		CKnownFile *const pExistingFile = FindSharedFileByPath(strFilePath);
		if (theApp.sharedfiles->ShouldBeShared(strDirectory, strFilePath, false)) {
			return json{
				{"ok", true},
				{"path", StdUtf8FromCString(strFilePath)},
				{"alreadyShared", true},
				{"queued", false},
				{"file", pExistingFile != NULL ? BuildSharedFileJson(*pExistingFile) : json(nullptr)}
			};
		}

		if (!theApp.sharedfiles->AddSingleSharedFile(strFilePath)) {
			rError.strCode = "EMULE_ERROR";
			rError.strMessage = _T("failed to queue shared file");
			return json();
		}

		RefreshSharedFilesUi();
		CKnownFile *const pSharedFile = FindSharedFileByPath(strFilePath);
		return json{
			{"ok", true},
			{"path", StdUtf8FromCString(strFilePath)},
			{"alreadyShared", false},
			{"queued", pSharedFile == NULL},
			{"file", pSharedFile != NULL ? BuildSharedFileJson(*pSharedFile) : json(nullptr)}
		};
	}

	if (strCommand == "shared/get") {
		uchar hash[MDX_DIGEST_SIZE];
		if (!TryDecodeHash(params.contains("hash") ? params["hash"] : json(), hash, rError))
			return json();

		CKnownFile *const pKnownFile = theApp.sharedfiles->GetFileByID(hash);
		if (pKnownFile == NULL) {
			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("shared file not found");
			return json();
		}
		return BuildSharedFileJson(*pKnownFile);
	}

	if (strCommand == "shared/remove") {
		const bool bHasHash = params.contains("hash");
		const bool bHasPath = params.contains("path");
		if (bHasHash == bHasPath) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("exactly one of hash or path is required");
			return json();
		}

		CString strFilePath;
		CKnownFile *pKnownFile = NULL;
		if (bHasHash) {
			uchar hash[MDX_DIGEST_SIZE];
			if (!TryDecodeHash(params["hash"], hash, rError))
				return json();

			pKnownFile = theApp.sharedfiles->GetFileByID(hash);
			if (pKnownFile == NULL) {
				rError.strCode = "NOT_FOUND";
				rError.strMessage = _T("shared file not found");
				return json();
			}
			strFilePath = pKnownFile->GetFilePath();
		} else {
			if (!TryGetPathParam(params["path"], "path", strFilePath, rError))
				return json();
			pKnownFile = FindSharedFileByPath(strFilePath);
		}

		const int iSlash = strFilePath.ReverseFind(_T('\\'));
		const CString strDirectory = iSlash >= 0 ? strFilePath.Left(iSlash) : CString();
		const bool bIsShared = !strDirectory.IsEmpty() && theApp.sharedfiles->ShouldBeShared(strDirectory, strFilePath, false);
		const bool bMustRemainShared = !strDirectory.IsEmpty() && theApp.sharedfiles->ShouldBeShared(strDirectory, strFilePath, true);
		if (pKnownFile != NULL && pKnownFile->IsPartFile()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("part files cannot be unshared individually");
			return json();
		}
		if (!PipeApiSurfaceSeams::CanRemoveSharedFile(bIsShared, bMustRemainShared)) {
			rError.strCode = bMustRemainShared ? "INVALID_ARGUMENT" : "NOT_FOUND";
			rError.strMessage = bMustRemainShared ? _T("file belongs to a mandatory shared directory") : _T("shared file not found");
			return json();
		}
		if (!theApp.sharedfiles->ExcludeFile(strFilePath)) {
			rError.strCode = "EMULE_ERROR";
			rError.strMessage = _T("failed to remove shared file");
			return json();
		}

		RefreshSharedFilesUi();
		return json{
			{"ok", true},
			{"path", StdUtf8FromCString(strFilePath)},
			{"hash", pKnownFile != NULL ? JsonHashOrNull(pKnownFile->GetFileHash()) : json(nullptr)}
		};
	}

	if (strCommand == "uploads/list" || strCommand == "uploads/queue") {
		const bool bWaitingQueue = strCommand == "uploads/queue";
		json result = json::array();
		if (!bWaitingQueue) {
			for (POSITION pos = theApp.uploadqueue->GetFirstFromUploadList(); pos != NULL;) {
				CUpDownClient *const pClient = theApp.uploadqueue->GetNextFromUploadList(pos);
				if (pClient != NULL)
					result.push_back(BuildUploadJson(*pClient, false));
			}
		} else {
			for (POSITION pos = theApp.uploadqueue->GetFirstFromWaitingList(); pos != NULL;) {
				CUpDownClient *const pClient = theApp.uploadqueue->GetNextFromWaitingList(pos);
				if (pClient != NULL)
					result.push_back(BuildUploadJson(*pClient, true));
			}
		}
		return result;
	}

	if (strCommand == "uploads/remove" || strCommand == "uploads/release_slot") {
		SPipeApiClientSelector selector;
		if (!TryGetUploadClientSelector(params, selector, rError))
			return json();

		CUpDownClient *const pClient = FindClientForUploadControl(selector, rError);
		if (pClient == NULL)
			return json();

		if (strCommand == "uploads/remove") {
			if (theApp.uploadqueue->RemoveFromWaitingQueue(pClient, true))
				return json{{"ok", true}, {"removed", "queue"}};
			if (theApp.uploadqueue->RemoveFromUploadQueue(pClient, _T("Removed by pipe API"), true, true))
				return json{{"ok", true}, {"removed", "slot"}};

			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("upload client is not active or queued");
			return json();
		}

		if (!theApp.uploadqueue->RemoveFromUploadQueue(pClient, _T("Released upload slot by pipe API"), true, true)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("client does not currently hold an upload slot");
			return json();
		}
		return json{{"ok", true}};
	}

	if (strCommand == "transfers/list") {
		PipeApiCommandSeams::STransfersListRequest request;
		std::string strError;
		if (!PipeApiCommandSeams::TryParseTransfersListRequest(params, request, strError)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = CStringFromStdUtf8(strError);
			return json();
		}
		if (request.bHasCategory && request.uCategory >= thePrefs.GetCatCount()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("category is out of range");
			return json();
		}

		const CString strFilter(CStringFromStdUtf8(request.strFilterLower));

		json result = json::array();
		for (POSITION pos = theApp.downloadqueue->GetFileHeadPosition(); pos != NULL;) {
			CPartFile *pPartFile = theApp.downloadqueue->GetFileNext(pos);
			if (pPartFile == NULL)
				continue;
			if (request.bHasCategory && pPartFile->GetCategory() != request.uCategory)
				continue;
			if (!strFilter.IsEmpty()) {
				CString strName(pPartFile->GetFileName());
				strName.MakeLower();
				CString strState(GetTransferStateName(*pPartFile));
				strState.MakeLower();
				if (strName.Find(strFilter) < 0 && strState.Find(strFilter) < 0)
					continue;
			}
			result.push_back(BuildTransferJson(*pPartFile));
		}
		return result;
	}

	if (strCommand == "transfers/get") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		return pPartFile != NULL ? BuildTransferJson(*pPartFile) : json();
	}

	if (strCommand == "transfers/sources") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();

		json result = json::array();
		for (POSITION pos = pPartFile->srclist.GetHeadPosition(); pos != NULL;)
			result.push_back(BuildSourceJson(*pPartFile->srclist.GetNext(pos)));
		return result;
	}

	if (strCommand == "transfers/add") {
		std::string strLinkUtf8;
		std::string strError;
		if (!PipeApiCommandSeams::TryParseTransferAddLink(params, strLinkUtf8, strError)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = CStringFromStdUtf8(strError);
			return json();
		}

		CED2KLink *pLink = NULL;
		try {
			CString strLink(CStringFromStdUtf8(strLinkUtf8));
			const bool bSlash = (strLink[strLink.GetLength() - 1] == _T('/'));
			pLink = CED2KLink::CreateLinkFromUrl(bSlash ? strLink : strLink + _T('/'));
			if (pLink == NULL || pLink->GetKind() != CED2KLink::kFile)
				throw CString(_T("invalid ed2k link"));

			const CED2KFileLink *const pFileLink = pLink->GetFileLink();
			theApp.downloadqueue->AddFileLinkToDownload(*pFileLink, 0);
			CPartFile *const pPartFile = theApp.downloadqueue->GetFileByID(pFileLink->GetHashKey());
			if (pPartFile != NULL)
				thePipeApiServer.NotifyTransferAdded(pPartFile);

			const json result{
				{"hash", StdUtf8FromCString(HashToHex(pFileLink->GetHashKey()))},
				{"name", StdUtf8FromCString(pFileLink->GetName())}
			};
			delete pLink;
			return result;
		} catch (const CString &rLinkError) {
			delete pLink;
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = rLinkError;
			return json();
		}
	}

	auto handleTransferBulkMutation = [&](LPCTSTR pszAction) -> json
	{
		PipeApiCommandSeams::STransferBulkMutationRequest request;
		std::string strError;
		if (!PipeApiCommandSeams::TryParseTransferBulkMutationRequest(params, request, strError)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = CStringFromStdUtf8(strError);
			return json();
		}

		bool bUpdateTabs = false;
		json results = json::array();
		for (const json &hashValue : request.hashes) {
			SPipeApiError itemError = rError;
			CPartFile *pPartFile = FindPartFileByHash(hashValue, itemError);
			if (pPartFile == NULL) {
				results.push_back(json{
					{"hash", hashValue.is_string() ? hashValue : json()},
					{"ok", false},
					{"error", StdUtf8FromCString(itemError.strMessage)}
				});
				continue;
			}

			const CString strHash(HashToHex(pPartFile->GetFileHash()));
			bool bOk = false;
			CString strErrorText;

			if (_tcscmp(pszAction, _T("pause")) == 0) {
				if (pPartFile->CanPauseFile()) {
					pPartFile->PauseFile();
					bOk = true;
				} else
					strErrorText = _T("transfer cannot be paused");
			} else if (_tcscmp(pszAction, _T("resume")) == 0) {
				if (pPartFile->CanResumeFile()) {
					if (pPartFile->GetStatus() == PS_INSUFFICIENT)
						pPartFile->ResumeFileInsufficient();
					else
						pPartFile->ResumeFile();
					bOk = true;
				} else
					strErrorText = _T("transfer cannot be resumed");
			} else if (_tcscmp(pszAction, _T("stop")) == 0) {
				if (pPartFile->CanStopFile()) {
					pPartFile->StopFile();
					bOk = true;
					bUpdateTabs = true;
				} else
					strErrorText = _T("transfer cannot be stopped");
			} else if (_tcscmp(pszAction, _T("delete")) == 0) {
				const bool bDeleteFiles = request.bDeleteFiles;
				if (pPartFile->GetStatus() == PS_COMPLETE) {
					if (!bDeleteFiles) {
						strErrorText = _T("completed transfer deletion requires delete_files=true");
					} else if (!ShellDeleteFile(pPartFile->GetFilePath())) {
						strErrorText = GetErrorMessage(::GetLastError());
					} else {
						theApp.sharedfiles->RemoveFile(pPartFile, true);
						if (theApp.emuledlg->transferwnd->GetDownloadList() != NULL)
							theApp.emuledlg->transferwnd->GetDownloadList()->RemoveFile(pPartFile);
						bOk = true;
					}
				} else if (!bDeleteFiles) {
					strErrorText = _T("partial transfer deletion requires delete_files=true");
				} else {
					pPartFile->DeletePartFile();
					bOk = true;
				}
			}

			results.push_back(BuildMutationResult(strHash, bOk, bOk ? NULL : strErrorText));
			if (bOk && _tcscmp(pszAction, _T("delete")) != 0)
				thePipeApiServer.NotifyTransferUpdated(pPartFile);
		}

		if (bUpdateTabs)
			theApp.emuledlg->transferwnd->UpdateCatTabTitles();

		return json{{"results", results}};
	};

	if (strCommand == "transfers/pause")
		return handleTransferBulkMutation(_T("pause"));
	if (strCommand == "transfers/resume")
		return handleTransferBulkMutation(_T("resume"));
	if (strCommand == "transfers/stop")
		return handleTransferBulkMutation(_T("stop"));
	if (strCommand == "transfers/delete")
		return handleTransferBulkMutation(_T("delete"));

	if (strCommand == "transfers/recheck") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();
		if (!StartPartFileRecheck(*pPartFile, rError))
			return json();
		thePipeApiServer.NotifyTransferUpdated(pPartFile);
		return json{{"ok", true}};
	}

	if (strCommand == "transfers/set_priority") {
		CPartFile *const pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();
		if (!params.contains("priority") || !params["priority"].is_string()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("priority must be a string");
			return json();
		}

		switch (PipeApiSurfaceSeams::ParseTransferPriorityName(params["priority"].get_ref<const std::string&>().c_str())) {
		case PipeApiSurfaceSeams::ETransferPriority::Auto:
			pPartFile->SetAutoDownPriority(true);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::VeryLow:
			pPartFile->SetAutoDownPriority(false);
			pPartFile->SetDownPriority(PR_VERYLOW);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::Low:
			pPartFile->SetAutoDownPriority(false);
			pPartFile->SetDownPriority(PR_LOW);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::Normal:
			pPartFile->SetAutoDownPriority(false);
			pPartFile->SetDownPriority(PR_NORMAL);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::High:
			pPartFile->SetAutoDownPriority(false);
			pPartFile->SetDownPriority(PR_HIGH);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::VeryHigh:
			pPartFile->SetAutoDownPriority(false);
			pPartFile->SetDownPriority(PR_VERYHIGH);
			break;
		case PipeApiSurfaceSeams::ETransferPriority::Invalid:
		default:
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("priority must be one of auto, very_low, low, normal, high, very_high");
			return json();
		}

		pPartFile->UpdateDisplayedInfo(true);
		thePipeApiServer.NotifyTransferUpdated(pPartFile);
		return json{{"ok", true}};
	}

	if (strCommand == "transfers/set_category") {
		CPartFile *const pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();
		uint64_t uRequestedCategory = 0;
		if (!params.contains("category") || !PipeApiCommandSeams::TryParseNonNegativeUInt64(params["category"], uRequestedCategory) || uRequestedCategory > UINT_MAX) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("category must be an unsigned number");
			return json();
		}

		const UINT uCategory = static_cast<UINT>(uRequestedCategory);
		if (uCategory >= thePrefs.GetCatCount()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("category is out of range");
			return json();
		}

		pPartFile->SetCategory(uCategory);
		pPartFile->UpdateDisplayedInfo(true);
		theApp.emuledlg->transferwnd->UpdateCatTabTitles();
		thePipeApiServer.NotifyTransferUpdated(pPartFile);
		return json{{"ok", true}};
	}

	if (strCommand == "search/start") {
		CSearchResultsWnd *const pSearchResults = theApp.emuledlg->searchwnd != NULL ? theApp.emuledlg->searchwnd->m_pwndResults : NULL;
		if (pSearchResults == NULL) {
			rError.strCode = "EMULE_UNAVAILABLE";
			rError.strMessage = _T("search window is not available");
			return json();
		}
		PipeApiCommandSeams::SSearchStartRequest request;
		std::string strError;
		if (!PipeApiCommandSeams::TryParseSearchStartRequest(params, request, strError)) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = CStringFromStdUtf8(strError);
			return json();
		}

		SSearchParams *const pSearchParams = new SSearchParams;
		pSearchParams->strExpression = CStringFromStdUtf8(request.strQuery);
		switch (request.eMethod) {
		case PipeApiCommandSeams::ESearchMethod::Automatic:
			pSearchParams->eType = SearchTypeAutomatic;
			break;
		case PipeApiCommandSeams::ESearchMethod::Server:
			pSearchParams->eType = SearchTypeEd2kServer;
			break;
		case PipeApiCommandSeams::ESearchMethod::Global:
			pSearchParams->eType = SearchTypeEd2kGlobal;
			break;
		case PipeApiCommandSeams::ESearchMethod::Kad:
			pSearchParams->eType = SearchTypeKademlia;
			break;
		case PipeApiCommandSeams::ESearchMethod::Invalid:
		default:
			delete pSearchParams;
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("method must be one of automatic, server, global, kad");
			return json();
		}

		switch (request.eFileType) {
		case PipeApiCommandSeams::ESearchFileType::Any:
			pSearchParams->strFileType = _T(ED2KFTSTR_ANY);
			break;
		case PipeApiCommandSeams::ESearchFileType::Archive:
			pSearchParams->strFileType = _T(ED2KFTSTR_ARCHIVE);
			break;
		case PipeApiCommandSeams::ESearchFileType::Audio:
			pSearchParams->strFileType = _T(ED2KFTSTR_AUDIO);
			break;
		case PipeApiCommandSeams::ESearchFileType::CdImage:
			pSearchParams->strFileType = _T(ED2KFTSTR_CDIMAGE);
			break;
		case PipeApiCommandSeams::ESearchFileType::Image:
			pSearchParams->strFileType = _T(ED2KFTSTR_IMAGE);
			break;
		case PipeApiCommandSeams::ESearchFileType::Program:
			pSearchParams->strFileType = _T(ED2KFTSTR_PROGRAM);
			break;
		case PipeApiCommandSeams::ESearchFileType::Video:
			pSearchParams->strFileType = _T(ED2KFTSTR_VIDEO);
			break;
		case PipeApiCommandSeams::ESearchFileType::Document:
			pSearchParams->strFileType = _T(ED2KFTSTR_DOCUMENT);
			break;
		case PipeApiCommandSeams::ESearchFileType::EmuleCollection:
			pSearchParams->strFileType = _T(ED2KFTSTR_EMULECOLLECTION);
			break;
		case PipeApiCommandSeams::ESearchFileType::Invalid:
		default:
			delete pSearchParams;
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("type is not supported");
			return json();
		}

		pSearchParams->strExtension = CStringFromStdUtf8(request.strExtension);
		if (request.bHasMinSize)
			pSearchParams->ullMinSize = request.ullMinSize;
		if (request.bHasMaxSize)
			pSearchParams->ullMaxSize = request.ullMaxSize;

		CString strSearchError;
		if (!pSearchResults->StartSearchFromApi(pSearchParams, strSearchError)) {
			rError.strCode = "EMULE_ERROR";
			rError.strMessage = strSearchError.IsEmpty() ? CString(_T("failed to start search")) : strSearchError;
			return json();
		}

		return json{{"search_id", StdUtf8FromCString(FormatSearchId(pSearchParams->dwSearchID))}};
	}

	if (strCommand == "search/results") {
		if (theApp.emuledlg->searchwnd == NULL || theApp.emuledlg->searchwnd->m_pwndResults == NULL) {
			rError.strCode = "EMULE_UNAVAILABLE";
			rError.strMessage = _T("search window is not available");
			return json();
		}

		uint32 uSearchID = 0;
		if (!TryGetSearchId(params.contains("search_id") ? params["search_id"] : json(), uSearchID, rError))
			return json();
		if (theApp.emuledlg->searchwnd->m_pwndResults->GetSearchResultsParams(uSearchID) == NULL) {
			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("search not found");
			return json();
		}

		CArray<const CSearchFile*, const CSearchFile*> aResults;
		if (!theApp.searchlist->GetVisibleResults(uSearchID, aResults)) {
			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("search not found");
			return json();
		}

		json results = json::array();
		for (INT_PTR i = 0; i < aResults.GetCount(); ++i)
			results.push_back(BuildSearchResultJson(*aResults[i]));

		return json{
			{"status", theApp.emuledlg->searchwnd->m_pwndResults->IsSearchRunning(uSearchID) ? "running" : "complete"},
			{"results", results}
		};
	}

	if (strCommand == "search/stop") {
		if (theApp.emuledlg->searchwnd == NULL || theApp.emuledlg->searchwnd->m_pwndResults == NULL) {
			rError.strCode = "EMULE_UNAVAILABLE";
			rError.strMessage = _T("search window is not available");
			return json();
		}

		uint32 uSearchID = 0;
		if (!TryGetSearchId(params.contains("search_id") ? params["search_id"] : json(), uSearchID, rError))
			return json();
		if (theApp.emuledlg->searchwnd->m_pwndResults->GetSearchResultsParams(uSearchID) == NULL) {
			rError.strCode = "NOT_FOUND";
			rError.strMessage = _T("search not found");
			return json();
		}

		theApp.emuledlg->searchwnd->m_pwndResults->CancelSearch(uSearchID);
		return json{{"ok", true}};
	}

	if (strCommand == "log/get") {
		const size_t maxEntries = static_cast<size_t>(max(1, params.value("limit", 200)));
		const std::vector<SRecentLogEntry> entries = GetRecentLogEntries(maxEntries);
		json result = json::array();
		for (const SRecentLogEntry &entry : entries) {
			CString strLevel(_T("info"));
			switch (entry.uFlags & LOGMSGTYPEMASK) {
			case LOG_WARNING:
				strLevel = _T("warning");
				break;
			case LOG_ERROR:
				strLevel = _T("error");
				break;
			case LOG_SUCCESS:
				strLevel = _T("success");
				break;
			default:
				break;
			}

			result.push_back(json{
				{"timestamp", static_cast<int64_t>(entry.time.GetTime())},
				{"message", StdUtf8FromCString(entry.strText)},
				{"level", StdUtf8FromCString(strLevel)},
				{"debug", (entry.uFlags & LOG_DEBUG) != 0}
			});
		}
		return result;
	}

	rError.strCode = "INVALID_ARGUMENT";
	rError.strMessage.Format(_T("unknown command: %hs"), strCommand.c_str());
	return json();
}
}

CPipeApiServer thePipeApiServer;

SPipeApiCommandRequest::SPipeApiCommandRequest()
	: hCompletedEvent(::CreateEvent(NULL, TRUE, FALSE, NULL))
	, bStarted(false)
	, bCancelled(false)
{
}

SPipeApiCommandRequest::~SPipeApiCommandRequest()
{
	if (hCompletedEvent != NULL)
		VERIFY(::CloseHandle(hCompletedEvent));
}

CPipeApiServer::CPipeApiServer()
	: m_bStopRequested(false)
	, m_bConnected(false)
	, m_bStatsEventQueued(false)
	, m_uConsecutiveCommandTimeouts(0)
	, m_eLifecycleState(EPipeApiLifecycleState::Stopped)
	, m_dwLastQueueWarningTick(0)
	, m_dwLastTimeoutWarningTick(0)
	, m_dwLastDisconnectWarningTick(0)
	, m_uPendingWriteBytes(0)
	, m_hPipe(INVALID_HANDLE_VALUE)
	, m_dwLastStatsEventTick()
	, m_bHasServerSnapshot(false)
	, m_uLastServerPort(0)
	, m_bHasKadSnapshot(false)
	, m_bLastKadRunning(false)
	, m_bLastKadConnected(false)
	, m_iLastKadFirewalled(-1)
	, m_uLastKadBootstrapProgress(0)
{
}

CPipeApiServer::~CPipeApiServer()
{
	Stop();
}

void CPipeApiServer::Start()
{
	if (m_worker.joinable())
		return;

	m_bStopRequested.store(false);
	SetLifecycleState(EPipeApiLifecycleState::Listening);
	m_writeWorker = std::thread(&CPipeApiServer::RunWriteWorker, this);
	m_worker = std::thread(&CPipeApiServer::RunWorker, this);
}

void CPipeApiServer::Stop()
{
	m_bStopRequested.store(true);
	m_writeCondition.notify_all();
	WakePendingConnect();
	DisconnectPipe();

	if (m_worker.joinable()) {
		::CancelSynchronousIo((HANDLE)m_worker.native_handle());
		m_worker.join();
	}
	if (m_writeWorker.joinable()) {
		::CancelSynchronousIo((HANDLE)m_writeWorker.native_handle());
		m_writeWorker.join();
	}

	m_bConnected.store(false);
	SetLifecycleState(EPipeApiLifecycleState::Stopped);
	ResetConnectionStateSnapshot();
}

LRESULT CPipeApiServer::OnHandleCommand(WPARAM, LPARAM lParam)
{
	SPipeApiCommandContext *pContext = reinterpret_cast<SPipeApiCommandContext*>(lParam);
	if (pContext != NULL)
		DispatchCommandLine(pContext->strRequestLine, pContext->strResponseLine);

	for (;;) {
		std::shared_ptr<SPipeApiCommandRequest> pRequest;
		{
			const std::lock_guard<std::mutex> commandLock(m_commandMutex);
			if (m_pendingCommands.empty())
				break;

			pRequest = m_pendingCommands.front();
			m_pendingCommands.pop_front();
		}

		if (pRequest == NULL)
			continue;

		pRequest->bStarted.store(true);
		if (!pRequest->bCancelled.load() && m_eLifecycleState.load() == EPipeApiLifecycleState::Connected)
			DispatchCommandLine(pRequest->strRequestLine, pRequest->strResponseLine);
		if (pRequest->hCompletedEvent != NULL)
			VERIFY(::SetEvent(pRequest->hCompletedEvent));
	}
	return 0;
}

void CPipeApiServer::NotifyStatsUpdated(bool bForce)
{
	if (!IsConnected())
		return;

	const DWORD dwNow = ::GetTickCount();
	if (!bForce && dwNow - m_dwLastStatsEventTick < SEC2MS(1))
		return;

	m_dwLastStatsEventTick = dwNow;
	(void)WriteCurrentPipeLine(SerializeEventLine("stats_updated", BuildGlobalStatsJson()), PipeApiPolicy::EWriteKind::Stats);
}

void CPipeApiServer::NotifyConnectionStateChanged()
{
	if (!IsConnected())
		return;

	CServer *const pCurrentServer = theApp.serverconnect->GetCurrentServer();
	const bool bServerConnected = theApp.serverconnect->IsConnected() && pCurrentServer != NULL;
	CString strServerAddress;
	if (bServerConnected)
		strServerAddress = pCurrentServer->GetAddress();
	const uint16 uServerPort = bServerConnected ? pCurrentServer->GetPort() : 0;
	const bool bServerChanged = bServerConnected != m_bHasServerSnapshot
		|| (bServerConnected && (uServerPort != m_uLastServerPort || strServerAddress.CompareNoCase(m_strLastServerAddress) != 0));
	if (bServerChanged) {
		if (bServerConnected) {
			(void)WriteCurrentPipeLine(SerializeEventLine("server_connected", BuildServerJson(*pCurrentServer)), PipeApiPolicy::EWriteKind::Structural);
		} else if (!m_strLastServerAddress.IsEmpty() || m_uLastServerPort != 0) {
			(void)WriteCurrentPipeLine(SerializeEventLine("server_disconnected", json{
				{"address", StdUtf8FromCString(m_strLastServerAddress)},
				{"port", m_uLastServerPort}
			}), PipeApiPolicy::EWriteKind::Structural);
		}
	}
	m_bHasServerSnapshot = bServerConnected;
	m_strLastServerAddress = strServerAddress;
	m_uLastServerPort = uServerPort;

	const bool bKadRunning = Kademlia::CKademlia::IsRunning();
	const bool bKadConnected = Kademlia::CKademlia::IsConnected();
	const int iKadFirewalled = bKadConnected ? (Kademlia::CKademlia::IsFirewalled() ? 1 : 0) : -1;
	const uint32 uKadBootstrapProgress = Kademlia::CKademlia::IsBootstrapping() ? Kademlia::CKademlia::GetBootstrapProgressPercent() : 0;
	const bool bKadChanged = !m_bHasKadSnapshot
		|| bKadRunning != m_bLastKadRunning
		|| bKadConnected != m_bLastKadConnected
		|| iKadFirewalled != m_iLastKadFirewalled
		|| uKadBootstrapProgress != m_uLastKadBootstrapProgress;
	if (bKadChanged)
		(void)WriteCurrentPipeLine(SerializeEventLine("kad_status_changed", BuildKadStatusJson()), PipeApiPolicy::EWriteKind::Structural);

	m_bHasKadSnapshot = true;
	m_bLastKadRunning = bKadRunning;
	m_bLastKadConnected = bKadConnected;
	m_iLastKadFirewalled = iKadFirewalled;
	m_uLastKadBootstrapProgress = uKadBootstrapProgress;
}

void CPipeApiServer::NotifyTransferAdded(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("transfer_added", BuildTransferJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyTransferRemoved(const CPartFile *pPartFile)
{
	if (pPartFile == NULL)
		return;

	(void)WriteCurrentPipeLine(SerializeEventLine("transfer_removed", json{
		{"hash", StdUtf8FromCString(HashToHex(pPartFile->GetFileHash()))},
		{"name", StdUtf8FromCString(pPartFile->GetFileName())}
	}), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyTransferCompleted(const CPartFile *pPartFile, bool bSucceeded)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine(bSucceeded ? "transfer_completed" : "transfer_error", BuildTransferJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyTransferUpdated(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("transfer_updated", BuildTransferJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifySearchResultAdded(const CSearchFile *pSearchFile)
{
	if (pSearchFile == NULL || pSearchFile->GetListParent() != NULL || !IsConnected())
		return;

	(void)WriteCurrentPipeLine(SerializeEventLine("search_results", json{
		{"search_id", StdUtf8FromCString(FormatSearchId(pSearchFile->GetSearchID()))},
		{"results", json::array({BuildSearchResultJson(*pSearchFile)})}
	}), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::RunWorker()
{
	while (!m_bStopRequested.load()) {
		HANDLE hPipe = ::CreateNamedPipeW(
			PIPE_API_NAME,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			1,
			PIPE_API_BUFFER_SIZE,
			PIPE_API_BUFFER_SIZE,
			0,
			NULL);
		if (hPipe == INVALID_HANDLE_VALUE) {
			/** Surface pipe startup failures in the disk log so headless stress runs keep the OS error code. */
			DebugLogError(_T("Pipe API server failed to create named pipe %ls: %s"), PIPE_API_NAME, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
			return;
		}

		{
			const std::lock_guard<std::mutex> pipeLock(m_pipeMutex);
			m_hPipe = hPipe;
		}
		SetLifecycleState(EPipeApiLifecycleState::Listening);

		const BOOL bConnected = ::ConnectNamedPipe(hPipe, NULL)
			? TRUE
			: (::GetLastError() == ERROR_PIPE_CONNECTED);
		if (!bConnected) {
			DisconnectPipe();
			continue;
		}

		m_bConnected.store(true);
		m_uConsecutiveCommandTimeouts.store(0);
		SetLifecycleState(EPipeApiLifecycleState::Connected);
		m_strReadBuffer.clear();
		CaptureConnectionStateSnapshot();
		m_writeCondition.notify_all();
		NotifyStatsUpdated(true);
		ProcessClient(hPipe);
		m_bConnected.store(false);
		DisconnectPipe();
	}
}

void CPipeApiServer::RunWriteWorker()
{
	for (;;) {
		SPipeApiWriteEntry entry;
		{
			std::unique_lock<std::mutex> writeQueueLock(m_writeQueueMutex);
			m_writeCondition.wait(writeQueueLock, [this]()
			{
				return m_bStopRequested.load() || !m_pendingWrites.empty();
			});

			if (m_bStopRequested.load())
				return;

			entry = m_pendingWrites.front();
			m_pendingWrites.pop_front();
			m_uPendingWriteBytes -= entry.uQueuedBytes;
			if (entry.eKind == PipeApiPolicy::EWriteKind::Stats)
				m_bStatsEventQueued.store(false);
		}

		HANDLE hPipe = INVALID_HANDLE_VALUE;
		{
			const std::lock_guard<std::mutex> pipeLock(m_pipeMutex);
			hPipe = m_hPipe;
		}

		if (hPipe == INVALID_HANDLE_VALUE || !IsConnected())
			continue;

		if (!WriteUtf8Line(hPipe, entry.strSerializedLine)) {
			m_bConnected.store(false);
			DisconnectPipe();
		}
	}
}

bool CPipeApiServer::ProcessClient(HANDLE hPipe)
{
	while (!m_bStopRequested.load()) {
		CStringA strRequestLine;
		if (!ReadNextLine(hPipe, strRequestLine))
			return false;

		CStringA strResponseLine;
		const CStringA strRequestId(TryGetRequestId(strRequestLine));
		std::shared_ptr<SPipeApiCommandRequest> pRequest = std::make_shared<SPipeApiCommandRequest>();
		pRequest->strRequestLine = strRequestLine;

		const EPipeApiQueueCommandResult eQueueResult =
			pRequest->hCompletedEvent == NULL ? EPipeApiQueueCommandResult::Unavailable : QueueCommandRequest(pRequest);
		if (eQueueResult == EPipeApiQueueCommandResult::Unavailable) {
			SPipeApiError error;
			error.strId = strRequestId;
			error.strCode = "EMULE_UNAVAILABLE";
			error.strMessage = _T("eMule main window is not available");
			strResponseLine = SerializeErrorLine(error);
		} else if (eQueueResult == EPipeApiQueueCommandResult::Busy) {
			SPipeApiError error;
			error.strId = strRequestId;
			error.strCode = "EMULE_BUSY";
			error.strMessage = _T("pipe command queue is saturated");
			strResponseLine = SerializeErrorLine(error);
		} else if (!WaitForCommandResponse(pRequest, strResponseLine)) {
			SPipeApiError error;
			error.strId = strRequestId;
			error.strCode = m_bStopRequested.load() ? "EMULE_UNAVAILABLE" : "EMULE_TIMEOUT";
			error.strMessage = m_bStopRequested.load()
				? _T("pipe API server is stopping")
				: _T("pipe command timed out");
			strResponseLine = SerializeErrorLine(error);

			if (!m_bStopRequested.load()) {
				const unsigned uTimeouts = m_uConsecutiveCommandTimeouts.fetch_add(1) + 1;
				if (ShouldLogPipeWarning(m_dwLastTimeoutWarningTick))
					DebugLogWarning(_T("Pipe API command timed out after %lu ms (%u consecutive timeouts)"), PIPE_API_COMMAND_TIMEOUT_MS, uTimeouts);
				if (uTimeouts >= PipeApiPolicy::kMaxConsecutiveCommandTimeouts)
					ForceDisconnectCurrentPipe(_T("Pipe API disconnected an unresponsive client after repeated command timeouts"));
			}
		} else {
			m_uConsecutiveCommandTimeouts.store(0);
		}

		if (!strResponseLine.IsEmpty() && !WriteUtf8Line(hPipe, strResponseLine))
			return false;
	}
	return true;
}

bool CPipeApiServer::ReadNextLine(HANDLE hPipe, CStringA &rLine)
{
	rLine.Empty();

	for (;;) {
		const std::string::size_type posNewLine = m_strReadBuffer.find('\n');
		if (posNewLine != std::string::npos) {
			std::string strLine(m_strReadBuffer.substr(0, posNewLine));
			m_strReadBuffer.erase(0, posNewLine + 1);
			if (!strLine.empty() && strLine.back() == '\r')
				strLine.pop_back();
			rLine = CStringA(strLine.c_str(), static_cast<int>(strLine.size()));
			return true;
		}

		char acChunk[1024];
		DWORD dwRead = 0;
		if (!::ReadFile(hPipe, acChunk, sizeof acChunk, &dwRead, NULL) || dwRead == 0)
			return false;
		m_strReadBuffer.append(acChunk, dwRead);
	}
}

void CPipeApiServer::DispatchCommandLine(const CStringA &rLine, CStringA &rResponseLine)
{
	SPipeApiError error;
	try {
		const json request = json::parse(StdStringFromCStringA(rLine));
		if (request.contains("id") && request["id"].is_string())
			error.strId = CStringA(request["id"].get<std::string>().c_str());

		if (!request.contains("cmd") || !request["cmd"].is_string()) {
			error.strCode = "INVALID_ARGUMENT";
			error.strMessage = _T("request is missing a string cmd field");
			rResponseLine = SerializeErrorLine(error);
			return;
		}

		const json result = HandleUiCommand(request, error);
		rResponseLine = error.strCode.IsEmpty()
			? SerializeResponseLine(error.strId, result)
			: SerializeErrorLine(error);
	} catch (const json::exception &rJsonError) {
		error.strCode = "INVALID_ARGUMENT";
		error.strMessage.Format(_T("invalid JSON request: %hs"), rJsonError.what());
		rResponseLine = SerializeErrorLine(error);
	}
}

EPipeApiQueueCommandResult CPipeApiServer::QueueCommandRequest(const std::shared_ptr<SPipeApiCommandRequest> &pRequest)
{
	if (pRequest == NULL || !IsAcceptingWork() || theApp.emuledlg == NULL || !::IsWindow(theApp.emuledlg->GetSafeHwnd()))
		return EPipeApiQueueCommandResult::Unavailable;

	{
		const std::lock_guard<std::mutex> commandLock(m_commandMutex);
		if (!PipeApiPolicy::CanQueueCommand(m_pendingCommands.size())) {
			if (ShouldLogPipeWarning(m_dwLastQueueWarningTick))
				DebugLogWarning(_T("Pipe API rejected a command because %u commands are already queued"), (UINT)m_pendingCommands.size());
			return EPipeApiQueueCommandResult::Busy;
		}
		m_pendingCommands.push_back(pRequest);
	}

	if (theApp.emuledlg->PostMessage(UM_PIPE_API_COMMAND, 0, 0))
		return EPipeApiQueueCommandResult::Queued;

	{
		const std::lock_guard<std::mutex> commandLock(m_commandMutex);
		const auto it = std::find(m_pendingCommands.begin(), m_pendingCommands.end(), pRequest);
		if (it != m_pendingCommands.end())
			m_pendingCommands.erase(it);
	}
	return EPipeApiQueueCommandResult::Unavailable;
}

bool CPipeApiServer::WaitForCommandResponse(const std::shared_ptr<SPipeApiCommandRequest> &pRequest, CStringA &rResponseLine) const
{
	if (pRequest == NULL || pRequest->hCompletedEvent == NULL)
		return false;

	const DWORD dwStartTick = ::GetTickCount();
	for (;;) {
		if (m_bStopRequested.load()) {
			pRequest->bCancelled.store(true);
			return false;
		}

		const DWORD dwWaitResult = ::WaitForSingleObject(pRequest->hCompletedEvent, PIPE_API_COMMAND_POLL_MS);
		if (dwWaitResult == WAIT_OBJECT_0) {
			rResponseLine = pRequest->strResponseLine;
			return true;
		}
		if (dwWaitResult == WAIT_FAILED) {
			pRequest->bCancelled.store(true);
			return false;
		}
		if (::GetTickCount() - dwStartTick >= PIPE_API_COMMAND_TIMEOUT_MS) {
			pRequest->bCancelled.store(true);
			if (!pRequest->bStarted.load()) {
				CPipeApiServer *const pMutableThis = const_cast<CPipeApiServer*>(this);
				(void)pMutableThis->TryRemovePendingCommandRequest(pRequest);
			}
			return false;
		}
	}
}

bool CPipeApiServer::EnqueuePipeLine(const CStringA &rSerializedLine, const PipeApiPolicy::EWriteKind eKind)
{
	if (!IsAcceptingWork())
		return false;

	{
		const std::lock_guard<std::mutex> writeQueueLock(m_writeQueueMutex);
		const size_t uQueuedBytes = static_cast<size_t>(rSerializedLine.GetLength()) + 1;
		switch (PipeApiPolicy::GetWriteAction(
			eKind,
			m_pendingWrites.size(),
			m_uPendingWriteBytes,
			uQueuedBytes,
			m_bStatsEventQueued.load()))
		{
		case PipeApiPolicy::EWriteAction::Drop:
			return true;
		case PipeApiPolicy::EWriteAction::Disconnect:
			if (ShouldLogPipeWarning(m_dwLastQueueWarningTick))
				DebugLogWarning(_T("Pipe API disconnected a slow client because the outbound queue hit %u items / %u bytes"),
					(UINT)m_pendingWrites.size(), (UINT)m_uPendingWriteBytes);
			break;
		case PipeApiPolicy::EWriteAction::Queue:
			m_pendingWrites.push_back({rSerializedLine, eKind, uQueuedBytes});
			m_uPendingWriteBytes += uQueuedBytes;
			if (eKind == PipeApiPolicy::EWriteKind::Stats)
				m_bStatsEventQueued.store(true);
			m_writeCondition.notify_one();
			return true;
		}
	}

	ForceDisconnectCurrentPipe(_T("Pipe API disconnected a slow client after outbound queue saturation"), false);
	return false;
}

bool CPipeApiServer::WriteUtf8Line(HANDLE hPipe, const CStringA &rSerializedLine)
{
	const std::lock_guard<std::mutex> writeLock(m_writeMutex);
	if (hPipe == INVALID_HANDLE_VALUE)
		return false;

	CStringA strLine(rSerializedLine);
	strLine += '\n';

	DWORD dwWritten = 0;
	return ::WriteFile(hPipe, (LPCSTR)strLine, static_cast<DWORD>(strLine.GetLength()), &dwWritten, NULL) != FALSE
		&& dwWritten == static_cast<DWORD>(strLine.GetLength());
}

bool CPipeApiServer::WriteCurrentPipeLine(const CStringA &rSerializedLine, const PipeApiPolicy::EWriteKind eKind)
{
	return EnqueuePipeLine(rSerializedLine, eKind);
}

void CPipeApiServer::SetLifecycleState(const EPipeApiLifecycleState eState)
{
	m_eLifecycleState.store(eState);
}

bool CPipeApiServer::IsAcceptingWork() const
{
	return !m_bStopRequested.load() && m_eLifecycleState.load() == EPipeApiLifecycleState::Connected && m_bConnected.load();
}

bool CPipeApiServer::TryRemovePendingCommandRequest(const std::shared_ptr<SPipeApiCommandRequest> &pRequest)
{
	if (pRequest == NULL)
		return false;

	const std::lock_guard<std::mutex> commandLock(m_commandMutex);
	const auto it = std::find(m_pendingCommands.begin(), m_pendingCommands.end(), pRequest);
	if (it == m_pendingCommands.end())
		return false;

	m_pendingCommands.erase(it);
	return true;
}

void CPipeApiServer::CancelPendingCommands()
{
	std::deque<std::shared_ptr<SPipeApiCommandRequest>> pendingCommands;
	{
		const std::lock_guard<std::mutex> commandLock(m_commandMutex);
		pendingCommands.swap(m_pendingCommands);
	}

	for (const std::shared_ptr<SPipeApiCommandRequest> &pRequest : pendingCommands) {
		if (pRequest == NULL)
			continue;

		pRequest->bCancelled.store(true);
		if (pRequest->hCompletedEvent != NULL)
			VERIFY(::SetEvent(pRequest->hCompletedEvent));
	}
}

void CPipeApiServer::ClearPendingWrites()
{
	const std::lock_guard<std::mutex> writeQueueLock(m_writeQueueMutex);
	m_pendingWrites.clear();
	m_uPendingWriteBytes = 0;
	m_bStatsEventQueued.store(false);
}

bool CPipeApiServer::ForceDisconnectCurrentPipe(LPCTSTR pszReason, const bool bLogWarning)
{
	if (bLogWarning && ShouldLogPipeWarning(m_dwLastDisconnectWarningTick))
		DebugLogWarning(_T("%s"), pszReason);

	m_bConnected.store(false);
	DisconnectPipe();
	return false;
}

void CPipeApiServer::DisconnectPipe()
{
	SetLifecycleState(EPipeApiLifecycleState::Disconnecting);
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	{
		const std::lock_guard<std::mutex> pipeLock(m_pipeMutex);
		hPipe = m_hPipe;
		m_hPipe = INVALID_HANDLE_VALUE;
	}

	CancelPendingCommands();
	ClearPendingWrites();
	m_uConsecutiveCommandTimeouts.store(0);
	ResetConnectionStateSnapshot();

	if (hPipe != INVALID_HANDLE_VALUE) {
		::CancelIoEx(hPipe, NULL);
		::DisconnectNamedPipe(hPipe);
		::CloseHandle(hPipe);
	}
	SetLifecycleState(m_bStopRequested.load() ? EPipeApiLifecycleState::Stopped : EPipeApiLifecycleState::Listening);
}

void CPipeApiServer::CaptureConnectionStateSnapshot()
{
	CServer *const pCurrentServer = theApp.serverconnect->GetCurrentServer();
	m_bHasServerSnapshot = theApp.serverconnect->IsConnected() && pCurrentServer != NULL;
	if (m_bHasServerSnapshot)
		m_strLastServerAddress = pCurrentServer->GetAddress();
	else
		m_strLastServerAddress.Empty();
	m_uLastServerPort = m_bHasServerSnapshot ? pCurrentServer->GetPort() : 0;
	m_bHasKadSnapshot = true;
	m_bLastKadRunning = Kademlia::CKademlia::IsRunning();
	m_bLastKadConnected = Kademlia::CKademlia::IsConnected();
	m_iLastKadFirewalled = m_bLastKadConnected ? (Kademlia::CKademlia::IsFirewalled() ? 1 : 0) : -1;
	m_uLastKadBootstrapProgress = Kademlia::CKademlia::IsBootstrapping() ? Kademlia::CKademlia::GetBootstrapProgressPercent() : 0;
}

void CPipeApiServer::ResetConnectionStateSnapshot()
{
	m_bHasServerSnapshot = false;
	m_strLastServerAddress.Empty();
	m_uLastServerPort = 0;
	m_bHasKadSnapshot = false;
	m_bLastKadRunning = false;
	m_bLastKadConnected = false;
	m_iLastKadFirewalled = -1;
	m_uLastKadBootstrapProgress = 0;
}

void CPipeApiServer::WakePendingConnect() const
{
	HANDLE hClient = ::CreateFileW(PIPE_API_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hClient != INVALID_HANDLE_VALUE)
		::CloseHandle(hClient);
}
