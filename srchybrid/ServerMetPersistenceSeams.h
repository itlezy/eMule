#pragma once

#include <cstdint>
#include <cstdio>

#include <tchar.h>
#include <windows.h>

#include "LongPathSeams.h"
#include "PartFilePersistenceSeams.h"

/**
 * @brief Testable helpers for non-destructive `server.met` replacement.
 */
namespace ServerMetPersistenceSeams
{
constexpr BYTE kCurrentServerMetHeader = 0xE0;
constexpr BYTE kLegacyServerMetHeader = 0x0E;
constexpr uint64_t kServerMetHeaderBytes = sizeof(BYTE) + sizeof(uint32_t);
constexpr uint64_t kMinimumServerRecordBytes = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint32_t);

/**
 * @brief Reports the structural metadata read from a `server.met` candidate.
 */
struct ServerMetCandidateInfo
{
	uint32_t ServerCount = 0;
};

/**
 * @brief Returns whether a file header is accepted by the current server-list loader.
 */
inline bool IsAcceptedServerMetHeader(const BYTE byHeader)
{
	return byHeader == kCurrentServerMetHeader || byHeader == kLegacyServerMetHeader;
}

/**
 * @brief Validates the fixed-size prefix of a `server.met` candidate before promotion.
 */
inline bool HasPlausibleServerMetShape(const uint64_t nFileBytes, const BYTE byHeader, const uint32_t nServerCount, const bool bRequireServers)
{
	if (!IsAcceptedServerMetHeader(byHeader))
		return false;
	if (bRequireServers && nServerCount == 0)
		return false;
	if (nFileBytes < kServerMetHeaderBytes)
		return false;

	const uint64_t nMinimumPayloadBytes = static_cast<uint64_t>(nServerCount) * kMinimumServerRecordBytes;
	return nFileBytes - kServerMetHeaderBytes >= nMinimumPayloadBytes;
}

/**
 * @brief Inspects a `server.met` file without mutating the live server list.
 */
inline bool InspectServerMetCandidate(LPCTSTR pszPath, ServerMetCandidateInfo &rInfo, const bool bRequireServers = true)
{
	rInfo = ServerMetCandidateInfo{};
	FILE *pFile = LongPathSeams::OpenFileStreamSharedReadLongPath(pszPath, false);
	if (pFile == NULL)
		return false;

	if (_fseeki64(pFile, 0, SEEK_END) != 0) {
		fclose(pFile);
		return false;
	}
	const __int64 nFileLength = _ftelli64(pFile);
	if (nFileLength < 0 || _fseeki64(pFile, 0, SEEK_SET) != 0) {
		fclose(pFile);
		return false;
	}

	BYTE byHeader = 0;
	uint32_t nServerCount = 0;
	const bool bReadHeader = fread(&byHeader, 1, sizeof byHeader, pFile) == sizeof byHeader;
	const bool bReadCount = fread(&nServerCount, 1, sizeof nServerCount, pFile) == sizeof nServerCount;
	fclose(pFile);

	if (!bReadHeader || !bReadCount)
		return false;
	if (!HasPlausibleServerMetShape(static_cast<uint64_t>(nFileLength), byHeader, nServerCount, bRequireServers))
		return false;

	rInfo.ServerCount = nServerCount;
	return true;
}

/**
 * @brief Promotes a prepared replacement file without moving the destination away first.
 */
inline bool PromotePreparedServerMetWithOps(LPCTSTR pszSourcePath, LPCTSTR pszTargetPath, DWORD *pdwLastError, const PartFilePersistenceSeams::FileSystemOps &rOps)
{
	return PartFilePersistenceSeams::TryReplaceFileAtomicallyWithOps(pszSourcePath, pszTargetPath, pdwLastError, rOps);
}

/**
 * @brief Promotes a prepared replacement file through the default filesystem operations.
 */
inline bool PromotePreparedServerMet(LPCTSTR pszSourcePath, LPCTSTR pszTargetPath, DWORD *pdwLastError = NULL)
{
	return PromotePreparedServerMetWithOps(pszSourcePath, pszTargetPath, pdwLastError, PartFilePersistenceSeams::GetDefaultFileSystemOps());
}

/**
 * @brief Copies the current live file into a backup through a temporary replace path.
 */
inline bool RefreshServerMetBackupWithOps(LPCTSTR pszLivePath, LPCTSTR pszBackupPath, LPCTSTR pszBackupTempPath, DWORD *pdwLastError, const PartFilePersistenceSeams::FileSystemOps &rOps)
{
	if (pdwLastError != NULL)
		*pdwLastError = ERROR_SUCCESS;
	if (!PartFilePersistenceSeams::PathExists(pszLivePath, rOps))
		return true;
	return PartFilePersistenceSeams::TryCopyFileToTempAndReplaceWithOps(pszLivePath, pszBackupPath, pszBackupTempPath, false, pdwLastError, rOps);
}

/**
 * @brief Copies the current live file into a backup through the default filesystem operations.
 */
inline bool RefreshServerMetBackup(LPCTSTR pszLivePath, LPCTSTR pszBackupPath, LPCTSTR pszBackupTempPath, DWORD *pdwLastError = NULL)
{
	return RefreshServerMetBackupWithOps(pszLivePath, pszBackupPath, pszBackupTempPath, pdwLastError, PartFilePersistenceSeams::GetDefaultFileSystemOps());
}

/**
 * @brief Validates and promotes a downloaded `server.met` candidate.
 */
inline bool InstallDownloadedServerMetCandidate(LPCTSTR pszCandidatePath, LPCTSTR pszTargetPath, DWORD *pdwLastError = NULL)
{
	if (pdwLastError != NULL)
		*pdwLastError = ERROR_SUCCESS;

	ServerMetCandidateInfo info;
	if (!InspectServerMetCandidate(pszCandidatePath, info, true)) {
		if (pdwLastError != NULL)
			*pdwLastError = ERROR_INVALID_DATA;
		return false;
	}
	return PromotePreparedServerMet(pszCandidatePath, pszTargetPath, pdwLastError);
}
}
