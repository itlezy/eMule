#pragma once

#include <cstddef>
#include <cstdint>

namespace PipeApiPolicy
{
/**
 * Categorizes queued outbound pipe traffic by how lossy it may be under load.
 */
enum class EWriteKind : uint8_t
{
	Stats,
	Structural
};

/**
 * Describes how the pipe server should react when a new outbound line arrives
 * under current queue pressure.
 */
enum class EWriteAction : uint8_t
{
	Queue,
	Drop,
	Disconnect
};

/**
 * Caps the number of queued UI-bound commands for the current client.
 */
static constexpr size_t kMaxPendingCommands = 64;

/**
 * Caps the number of queued outbound event lines for the current client.
 */
static constexpr size_t kMaxPendingWrites = 256;

/**
 * Caps the total queued outbound payload bytes for the current client.
 */
static constexpr size_t kMaxPendingWriteBytes = 512u * 1024u;

/**
 * Drops an unhealthy client after this many consecutive command timeouts.
 */
static constexpr unsigned kMaxConsecutiveCommandTimeouts = 2;

/**
 * Returns whether another UI-bound pipe command may be queued safely.
 */
inline bool CanQueueCommand(const size_t uPendingCommands)
{
	return uPendingCommands < kMaxPendingCommands;
}

/**
 * Decides whether a new outbound line should be queued, dropped, or should
 * force a reconnect because the current client stopped draining data.
 */
inline EWriteAction GetWriteAction(
	const EWriteKind eKind,
	const size_t uPendingWrites,
	const size_t uPendingWriteBytes,
	const size_t uLineBytes,
	const bool bStatsAlreadyQueued)
{
	if (eKind == EWriteKind::Stats && bStatsAlreadyQueued)
		return EWriteAction::Drop;

	if (uPendingWrites >= kMaxPendingWrites || uPendingWriteBytes + uLineBytes > kMaxPendingWriteBytes)
		return eKind == EWriteKind::Stats ? EWriteAction::Drop : EWriteAction::Disconnect;

	return EWriteAction::Queue;
}
}
