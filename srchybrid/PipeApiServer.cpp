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

#include <vector>

#include "ClientStateDefs.h"
#include "ED2KLink.h"
#include "Emule.h"
#include "EmuleDlg.h"
#include "Log.h"
#include "OtherFunctions.h"
#include "PartFile.h"
#include "Preferences.h"
#include "ServerConnect.h"
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

struct SPipeApiError
{
	CStringA strId;
	CStringA strCode;
	CString strMessage;
};

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

CPipeApiServer::CPipeApiServer()
	: m_bStopRequested(false)
	, m_bConnected(false)
	, m_hPipe(INVALID_HANDLE_VALUE)
	, m_dwLastStatsEventTick()
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
	m_worker = std::thread(&CPipeApiServer::RunWorker, this);
}

void CPipeApiServer::Stop()
{
	m_bStopRequested.store(true);
	WakePendingConnect();
	DisconnectPipe();

	if (m_worker.joinable()) {
		::CancelSynchronousIo((HANDLE)m_worker.native_handle());
		m_worker.join();
	}

	m_bConnected.store(false);
}

LRESULT CPipeApiServer::OnHandleCommand(WPARAM, LPARAM lParam)
{
	SPipeApiCommandContext *pContext = reinterpret_cast<SPipeApiCommandContext*>(lParam);
	if (pContext != NULL)
		DispatchCommandLine(pContext->strRequestLine, pContext->strResponseLine);
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
	(void)WriteCurrentPipeLine(SerializeEventLine("stats_updated", BuildSystemStatsJson()));
}

void CPipeApiServer::NotifyDownloadAdded(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("download_added", BuildDownloadJson(*pPartFile)));
}

void CPipeApiServer::NotifyDownloadRemoved(const CPartFile *pPartFile)
{
	if (pPartFile == NULL)
		return;

	(void)WriteCurrentPipeLine(SerializeEventLine("download_removed", json{
		{"hash", StdUtf8FromCString(HashToHex(pPartFile->GetFileHash()))},
		{"name", StdUtf8FromCString(pPartFile->GetFileName())}
	}));
}

void CPipeApiServer::NotifyDownloadCompleted(const CPartFile *pPartFile, bool bSucceeded)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine(bSucceeded ? "download_completed" : "download_error", BuildDownloadJson(*pPartFile)));
}

void CPipeApiServer::NotifyDownloadUpdated(const CPartFile *pPartFile)
{
	if (pPartFile != NULL)
		(void)WriteCurrentPipeLine(SerializeEventLine("download_updated", BuildDownloadJson(*pPartFile)));
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

		const BOOL bConnected = ::ConnectNamedPipe(hPipe, NULL)
			? TRUE
			: (::GetLastError() == ERROR_PIPE_CONNECTED);
		if (!bConnected) {
			DisconnectPipe();
			continue;
		}

		m_bConnected.store(true);
		m_strReadBuffer.clear();
		NotifyStatsUpdated(true);
		ProcessClient(hPipe);
		m_bConnected.store(false);
		DisconnectPipe();
	}
}

bool CPipeApiServer::ProcessClient(HANDLE hPipe)
{
	while (!m_bStopRequested.load()) {
		CStringA strRequestLine;
		if (!ReadNextLine(hPipe, strRequestLine))
			return false;

		SPipeApiCommandContext context;
		context.strRequestLine = strRequestLine;

		if (theApp.emuledlg != NULL && ::IsWindow(theApp.emuledlg->GetSafeHwnd()))
			theApp.emuledlg->SendMessage(UM_PIPE_API_COMMAND, 0, reinterpret_cast<LPARAM>(&context));
		else {
			SPipeApiError error;
			error.strCode = "EMULE_UNAVAILABLE";
			error.strMessage = _T("eMule main window is not available");
			context.strResponseLine = SerializeErrorLine(error);
		}

		if (!context.strResponseLine.IsEmpty() && !WriteUtf8Line(hPipe, context.strResponseLine))
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

bool CPipeApiServer::WriteCurrentPipeLine(const CStringA &rSerializedLine)
{
	if (!IsConnected())
		return false;

	HANDLE hPipe = INVALID_HANDLE_VALUE;
	{
		const std::lock_guard<std::mutex> pipeLock(m_pipeMutex);
		hPipe = m_hPipe;
	}

	return hPipe != INVALID_HANDLE_VALUE && WriteUtf8Line(hPipe, rSerializedLine);
}

void CPipeApiServer::DisconnectPipe()
{
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	{
		const std::lock_guard<std::mutex> pipeLock(m_pipeMutex);
		hPipe = m_hPipe;
		m_hPipe = INVALID_HANDLE_VALUE;
	}

	if (hPipe != INVALID_HANDLE_VALUE) {
		::CancelIoEx(hPipe, NULL);
		::DisconnectNamedPipe(hPipe);
		::CloseHandle(hPipe);
	}
}

void CPipeApiServer::WakePendingConnect() const
{
	HANDLE hClient = ::CreateFileW(PIPE_API_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hClient != INVALID_HANDLE_VALUE)
		::CloseHandle(hClient);
}
