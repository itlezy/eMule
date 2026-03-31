#pragma once

#include "ModernLimits.h"

/**
 * \file FileBufferSlider.h
 * \brief Maps the Tweaks file-buffer trackbar to bounded byte sizes without making the large range unusable.
 */

namespace FileBufferSlider
{
	/** Lowest file-buffer size exposed by the slider in KiB. */
	inline constexpr int kMinPosition = 16;
	/** Last position in the fine-grained KiB range. */
	inline constexpr int kFineRangeMaxPosition = 1024;
	/** Highest file-buffer size exposed by the slider in MiB. */
	inline constexpr int kMaxPosition = kFineRangeMaxPosition + static_cast<int>(ModernLimits::kMaxFileBufferSize / (1024u * 1024u)) - 1;

	/**
	 * \brief Converts a slider position to a file-buffer size in bytes.
	 *
	 * Positions up to 1024 keep the legacy KiB-level control for small values.
	 * Higher positions switch to 1 MiB steps so the trackbar can still reach 512 MiB comfortably.
	 */
	inline constexpr unsigned int PositionToBytes(int iPosition) noexcept
	{
		if (iPosition < kMinPosition)
			iPosition = kMinPosition;
		if (iPosition > kMaxPosition)
			iPosition = kMaxPosition;
		if (iPosition <= kFineRangeMaxPosition)
			return static_cast<unsigned int>(iPosition) * 1024u;
		return static_cast<unsigned int>(iPosition - kFineRangeMaxPosition + 1) * 1024u * 1024u;
	}

	/**
	 * \brief Converts a persisted file-buffer size in bytes to the nearest slider position.
	 *
	 * Small values keep their KiB precision. Larger values round to the nearest MiB slider step.
	 */
	inline constexpr int BytesToPosition(const unsigned int uBytes) noexcept
	{
		if (uBytes <= static_cast<unsigned int>(kFineRangeMaxPosition) * 1024u) {
			const int iKiB = static_cast<int>(uBytes / 1024u);
			return (iKiB < kMinPosition) ? kMinPosition : iKiB;
		}

		unsigned int uMiB = (uBytes + (512u * 1024u)) / (1024u * 1024u);
		if (uMiB < 2u)
			uMiB = 2u;
		const unsigned int uMaxMiB = ModernLimits::kMaxFileBufferSize / (1024u * 1024u);
		if (uMiB > uMaxMiB)
			uMiB = uMaxMiB;
		return kFineRangeMaxPosition + static_cast<int>(uMiB) - 1;
	}
}
