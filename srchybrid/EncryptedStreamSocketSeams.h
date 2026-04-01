#pragma once

namespace EncryptedStreamSocketSeams
{
/**
 * Returns whether flushing a delayed server negotiation buffer should complete the
 * delayed-send state because no buffered negotiation data remains.
 */
inline bool ShouldCompleteDelayedServerSendAfterFlush(const bool bIsDelayedServerSendState, const bool bHasPendingNegotiationBuffer)
{
	return bIsDelayedServerSendState && !bHasPendingNegotiationBuffer;
}
}
