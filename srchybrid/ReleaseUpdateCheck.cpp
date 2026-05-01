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
#include <afxinet.h>
#include <string>
#include "ReleaseUpdateCheck.h"
#include "ReleaseUpdateCheckSeams.h"
#include "Preferences.h"
#include "Version.h"
#include "WinInetHandle.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	const DWORD kReleaseCheckTimeoutMs = 7000;
	const size_t kMaxReleaseJsonBytes = 512u * 1024u;

	CString CStringFromUtf8(const std::string &strValue)
	{
		if (strValue.empty())
			return CString();
		return CString(CA2T(strValue.c_str(), CP_UTF8));
	}

	CString GetReleaseCheckUserAgent()
	{
		CString strUserAgent;
		strUserAgent.Format(_T("eMule-BB/%s"), MOD_RELEASE_VERSION_TEXT);
		return strUserAgent;
	}

	bool FetchLatestReleaseJson(std::string &strJson, CString &strError)
	{
		strJson.clear();
		strError.Empty();

		WinInetUtil::CInternetHandle hInternetSession(::InternetOpen(GetReleaseCheckUserAgent(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
		if (!hInternetSession) {
			strError.Format(_T("InternetOpen failed (%u)"), ::GetLastError());
			return false;
		}

		DWORD dwTimeoutMs = kReleaseCheckTimeoutMs;
		::InternetSetOption(hInternetSession.Get(), INTERNET_OPTION_CONNECT_TIMEOUT, &dwTimeoutMs, sizeof(dwTimeoutMs));
		::InternetSetOption(hInternetSession.Get(), INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeoutMs, sizeof(dwTimeoutMs));
		::InternetSetOption(hInternetSession.Get(), INTERNET_OPTION_SEND_TIMEOUT, &dwTimeoutMs, sizeof(dwTimeoutMs));

		const CString strHeaders(_T("Accept: application/vnd.github+json\r\n")
			_T("Accept-Encoding: identity\r\n")
			_T("X-GitHub-Api-Version: 2022-11-28\r\n"));
		const DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_SECURE;
		WinInetUtil::CInternetHandle hHttpFile(::InternetOpenUrl(hInternetSession.Get(),
			thePrefs.GetVersionCheckApiURL(),
			strHeaders,
			strHeaders.GetLength(),
			dwFlags,
			0));
		if (!hHttpFile) {
			strError.Format(_T("InternetOpenUrl failed (%u)"), ::GetLastError());
			return false;
		}

		DWORD dwStatusCode = 0;
		DWORD dwStatusLength = sizeof(dwStatusCode);
		if (!::HttpQueryInfo(hHttpFile.Get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatusCode, &dwStatusLength, NULL) || dwStatusCode != HTTP_STATUS_OK) {
			strError.Format(_T("Unexpected HTTP status %u"), static_cast<unsigned>(dwStatusCode));
			return false;
		}

		BYTE buffer[16 * 1024] = {};
		DWORD dwBytesRead = 0;
		do {
			if (!::InternetReadFile(hHttpFile.Get(), buffer, sizeof(buffer), &dwBytesRead)) {
				strError.Format(_T("InternetReadFile failed (%u)"), ::GetLastError());
				return false;
			}

			if (dwBytesRead != 0) {
				if (strJson.size() + dwBytesRead > kMaxReleaseJsonBytes) {
					strError = _T("Latest-release JSON response is too large.");
					return false;
				}
				strJson.append(reinterpret_cast<const char*>(buffer), dwBytesRead);
			}
		} while (dwBytesRead != 0);

		return true;
	}
}

ReleaseUpdateCheck::SUpdateCheckResult ReleaseUpdateCheck::CheckLatestRelease()
{
	SUpdateCheckResult result;

	std::string strJson;
	if (!FetchLatestReleaseJson(strJson, result.strError))
		return result;

	const ReleaseUpdateCheckSeams::SModReleaseVersion localVersion = {
		MOD_RELEASE_VERSION_MAJOR,
		MOD_RELEASE_VERSION_MINOR,
		MOD_RELEASE_VERSION_PATCH
	};
	ReleaseUpdateCheckSeams::SReleaseEvaluation evaluation = ReleaseUpdateCheckSeams::EvaluateLatestReleaseJson(
		strJson,
		localVersion,
		ReleaseUpdateCheckSeams::GetCurrentPlatformAssetToken());

	result.strLatestVersion = CStringFromUtf8(ReleaseUpdateCheckSeams::FormatReleaseVersion(evaluation.version));
	result.strReleaseUrl = CStringFromUtf8(evaluation.strReleaseUrl);
	result.strRequiredAssetName = CStringFromUtf8(evaluation.strRequiredAssetName);
	result.strError = CStringFromUtf8(evaluation.strError);

	switch (evaluation.eStatus) {
	case ReleaseUpdateCheckSeams::EReleaseEvaluationStatus::Newer:
		result.eStatus = EUpdateCheckStatus::NewerVersionAvailable;
		break;
	case ReleaseUpdateCheckSeams::EReleaseEvaluationStatus::NotNewer:
		result.eStatus = EUpdateCheckStatus::NoNewerVersion;
		break;
	case ReleaseUpdateCheckSeams::EReleaseEvaluationStatus::MissingAsset:
	case ReleaseUpdateCheckSeams::EReleaseEvaluationStatus::IgnoredRelease:
	case ReleaseUpdateCheckSeams::EReleaseEvaluationStatus::ParseFailed:
	default:
		result.eStatus = EUpdateCheckStatus::Failed;
		if (result.strError.IsEmpty())
			result.strError = _T("Latest release could not be evaluated.");
		break;
	}

	return result;
}
