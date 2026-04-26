#pragma once

#include <algorithm>
#include <cstdint>

#include <atlstr.h>
#include <tchar.h>

#include "PathHelpers.h"

namespace WebServerStaticFileSeams
{
constexpr DWORD kStaticFileChunkSize = 64u * 1024u;

inline int HexNibble(const TCHAR ch)
{
	if (ch >= _T('0') && ch <= _T('9'))
		return ch - _T('0');
	if (ch >= _T('a') && ch <= _T('f'))
		return 10 + (ch - _T('a'));
	if (ch >= _T('A') && ch <= _T('F'))
		return 10 + (ch - _T('A'));
	return -1;
}

inline CString StripRequestSuffix(const CString &rstrRequestTarget)
{
	int iEnd = rstrRequestTarget.Find(_T('?'));
	const int iFragment = rstrRequestTarget.Find(_T('#'));
	if (iEnd < 0 || (iFragment >= 0 && iFragment < iEnd))
		iEnd = iFragment;
	return iEnd >= 0 ? rstrRequestTarget.Left(iEnd) : rstrRequestTarget;
}

inline bool TryDecodeStaticRequestPath(const CString &rstrRequestPath, CString &rstrDecodedPath)
{
	rstrDecodedPath.Empty();
	for (int i = 0; i < rstrRequestPath.GetLength(); ++i) {
		const TCHAR ch = rstrRequestPath[i];
		if (ch != _T('%')) {
			rstrDecodedPath += ch;
			continue;
		}

		if (i + 2 >= rstrRequestPath.GetLength())
			return false;
		const int nHigh = HexNibble(rstrRequestPath[i + 1]);
		const int nLow = HexNibble(rstrRequestPath[i + 2]);
		if (nHigh < 0 || nLow < 0)
			return false;

		const TCHAR chDecoded = static_cast<TCHAR>((nHigh << 4) | nLow);
		if (chDecoded == _T('\0') || PathHelpers::IsPathSeparator(chDecoded))
			return false;

		rstrDecodedPath += chDecoded;
		i += 2;
	}

	return true;
}

inline bool ContainsParentSegment(const CString &rstrRelativePath)
{
	int iIndex = 0;
	CString strSegment;
	while (PathHelpers::ReadNextPathSegment(rstrRelativePath, iIndex, strSegment))
		if (strSegment == _T(".."))
			return true;
	return false;
}

inline bool HasDriveDesignator(const CString &rstrPath)
{
	return rstrPath.GetLength() >= 2
		&& ((rstrPath[0] >= _T('A') && rstrPath[0] <= _T('Z')) || (rstrPath[0] >= _T('a') && rstrPath[0] <= _T('z')))
		&& rstrPath[1] == _T(':');
}

inline bool TryBuildContainedStaticFilePath(const CString &rstrWebRoot, const CString &rstrRequestTarget, CString &rstrFilePath)
{
	rstrFilePath.Empty();
	if (rstrWebRoot.IsEmpty() || rstrRequestTarget.IsEmpty())
		return false;

	CString strDecodedPath;
	if (!TryDecodeStaticRequestPath(StripRequestSuffix(rstrRequestTarget), strDecodedPath))
		return false;

	CString strRelativePath(PathHelpers::NormalizePathSeparators(strDecodedPath));
	if (!strRelativePath.IsEmpty() && PathHelpers::IsPathSeparator(strRelativePath[0]))
		strRelativePath.Delete(0, 1);
	if (strRelativePath.IsEmpty())
		return false;

	const PathHelpers::ParsedPathRoot requestRoot(PathHelpers::ParsePathRoot(strRelativePath));
	if (requestRoot.bAbsolute || !requestRoot.strPrefix.IsEmpty() || HasDriveDesignator(strRelativePath) || strRelativePath.Find(_T(':')) >= 0)
		return false;
	if (ContainsParentSegment(strRelativePath))
		return false;

	const CString strCanonicalRelative(PathHelpers::CanonicalizePath(strRelativePath));
	const PathHelpers::ParsedPathRoot canonicalRoot(PathHelpers::ParsePathRoot(strCanonicalRelative));
	if (strCanonicalRelative.IsEmpty() || strCanonicalRelative == _T(".") || canonicalRoot.bAbsolute || !canonicalRoot.strPrefix.IsEmpty())
		return false;

	const CString strCanonicalWebRoot(PathHelpers::CanonicalizeDirectoryPath(rstrWebRoot));
	const CString strCandidate(PathHelpers::AppendPathComponent(strCanonicalWebRoot, strCanonicalRelative));
	const CString strCanonicalCandidate(PathHelpers::CanonicalizePathForComparison(strCandidate));
	if (!PathHelpers::IsPathWithinDirectory(strCanonicalWebRoot, strCanonicalCandidate))
		return false;

	rstrFilePath = strCanonicalCandidate;
	return true;
}

inline CStringA GetStaticContentTypeHeader(const CString &rstrPath)
{
	CString strPath(StripRequestSuffix(rstrPath));
	const int iDot = strPath.ReverseFind(_T('.'));
	if (iDot < 0 || iDot + 1 >= strPath.GetLength())
		return CStringA();

	CString strExt(strPath.Mid(iDot + 1));
	strExt.MakeLower();
	if (strExt == _T("bmp") || strExt == _T("gif") || strExt == _T("jpeg") || strExt == _T("jpg") || strExt == _T("png")) {
		CStringA strHeader;
		strHeader.Format("Content-Type: image/%s\r\n", static_cast<LPCSTR>(CStringA(strExt)));
		return strHeader;
	}
	if (strExt == _T("ico"))
		return CStringA("Content-Type: image/x-icon\r\n");
	if (strExt == _T("css"))
		return CStringA("Content-Type: text/css\r\n");
	if (strExt == _T("js"))
		return CStringA("Content-Type: text/javascript\r\n");
	return CStringA();
}

inline bool IsStaticFileSizeAllowed(const ULONGLONG ullFileSize, const uint32_t nMaxWebUploadFileSizeMB)
{
	if (nMaxWebUploadFileSizeMB == 0)
		return true;
	return ullFileSize <= static_cast<ULONGLONG>(nMaxWebUploadFileSizeMB) * 1024ull * 1024ull;
}
}
