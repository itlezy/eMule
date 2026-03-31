#pragma once

/**
 * \file ModernLimits.h
 * \brief Shared defaults and normalization helpers for modernized local runtime limits.
 */

namespace ModernLimits
{
	/** Conservative branch default for the total connection budget. */
	inline constexpr unsigned int kDefaultMaxConnections = 500;
	/** Modern default for the half-open connection budget. */
	inline constexpr unsigned int kDefaultMaxHalfOpenConnections = 50;
	/** Modern default for the short burst connection budget. */
	inline constexpr unsigned int kDefaultMaxConnectionsPerFiveSeconds = 50;

	/** Minimum configurable timeout in seconds to avoid immediate disconnect loops. */
	inline constexpr unsigned int kMinTimeoutSeconds = 5;

	/** Modern default TCP socket timeout for peer connections. */
	inline constexpr unsigned int kDefaultConnectionTimeoutSeconds = 30;
	/** Modern default inactivity timeout while downloading payload blocks. */
	inline constexpr unsigned int kDefaultDownloadTimeoutSeconds = 75;
	/** Fixed eviction age for stale queued UDP packets. */
	inline constexpr unsigned int kDefaultUdpMaxQueueTimeSeconds = 20;
	/** Fixed latency allowance used by source reask pacing. */
	inline constexpr unsigned int kDefaultConnectionLatencyMs = 15000;

	/** Modern default UDP receive buffer size in bytes. */
	inline constexpr unsigned int kDefaultUdpReceiveBufferSize = 512u * 1024u;
	/** Modern default TCP send buffer size in bytes. */
	inline constexpr unsigned int kDefaultTcpSendBufferSize = 512u * 1024u;
	/** Modern default file buffer size in bytes. */
	inline constexpr unsigned int kDefaultFileBufferSize = 64u * 1024u * 1024u;
	/** Maximum file buffer size exposed through the Tweaks slider. */
	inline constexpr unsigned int kMaxFileBufferSize = 512u * 1024u * 1024u;
	/** Modern default file buffer time limit in seconds. */
	inline constexpr unsigned int kDefaultFileBufferTimeLimitSeconds = 120;
	/** Modern default queue size. */
	inline constexpr int kDefaultQueueSize = 100 * 100;
	/** Modern default source ceiling per file. */
	inline constexpr unsigned int kDefaultMaxSourcesPerFile = 600;
	/** Modern default soft source ceiling per file. */
	inline constexpr unsigned int kDefaultMaxSourcesPerFileSoft = 1000;
	/** Modern default UDP source query ceiling per file. */
	inline constexpr unsigned int kDefaultMaxSourcesPerFileUdp = 100u;
	/** Modern default per-client upload target in bytes per second. */
	inline constexpr unsigned int kDefaultUploadClientDataRate = 8u * 1024u * 1024u;

	/** Converts whole seconds to milliseconds for persisted timeout values. */
	inline constexpr unsigned long SecondsToMs(const unsigned int seconds) noexcept
	{
		return static_cast<unsigned long>(seconds) * 1000ul;
	}

	/** Normalizes configurable timeout values loaded from preferences. */
	inline constexpr unsigned long NormalizeTimeoutSeconds(unsigned int seconds, const unsigned int defaultSeconds) noexcept
	{
		if (seconds == 0)
			seconds = defaultSeconds;
		if (seconds < kMinTimeoutSeconds)
			seconds = kMinTimeoutSeconds;
		return SecondsToMs(seconds);
	}

	/** Serializes a persisted timeout back to whole seconds. */
	inline constexpr unsigned int TimeoutMsToSeconds(const unsigned long milliseconds) noexcept
	{
		return static_cast<unsigned int>(milliseconds / 1000ul);
	}

	/** Applies the configured per-client upload ceiling to a computed slot target. */
	inline constexpr unsigned int ApplyUploadClientDataRateCap(const unsigned int targetBytesPerSec, const unsigned int capBytesPerSec) noexcept
	{
		return (targetBytesPerSec < capBytesPerSec) ? targetBytesPerSec : capBytesPerSec;
	}
}
