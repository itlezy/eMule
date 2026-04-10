#pragma once

namespace FileBufferSlider
{
	inline constexpr int kMinPosition = 16;
	inline constexpr int kFineRangeMaxPosition = 1024;
	inline constexpr unsigned int kMaxFileBufferSizeBytes = 512u * 1024u * 1024u;
	inline constexpr int kMaxPosition = kFineRangeMaxPosition + static_cast<int>(kMaxFileBufferSizeBytes / (1024u * 1024u)) - 1;

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

	inline constexpr int BytesToPosition(const unsigned int uBytes) noexcept
	{
		if (uBytes <= static_cast<unsigned int>(kFineRangeMaxPosition) * 1024u) {
			const int iKiB = static_cast<int>(uBytes / 1024u);
			return (iKiB < kMinPosition) ? kMinPosition : iKiB;
		}

		unsigned int uMiB = (uBytes + (512u * 1024u)) / (1024u * 1024u);
		if (uMiB < 2u)
			uMiB = 2u;
		const unsigned int uMaxMiB = kMaxFileBufferSizeBytes / (1024u * 1024u);
		if (uMiB > uMaxMiB)
			uMiB = uMaxMiB;
		return kFineRangeMaxPosition + static_cast<int>(uMiB) - 1;
	}
}
