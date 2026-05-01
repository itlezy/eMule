#pragma once

#include <tchar.h>

#define EMULE_TEST_HAVE_WEB_SERVER_LEGACY_SEARCH_SEAMS 1

/**
 * @brief Testable policy helpers for legacy WebServer page handling.
 */
namespace WebServerLegacySeams
{
/**
 * @brief Reports whether a legacy eD2K search type token can be forwarded to the search layer.
 */
inline bool IsLegacySearchFileTypeAllowed(const TCHAR *pszFileType)
{
	if (pszFileType == nullptr || pszFileType[0] == _T('\0'))
		return true;

	static const TCHAR *const kAllowedFileTypes[] = {
		_T("Arc"),
		_T("Audio"),
		_T("Iso"),
		_T("Doc"),
		_T("Image"),
		_T("Pro"),
		_T("Video"),
		_T("EmuleCollection")
	};

	for (const TCHAR *pszAllowedFileType : kAllowedFileTypes)
		if (_tcscmp(pszFileType, pszAllowedFileType) == 0)
			return true;
	return false;
}

/**
 * @brief Reports whether an unsupported search type should be cleared before search dispatch.
 */
inline bool ShouldClearUnsupportedLegacySearchFileType(const TCHAR *pszFileType)
{
	return !IsLegacySearchFileTypeAllowed(pszFileType);
}

/**
 * @brief Keeps failed legacy WebServer search starts on caller-owned parameter cleanup.
 */
inline bool ShouldDeleteLegacySearchParamsAfterFailedStart()
{
	return true;
}

/**
 * @brief Keeps unexpected legacy WebServer search exceptions on the existing generic error response.
 */
inline bool ShouldUseGenericLegacySearchErrorAfterException()
{
	return true;
}

/**
 * @brief Keeps gzip-rendering failures on the historical uncompressed response fallback.
 */
inline bool ShouldFallbackToUncompressedResponseAfterGzipFailure()
{
	return true;
}
}
