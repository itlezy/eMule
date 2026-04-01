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
#include <vector>

#include "ClientStateDefs.h"
#include "ED2KLink.h"
#include "Emule.h"
#include "EmuleDlg.h"
#include "Log.h"
#include "OtherFunctions.h"
#include "PartFile.h"
#include "PipeApiSurfaceSeams.h"
#include "Preferences.h"
#include "Server.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "ServerWnd.h"
#include "SharedFileList.h"
#include "Statistics.h"
#include "StringConversion.h"
#include "TransferDlg.h"
#include "TransferWnd.h"
#include "UpDownClient.h"
#include "UploadQueue.h"
#include "UserMsgs.h"
#include "DownloadQueue.h"
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
 * Maps the existing CPartFile states to the remote API state names.
 */
CString GetDownloadStateName(const CPartFile &rPartFile)
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
 * Maps the numeric priority into a stable API string.
 */
CString GetPriorityName(const CPartFile &rPartFile)
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
 * Serializes one part file into the API download shape.
 */
json BuildDownloadJson(const CPartFile &rPartFile)
{
	const time_t eta = rPartFile.getTimeRemaining();
	return json{
		{"hash", StdUtf8FromCString(HashToHex(rPartFile.GetFileHash()))},
		{"name", StdUtf8FromCString(rPartFile.GetFileName())},
		{"size", static_cast<uint64>(rPartFile.GetFileSize())},
		{"sizeDone", static_cast<uint64>(rPartFile.GetCompletedSize())},
		{"progress", rPartFile.GetPercentCompleted() / 100.0},
		{"state", StdUtf8FromCString(GetDownloadStateName(rPartFile))},
		{"priority", StdUtf8FromCString(GetPriorityName(rPartFile))},
		{"autoPriority", rPartFile.IsAutoDownPriority()},
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
 * Builds the shared system stats payload used by polling and SSE.
 */
json BuildSystemStatsJson()
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
 * Looks up a download by hash and populates a standard NOT_FOUND error when
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
		rError.strMessage = _T("download not found");
	}
	return pPartFile;
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
		rError.strMessage = _T("download is already being hashed or completed");
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

	if (strCommand == "system/version") {
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

	if (strCommand == "system/stats")
		return BuildSystemStatsJson();

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

	if (strCommand == "uploads/active" || strCommand == "uploads/waiting" || strCommand == "uploads/all") {
		const bool bIncludeActive = strCommand != "uploads/waiting";
		const bool bIncludeWaiting = strCommand != "uploads/active";
		json result = json::array();
		if (bIncludeActive) {
			for (POSITION pos = theApp.uploadqueue->GetFirstFromUploadList(); pos != NULL;) {
				CUpDownClient *const pClient = theApp.uploadqueue->GetNextFromUploadList(pos);
				if (pClient != NULL)
					result.push_back(BuildUploadJson(*pClient, false));
			}
		}
		if (bIncludeWaiting) {
			for (POSITION pos = theApp.uploadqueue->GetFirstFromWaitingList(); pos != NULL;) {
				CUpDownClient *const pClient = theApp.uploadqueue->GetNextFromWaitingList(pos);
				if (pClient != NULL)
					result.push_back(BuildUploadJson(*pClient, true));
			}
		}
		return result;
	}

	if (strCommand == "downloads/list") {
		json result = json::array();
		for (POSITION pos = theApp.downloadqueue->GetFileHeadPosition(); pos != NULL;) {
			CPartFile *pPartFile = theApp.downloadqueue->GetFileNext(pos);
			if (pPartFile != NULL)
				result.push_back(BuildDownloadJson(*pPartFile));
		}
		return result;
	}

	if (strCommand == "downloads/get") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		return pPartFile != NULL ? BuildDownloadJson(*pPartFile) : json();
	}

	if (strCommand == "downloads/sources") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();

		json result = json::array();
		for (POSITION pos = pPartFile->srclist.GetHeadPosition(); pos != NULL;)
			result.push_back(BuildSourceJson(*pPartFile->srclist.GetNext(pos)));
		return result;
	}

	if (strCommand == "downloads/add") {
		if (!params.contains("links") || !params["links"].is_array()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("links must be a string array");
			return json();
		}

		json results = json::array();
		for (const json &linkValue : params["links"]) {
			if (!linkValue.is_string()) {
				results.push_back(json{{"ok", false}, {"error", "link must be a string"}});
				continue;
			}

			CED2KLink *pLink = NULL;
			try {
				CString strLink(CStringFromStdUtf8(linkValue.get<std::string>()));
				strLink.Trim();
				if (strLink.IsEmpty()) {
					results.push_back(json{{"ok", false}, {"error", "link must not be empty"}});
					continue;
				}

				const bool bSlash = (strLink[strLink.GetLength() - 1] == _T('/'));
				pLink = CED2KLink::CreateLinkFromUrl(bSlash ? strLink : strLink + _T('/'));
				if (pLink == NULL || pLink->GetKind() != CED2KLink::kFile)
					throw CString(_T("invalid ed2k link"));

				const CED2KFileLink *pFileLink = pLink->GetFileLink();
				theApp.downloadqueue->AddFileLinkToDownload(*pFileLink, 0);
				CPartFile *pPartFile = theApp.downloadqueue->GetFileByID(pFileLink->GetHashKey());
				if (pPartFile != NULL)
					thePipeApiServer.NotifyDownloadAdded(pPartFile);

				results.push_back(json{
					{"ok", true},
					{"hash", StdUtf8FromCString(HashToHex(pFileLink->GetHashKey()))},
					{"name", StdUtf8FromCString(pFileLink->GetName())}
				});
			} catch (const CString &rLinkError) {
				results.push_back(json{
					{"ok", false},
					{"error", StdUtf8FromCString(rLinkError)}
				});
			}
			delete pLink;
		}
		return json{{"results", results}};
	}

	auto handleBulkMutation = [&](LPCTSTR pszAction) -> json
	{
		if (!params.contains("hashes") || !params["hashes"].is_array()) {
			rError.strCode = "INVALID_ARGUMENT";
			rError.strMessage = _T("hashes must be a string array");
			return json();
		}

		bool bUpdateTabs = false;
		json results = json::array();
		for (const json &hashValue : params["hashes"]) {
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
					strErrorText = _T("download cannot be paused");
			} else if (_tcscmp(pszAction, _T("resume")) == 0) {
				if (pPartFile->CanResumeFile()) {
					if (pPartFile->GetStatus() == PS_INSUFFICIENT)
						pPartFile->ResumeFileInsufficient();
					else
						pPartFile->ResumeFile();
					bOk = true;
				} else
					strErrorText = _T("download cannot be resumed");
			} else if (_tcscmp(pszAction, _T("stop")) == 0) {
				if (pPartFile->CanStopFile()) {
					pPartFile->StopFile();
					bOk = true;
					bUpdateTabs = true;
				} else
					strErrorText = _T("download cannot be stopped");
			} else if (_tcscmp(pszAction, _T("delete")) == 0) {
				const bool bDeleteFiles = params.value("deleteFiles", false);
				if (pPartFile->GetStatus() == PS_COMPLETE) {
					if (!bDeleteFiles) {
						strErrorText = _T("completed download deletion requires deleteFiles=true");
					} else if (!ShellDeleteFile(pPartFile->GetFilePath())) {
						strErrorText = GetErrorMessage(::GetLastError());
					} else {
						theApp.sharedfiles->RemoveFile(pPartFile, true);
						if (theApp.emuledlg->transferwnd->GetDownloadList() != NULL)
							theApp.emuledlg->transferwnd->GetDownloadList()->RemoveFile(pPartFile);
						bOk = true;
					}
				} else if (!bDeleteFiles) {
					strErrorText = _T("partial download deletion requires deleteFiles=true");
				} else {
					pPartFile->DeletePartFile();
					bOk = true;
				}
			}

			results.push_back(BuildMutationResult(strHash, bOk, bOk ? NULL : strErrorText));
			if (bOk && _tcscmp(pszAction, _T("delete")) != 0)
				thePipeApiServer.NotifyDownloadUpdated(pPartFile);
		}

		if (bUpdateTabs)
			theApp.emuledlg->transferwnd->UpdateCatTabTitles();

		return json{{"results", results}};
	};

	if (strCommand == "downloads/pause")
		return handleBulkMutation(_T("pause"));
	if (strCommand == "downloads/resume")
		return handleBulkMutation(_T("resume"));
	if (strCommand == "downloads/stop")
		return handleBulkMutation(_T("stop"));
	if (strCommand == "downloads/delete")
		return handleBulkMutation(_T("delete"));

	if (strCommand == "downloads/recheck") {
		CPartFile *pPartFile = FindPartFileByHash(params.contains("hash") ? params["hash"] : json(), rError);
		if (pPartFile == NULL)
			return json();
		if (!StartPartFileRecheck(*pPartFile, rError))
			return json();
		thePipeApiServer.NotifyDownloadUpdated(pPartFile);
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
	(void)WriteCurrentPipeLine(SerializeEventLine("stats_updated", BuildSystemStatsJson()), PipeApiPolicy::EWriteKind::Stats);
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

void CPipeApiServer::NotifyDownloadAdded(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("download_added", BuildDownloadJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyDownloadRemoved(const CPartFile *pPartFile)
{
	if (pPartFile == NULL)
		return;

	(void)WriteCurrentPipeLine(SerializeEventLine("download_removed", json{
		{"hash", StdUtf8FromCString(HashToHex(pPartFile->GetFileHash()))},
		{"name", StdUtf8FromCString(pPartFile->GetFileName())}
	}), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyDownloadCompleted(const CPartFile *pPartFile, bool bSucceeded)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine(bSucceeded ? "download_completed" : "download_error", BuildDownloadJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
}

void CPipeApiServer::NotifyDownloadUpdated(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("download_updated", BuildDownloadJson(*pPartFile)), PipeApiPolicy::EWriteKind::Structural);
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
		if (hPipe == INVALID_HANDLE_VALUE)
			return;

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
