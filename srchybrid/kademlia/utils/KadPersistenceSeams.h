#pragma once

#include <cstdint>
#include <cstdio>

#include <tchar.h>
#include <windows.h>

#include "LongPathSeams.h"
#include "PartFilePersistenceSeams.h"

namespace Kademlia
{
constexpr uint64_t kKadPrefsFileBytes = sizeof(uint32_t) + sizeof(uint16_t) + 16u + sizeof(uint8_t);

/**
 * @brief Builds the deterministic temporary path used for Kad persistence promotion.
 */
inline CString BuildKadPersistenceTempFilename(LPCTSTR pszTargetPath)
{
	CString strTempPath(pszTargetPath != NULL ? pszTargetPath : _T(""));
	strTempPath += _T(".new");
	return strTempPath;
}

/**
 * @brief Keeps the bootstrap-only `nodes.dat` guard testable without routing-table dependencies.
 */
inline bool ShouldSkipNodesDatSaveForBootstrapOnly(const bool bHasBootstrapContacts, const uint32_t uRoutingContacts)
{
	return bHasBootstrapContacts && uRoutingContacts == 0;
}

/**
 * @brief Documents the sidecar ordering rule for Fast Kad metadata relative to `nodes.dat`.
 */
inline bool ShouldSaveFastKadSidecarAfterNodesPromotion(const bool bNodesDatPromoted)
{
	return bNodesDatPromoted;
}

/**
 * @brief Checks that a `preferencesKad.dat` candidate has the exact shape written by the current saver.
 */
inline bool InspectKadPrefsCandidate(LPCTSTR pszPath)
{
	FILE *pFile = LongPathSeams::OpenFileStreamSharedReadLongPath(pszPath, false);
	if (pFile == NULL)
		return false;

	if (_fseeki64(pFile, 0, SEEK_END) != 0) {
		fclose(pFile);
		return false;
	}
	const __int64 nFileLength = _ftelli64(pFile);
	fclose(pFile);

	return nFileLength == static_cast<__int64>(kKadPrefsFileBytes);
}

/**
 * @brief Promotes a prepared Kad persistence file with shared atomic replacement semantics and long-path preparation.
 */
inline bool PromotePreparedKadFileWithOps(LPCTSTR pszSourcePath, LPCTSTR pszTargetPath, DWORD *pdwLastError, const PartFilePersistenceSeams::FileSystemOps &rOps)
{
	if (pdwLastError != NULL)
		*pdwLastError = ERROR_SUCCESS;

	const CString strPreparedSource(LongPathSeams::PreparePathForLongPath(pszSourcePath).c_str());
	const CString strPreparedTarget(LongPathSeams::PreparePathForLongPath(pszTargetPath).c_str());
	return PartFilePersistenceSeams::TryReplaceFileAtomicallyWithOps(strPreparedSource, strPreparedTarget, pdwLastError, rOps);
}

/**
 * @brief Promotes a prepared Kad persistence file through the default filesystem operations.
 */
inline bool PromotePreparedKadFile(LPCTSTR pszSourcePath, LPCTSTR pszTargetPath, DWORD *pdwLastError = NULL)
{
	return PromotePreparedKadFileWithOps(pszSourcePath, pszTargetPath, pdwLastError, PartFilePersistenceSeams::GetDefaultFileSystemOps());
}

/**
 * @brief Validates and promotes a prepared `preferencesKad.dat` candidate.
 */
inline bool InstallPreparedKadPrefsCandidate(LPCTSTR pszCandidatePath, LPCTSTR pszTargetPath, DWORD *pdwLastError = NULL)
{
	if (pdwLastError != NULL)
		*pdwLastError = ERROR_SUCCESS;

	if (!InspectKadPrefsCandidate(pszCandidatePath)) {
		if (pdwLastError != NULL)
			*pdwLastError = ERROR_INVALID_DATA;
		return false;
	}
	return PromotePreparedKadFile(pszCandidatePath, pszTargetPath, pdwLastError);
}
}
