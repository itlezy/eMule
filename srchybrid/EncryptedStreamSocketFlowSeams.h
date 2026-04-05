//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct EncryptedStreamFlowState
{
	bool bDelayedServerSendState;
	bool bHasPendingNegotiationBuffer;
	bool bHandshakeFailed;
	bool bHandshakeFinished;
};

enum class EncryptedStreamFlowEvent
{
	FlushNegotiationBuffer,
	ReceiveWrongMagic,
	ReceiveUnsupportedMethod,
	ReceiveHandshakeSuccess,
	ResetDelayedSend
};

struct EncryptedStreamFlowAction
{
	bool bShouldCompleteDelayedSend;
	bool bShouldFailHandshake;
	bool bShouldMarkHandshakeFinished;
};

/**
 * @brief Advances the isolated delayed-send and negotiation outcome model used by obfuscation tests.
 */
inline EncryptedStreamFlowAction AdvanceEncryptedStreamFlow(EncryptedStreamFlowState &state, EncryptedStreamFlowEvent event)
{
	EncryptedStreamFlowAction action = {};

	switch (event) {
	case EncryptedStreamFlowEvent::FlushNegotiationBuffer:
		if (state.bDelayedServerSendState && !state.bHasPendingNegotiationBuffer)
			action.bShouldCompleteDelayedSend = true;
		break;

	case EncryptedStreamFlowEvent::ReceiveWrongMagic:
	case EncryptedStreamFlowEvent::ReceiveUnsupportedMethod:
		state.bHandshakeFailed = true;
		state.bDelayedServerSendState = false;
		action.bShouldFailHandshake = true;
		break;

	case EncryptedStreamFlowEvent::ReceiveHandshakeSuccess:
		state.bHandshakeFinished = true;
		state.bDelayedServerSendState = false;
		state.bHasPendingNegotiationBuffer = false;
		action.bShouldMarkHandshakeFinished = true;
		break;

	case EncryptedStreamFlowEvent::ResetDelayedSend:
		state.bDelayedServerSendState = false;
		state.bHasPendingNegotiationBuffer = false;
		break;
	}

	return action;
}
