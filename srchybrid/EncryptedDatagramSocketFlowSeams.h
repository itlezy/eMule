//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct EncryptedDatagramFlowState
{
	bool bPlainProtocolDetected;
	bool bDecryptAttempted;
	bool bKadPacket;
	bool bKadReceiverKeyMode;
	bool bVerifyKeysExpected;
	bool bVerifyKeysConsumed;
	bool bPayloadReady;
	bool bPassedThrough;
	bool bCryptOverheadRecorded;
};

enum class EncryptedDatagramFlowEvent
{
	ObservePlainProtocolMarker,
	ObserveEncryptedCandidate,
	MagicMatchedEd2k,
	MagicMatchedKadNodeId,
	MagicMatchedKadReceiverKey,
	MagicMismatch,
	PaddingTooLarge,
	VerifyKeysPresent,
	VerifyKeysMissing,
	PayloadDecrypted,
	Reset
};

struct EncryptedDatagramFlowAction
{
	bool bShouldAttemptDecrypt;
	bool bShouldPassThrough;
	bool bShouldConsumeVerifyKeys;
	bool bShouldExposePayload;
	bool bShouldRecordCryptOverhead;
};

inline EncryptedDatagramFlowState CreateEncryptedDatagramFlowState()
{
	return EncryptedDatagramFlowState{};
}

inline EncryptedDatagramFlowAction AdvanceEncryptedDatagramFlow(EncryptedDatagramFlowState &state, EncryptedDatagramFlowEvent event)
{
	EncryptedDatagramFlowAction action = {};

	switch (event) {
	case EncryptedDatagramFlowEvent::ObservePlainProtocolMarker:
		state.bPlainProtocolDetected = true;
		state.bPassedThrough = true;
		action.bShouldPassThrough = true;
		break;

	case EncryptedDatagramFlowEvent::ObserveEncryptedCandidate:
		state.bDecryptAttempted = true;
		action.bShouldAttemptDecrypt = true;
		break;

	case EncryptedDatagramFlowEvent::MagicMatchedEd2k:
		state.bKadPacket = false;
		state.bKadReceiverKeyMode = false;
		state.bVerifyKeysExpected = false;
		break;

	case EncryptedDatagramFlowEvent::MagicMatchedKadNodeId:
		state.bKadPacket = true;
		state.bKadReceiverKeyMode = false;
		state.bVerifyKeysExpected = true;
		break;

	case EncryptedDatagramFlowEvent::MagicMatchedKadReceiverKey:
		state.bKadPacket = true;
		state.bKadReceiverKeyMode = true;
		state.bVerifyKeysExpected = true;
		break;

	case EncryptedDatagramFlowEvent::MagicMismatch:
	case EncryptedDatagramFlowEvent::PaddingTooLarge:
		state.bPassedThrough = true;
		action.bShouldPassThrough = true;
		break;

	case EncryptedDatagramFlowEvent::VerifyKeysPresent:
		if (state.bVerifyKeysExpected) {
			state.bVerifyKeysConsumed = true;
			action.bShouldConsumeVerifyKeys = true;
		}
		break;

	case EncryptedDatagramFlowEvent::VerifyKeysMissing:
		if (state.bVerifyKeysExpected) {
			state.bPassedThrough = true;
			action.bShouldPassThrough = true;
		}
		break;

	case EncryptedDatagramFlowEvent::PayloadDecrypted:
		state.bPayloadReady = !state.bPassedThrough && (!state.bVerifyKeysExpected || state.bVerifyKeysConsumed);
		if (state.bPayloadReady) {
			state.bCryptOverheadRecorded = true;
			action.bShouldExposePayload = true;
			action.bShouldRecordCryptOverhead = true;
		}
		break;

	case EncryptedDatagramFlowEvent::Reset:
		state = CreateEncryptedDatagramFlowState();
		break;
	}

	return action;
}
