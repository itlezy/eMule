//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct AsyncDatagramFlowState
{
	bool bWriteInterestEnabled;
	bool bReceivePending;
	bool bSendPending;
	bool bDispatchPosted;
	bool bSocketOpen;
};

enum class AsyncDatagramFlowEvent
{
	ReceiveReady,
	SendReady,
	Dispatch,
	CloseSocket,
	EnableWriteInterest,
	DisableWriteInterest
};

struct AsyncDatagramFlowAction
{
	bool bShouldPostDispatch;
	bool bShouldRefreshAsyncSelect;
	bool bShouldDispatchReceive;
	bool bShouldDispatchSend;
};

/**
 * @brief Returns the stable zeroed datagram dispatcher state used right after construction.
 */
inline AsyncDatagramFlowState CreateAsyncDatagramFlowState()
{
	AsyncDatagramFlowState state = {};
	state.bSocketOpen = true;
	return state;
}

/**
 * @brief Advances the datagram dispatch state machine by one externally visible event.
 */
inline AsyncDatagramFlowAction AdvanceAsyncDatagramFlow(AsyncDatagramFlowState &state, AsyncDatagramFlowEvent event)
{
	AsyncDatagramFlowAction action = {};

	switch (event) {
	case AsyncDatagramFlowEvent::ReceiveReady:
		if (!state.bSocketOpen)
			break;
		state.bReceivePending = true;
		if (!state.bDispatchPosted) {
			state.bDispatchPosted = true;
			action.bShouldPostDispatch = true;
		}
		break;

	case AsyncDatagramFlowEvent::SendReady:
		if (!state.bSocketOpen)
			break;
		state.bSendPending = true;
		if (!state.bDispatchPosted) {
			state.bDispatchPosted = true;
			action.bShouldPostDispatch = true;
		}
		break;

	case AsyncDatagramFlowEvent::Dispatch:
		if (!state.bDispatchPosted && !state.bReceivePending && !state.bSendPending)
			break;

		state.bDispatchPosted = false;
		action.bShouldDispatchReceive = state.bReceivePending;
		action.bShouldDispatchSend = state.bSendPending;
		state.bReceivePending = false;
		state.bSendPending = false;
		break;

	case AsyncDatagramFlowEvent::CloseSocket:
		state.bWriteInterestEnabled = false;
		state.bReceivePending = false;
		state.bSendPending = false;
		state.bDispatchPosted = false;
		state.bSocketOpen = false;
		break;

	case AsyncDatagramFlowEvent::EnableWriteInterest:
		if (!state.bWriteInterestEnabled) {
			state.bWriteInterestEnabled = true;
			action.bShouldRefreshAsyncSelect = state.bSocketOpen;
		}
		break;

	case AsyncDatagramFlowEvent::DisableWriteInterest:
		if (state.bWriteInterestEnabled) {
			state.bWriteInterestEnabled = false;
			action.bShouldRefreshAsyncSelect = state.bSocketOpen;
		}
		break;
	}

	return action;
}
