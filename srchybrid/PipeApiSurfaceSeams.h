#pragma once

#include <cstring>
#include <cstdint>

namespace PipeApiSurfaceSeams
{
enum class ETransferPriority : uint8_t
{
	Invalid,
	Auto,
	VeryLow,
	Low,
	Normal,
	High,
	VeryHigh
};

enum class EMutablePreference : uint8_t
{
	Invalid,
	MaxUploadKiB,
	MaxDownloadKiB,
	MaxConnections,
	MaxConPerFive,
	MaxSourcesPerFile,
	UploadClientDataRate,
	MaxUploadSlots,
	QueueSize,
	AutoConnect,
	NewAutoUp,
	NewAutoDown,
	CreditSystem,
	SafeServerConnect,
	NetworkKademlia,
	NetworkEd2K
};

/**
 * Maps the persisted eD2K server priority to the public API string.
 */
inline const char* GetServerPriorityName(const unsigned uPreference)
{
	switch (uPreference) {
	case 2:
		return "low";
	case 1:
		return "high";
	case 0:
	default:
		return "normal";
	}
}

/**
 * Maps the internal upload state enum to the stable API state string.
 */
inline const char* GetUploadStateName(const uint8_t uUploadState)
{
	switch (uUploadState) {
	case 0:
		return "uploading";
	case 1:
		return "queued";
	case 2:
		return "connecting";
	case 3:
		return "banned";
	case 4:
	default:
		return "idle";
	}
}

/**
 * Parses the stable transfer-priority vocabulary used by the pipe API.
 */
inline ETransferPriority ParseTransferPriorityName(const char *pszPriority)
{
	if (pszPriority == nullptr || pszPriority[0] == '\0')
		return ETransferPriority::Invalid;
	if (strcmp(pszPriority, "auto") == 0)
		return ETransferPriority::Auto;
	if (strcmp(pszPriority, "very_low") == 0)
		return ETransferPriority::VeryLow;
	if (strcmp(pszPriority, "low") == 0)
		return ETransferPriority::Low;
	if (strcmp(pszPriority, "normal") == 0)
		return ETransferPriority::Normal;
	if (strcmp(pszPriority, "high") == 0)
		return ETransferPriority::High;
	if (strcmp(pszPriority, "very_high") == 0)
		return ETransferPriority::VeryHigh;
	return ETransferPriority::Invalid;
}

/**
 * Parses the stable mutable-preference vocabulary exposed over the pipe API.
 */
inline EMutablePreference ParseMutablePreferenceName(const char *pszPreferenceName)
{
	if (pszPreferenceName == nullptr || pszPreferenceName[0] == '\0')
		return EMutablePreference::Invalid;
	if (strcmp(pszPreferenceName, "maxUploadKiB") == 0)
		return EMutablePreference::MaxUploadKiB;
	if (strcmp(pszPreferenceName, "maxDownloadKiB") == 0)
		return EMutablePreference::MaxDownloadKiB;
	if (strcmp(pszPreferenceName, "maxConnections") == 0)
		return EMutablePreference::MaxConnections;
	if (strcmp(pszPreferenceName, "maxConPerFive") == 0)
		return EMutablePreference::MaxConPerFive;
	if (strcmp(pszPreferenceName, "maxSourcesPerFile") == 0)
		return EMutablePreference::MaxSourcesPerFile;
	if (strcmp(pszPreferenceName, "queueSize") == 0)
		return EMutablePreference::QueueSize;
	if (strcmp(pszPreferenceName, "autoConnect") == 0)
		return EMutablePreference::AutoConnect;
	if (strcmp(pszPreferenceName, "newAutoUp") == 0)
		return EMutablePreference::NewAutoUp;
	if (strcmp(pszPreferenceName, "newAutoDown") == 0)
		return EMutablePreference::NewAutoDown;
	if (strcmp(pszPreferenceName, "creditSystem") == 0)
		return EMutablePreference::CreditSystem;
	if (strcmp(pszPreferenceName, "safeServerConnect") == 0)
		return EMutablePreference::SafeServerConnect;
	if (strcmp(pszPreferenceName, "networkKademlia") == 0)
		return EMutablePreference::NetworkKademlia;
	if (strcmp(pszPreferenceName, "networkEd2k") == 0)
		return EMutablePreference::NetworkEd2K;
	return EMutablePreference::Invalid;
}

/**
 * Reports whether a shared-file removal request is legal for the selected file.
 */
inline bool CanRemoveSharedFile(const bool bIsShared, const bool bMustRemainShared)
{
	return bIsShared && !bMustRemainShared;
}
}
