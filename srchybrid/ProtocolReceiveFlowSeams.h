//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

#include <cstddef>

struct ProtocolReceiveFlowState
{
	size_t nHeaderBytesBuffered;
	size_t nPayloadBytesExpected;
	size_t nPayloadBytesBuffered;
	bool bHeaderDecoded;
	bool bPacketReady;
	bool bRejected;
};

struct ProtocolReceiveFlowAction
{
	bool bShouldAttemptHeaderParse;
	bool bShouldEmitPacket;
	bool bShouldRejectPacket;
	size_t nBytesConsumed;
};

inline ProtocolReceiveFlowState CreateProtocolReceiveFlowState()
{
	return ProtocolReceiveFlowState{};
}

inline ProtocolReceiveFlowAction AdvanceProtocolReceiveFlow(ProtocolReceiveFlowState &state, size_t nIncomingBytes, bool bHeaderValid, size_t nPayloadBytesExpected)
{
	static const size_t PROTOCOL_HEADER_SIZE = 6u;

	ProtocolReceiveFlowAction action = {};
	if (state.bRejected || state.bPacketReady || nIncomingBytes == 0)
		return action;

	size_t nRemainingIncomingBytes = nIncomingBytes;
	if (!state.bHeaderDecoded) {
		const size_t nHeaderBytesNeeded = PROTOCOL_HEADER_SIZE - state.nHeaderBytesBuffered;
		const size_t nHeaderBytesAccepted = nRemainingIncomingBytes < nHeaderBytesNeeded ? nRemainingIncomingBytes : nHeaderBytesNeeded;
		state.nHeaderBytesBuffered += nHeaderBytesAccepted;
		action.nBytesConsumed += nHeaderBytesAccepted;
		nRemainingIncomingBytes -= nHeaderBytesAccepted;

		if (state.nHeaderBytesBuffered == PROTOCOL_HEADER_SIZE) {
			action.bShouldAttemptHeaderParse = true;
			if (!bHeaderValid) {
				state.bRejected = true;
				action.bShouldRejectPacket = true;
				return action;
			}

			state.bHeaderDecoded = true;
			state.nPayloadBytesExpected = nPayloadBytesExpected;
			if (state.nPayloadBytesExpected == 0) {
				state.bPacketReady = true;
				action.bShouldEmitPacket = true;
				return action;
			}
		}
	}

	if (state.bHeaderDecoded && nRemainingIncomingBytes > 0 && !state.bPacketReady) {
		const size_t nPayloadBytesRemaining = state.nPayloadBytesExpected - state.nPayloadBytesBuffered;
		const size_t nPayloadBytesAccepted = nRemainingIncomingBytes < nPayloadBytesRemaining ? nRemainingIncomingBytes : nPayloadBytesRemaining;
		state.nPayloadBytesBuffered += nPayloadBytesAccepted;
		action.nBytesConsumed += nPayloadBytesAccepted;
		if (state.nPayloadBytesBuffered == state.nPayloadBytesExpected) {
			state.bPacketReady = true;
			action.bShouldEmitPacket = true;
		}
	}

	return action;
}

inline void ResetProtocolReceiveFlow(ProtocolReceiveFlowState &state)
{
	state = CreateProtocolReceiveFlowState();
}
