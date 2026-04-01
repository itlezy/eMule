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
}
