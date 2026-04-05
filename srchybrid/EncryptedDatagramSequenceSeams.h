//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

#include <cstddef>

#include "EncryptedDatagramFramingSeams.h"

struct EncryptedDatagramSequenceState
{
	size_t nBytesBuffered;
	size_t nHeaderBytesExpected;
	size_t nPayloadBytesExpected;
	size_t nPayloadBytesObserved;
	bool bHeaderDecoded;
	bool bPayloadReady;
	bool bPassedThrough;
};

struct EncryptedDatagramSequenceAction
{
	bool bShouldInspectHeader;
	bool bShouldExposePayload;
	bool bShouldPassThrough;
	size_t nBytesAccepted;
};

struct EncryptedDatagramSendSequenceState
{
	size_t nHeaderBytesTotal;
	size_t nHeaderBytesEmitted;
	size_t nPayloadBytesTotal;
	size_t nPayloadBytesEmitted;
	bool bReadyToSend;
	bool bPassedThrough;
	bool bTransmissionComplete;
};

struct EncryptedDatagramSendSequenceAction
{
	bool bShouldEmitHeaderSlice;
	bool bShouldEmitPayloadSlice;
	bool bShouldPassThrough;
	size_t nHeaderBytesEmitted;
	size_t nPayloadBytesEmitted;
	size_t nPayloadOffset;
};

inline EncryptedDatagramSequenceState CreateEncryptedDatagramSequenceState()
{
	return EncryptedDatagramSequenceState{};
}

inline EncryptedDatagramSendSequenceState CreateEncryptedDatagramSendSequenceState(
	const size_t nPayloadLength,
	const EncryptedDatagramFrameSnapshot &rSnapshot,
	const bool bPaddingFits)
{
	EncryptedDatagramSendSequenceState state = {};
	state.bReadyToSend = rSnapshot.bMarkerAllowed && rSnapshot.bHeaderLongEnough && bPaddingFits;
	state.bPassedThrough = !state.bReadyToSend;
	state.nHeaderBytesTotal = state.bReadyToSend ? rSnapshot.nExpectedOverhead : 0u;
	state.nPayloadBytesTotal = nPayloadLength;
	state.bTransmissionComplete = state.bPassedThrough;
	return state;
}

inline EncryptedDatagramSequenceAction AdvanceEncryptedDatagramSequence(
	EncryptedDatagramSequenceState &state,
	size_t nDatagramLength,
	size_t nIncomingSliceLength,
	const EncryptedDatagramFrameSnapshot &rSnapshot,
	bool bPaddingFits)
{
	EncryptedDatagramSequenceAction action = {};
	if (state.bPayloadReady || state.bPassedThrough || nIncomingSliceLength == 0)
		return action;

	const size_t nBytesRemaining = nDatagramLength > state.nBytesBuffered ? (nDatagramLength - state.nBytesBuffered) : 0u;
	const size_t nBytesAccepted = nIncomingSliceLength < nBytesRemaining ? nIncomingSliceLength : nBytesRemaining;
	if (nBytesAccepted == 0)
		return action;

	state.nBytesBuffered += nBytesAccepted;
	action.nBytesAccepted = nBytesAccepted;

	if (!state.bHeaderDecoded && (!rSnapshot.bMarkerAllowed || !rSnapshot.bHeaderLongEnough || !bPaddingFits)) {
		state.bHeaderDecoded = true;
		state.nHeaderBytesExpected = rSnapshot.nExpectedOverhead;
		state.bPassedThrough = true;
		action.bShouldInspectHeader = true;
		action.bShouldPassThrough = true;
		return action;
	}

	if (!state.bHeaderDecoded && state.nBytesBuffered >= rSnapshot.nExpectedOverhead) {
		state.bHeaderDecoded = true;
		state.nHeaderBytesExpected = rSnapshot.nExpectedOverhead;
		action.bShouldInspectHeader = true;
		if (!rSnapshot.bMarkerAllowed || !rSnapshot.bHeaderLongEnough || !bPaddingFits) {
			state.bPassedThrough = true;
			action.bShouldPassThrough = true;
			return action;
		}

		state.nPayloadBytesExpected = nDatagramLength >= state.nHeaderBytesExpected ? (nDatagramLength - state.nHeaderBytesExpected) : 0u;
		if (state.nPayloadBytesExpected == 0u) {
			state.bPayloadReady = true;
			action.bShouldExposePayload = true;
			return action;
		}
	}

	if (state.bHeaderDecoded && !state.bPassedThrough && state.nBytesBuffered >= (state.nHeaderBytesExpected + state.nPayloadBytesExpected)) {
		state.bPayloadReady = true;
		state.nPayloadBytesObserved = state.nPayloadBytesExpected;
		action.bShouldExposePayload = true;
	}

	return action;
}

inline EncryptedDatagramSendSequenceAction AdvanceEncryptedDatagramSendSequence(
	EncryptedDatagramSendSequenceState &state,
	size_t nOutgoingSliceBudget)
{
	EncryptedDatagramSendSequenceAction action = {};
	if (state.bTransmissionComplete || nOutgoingSliceBudget == 0u)
		return action;
	if (state.bPassedThrough) {
		state.bTransmissionComplete = true;
		action.bShouldPassThrough = true;
		return action;
	}

	size_t nRemainingBudget = nOutgoingSliceBudget;
	const size_t nHeaderBytesRemaining = state.nHeaderBytesTotal > state.nHeaderBytesEmitted ? (state.nHeaderBytesTotal - state.nHeaderBytesEmitted) : 0u;
	if (nHeaderBytesRemaining > 0u) {
		const size_t nHeaderSlice = nRemainingBudget < nHeaderBytesRemaining ? nRemainingBudget : nHeaderBytesRemaining;
		state.nHeaderBytesEmitted += nHeaderSlice;
		nRemainingBudget -= nHeaderSlice;
		action.bShouldEmitHeaderSlice = nHeaderSlice > 0u;
		action.nHeaderBytesEmitted = nHeaderSlice;
	}

	if (nRemainingBudget > 0u && state.nHeaderBytesEmitted == state.nHeaderBytesTotal) {
		const size_t nPayloadBytesRemaining = state.nPayloadBytesTotal > state.nPayloadBytesEmitted ? (state.nPayloadBytesTotal - state.nPayloadBytesEmitted) : 0u;
		const size_t nPayloadSlice = nRemainingBudget < nPayloadBytesRemaining ? nRemainingBudget : nPayloadBytesRemaining;
		action.bShouldEmitPayloadSlice = nPayloadSlice > 0u;
		action.nPayloadOffset = state.nPayloadBytesEmitted;
		action.nPayloadBytesEmitted = nPayloadSlice;
		state.nPayloadBytesEmitted += nPayloadSlice;
	}

	state.bTransmissionComplete = state.nHeaderBytesEmitted == state.nHeaderBytesTotal
		&& state.nPayloadBytesEmitted == state.nPayloadBytesTotal;
	return action;
}

inline void ResetEncryptedDatagramSequence(EncryptedDatagramSequenceState &state)
{
	state = CreateEncryptedDatagramSequenceState();
}
