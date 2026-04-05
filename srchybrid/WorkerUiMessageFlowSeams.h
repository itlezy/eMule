//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct WorkerUiFlowState
{
	bool bWindowAlive;
	bool bTargetClosing;
	bool bWakeupPosted;
	bool bPayloadQueued;
	bool bPayloadAvailableToUi;
	bool bPayloadDestroyed;
	unsigned int nQueuedPayloads;
};

enum class WorkerUiFlowEvent
{
	QueuePayload,
	PostPlainMessage,
	UiConsumesWakeup,
	UiTakesPayload,
	BeginClose,
	DiscardOwnerPayloads,
	DestroyWindow,
	Reset
};

struct WorkerUiFlowAction
{
	bool bShouldPostMessage;
	bool bShouldRejectPost;
	bool bShouldTransferPayloadToUi;
	bool bShouldDestroyPayload;
};

inline WorkerUiFlowState CreateWorkerUiFlowState()
{
	WorkerUiFlowState state = {};
	state.bWindowAlive = true;
	return state;
}

inline WorkerUiFlowAction AdvanceWorkerUiFlow(WorkerUiFlowState &state, WorkerUiFlowEvent event)
{
	WorkerUiFlowAction action = {};

	switch (event) {
	case WorkerUiFlowEvent::QueuePayload:
		if (!state.bWindowAlive || state.bTargetClosing) {
			action.bShouldRejectPost = true;
			break;
		}
		++state.nQueuedPayloads;
		state.bPayloadQueued = true;
		if (!state.bWakeupPosted) {
			state.bWakeupPosted = true;
			action.bShouldPostMessage = true;
		}
		break;

	case WorkerUiFlowEvent::PostPlainMessage:
		if (!state.bWindowAlive || state.bTargetClosing) {
			action.bShouldRejectPost = true;
			break;
		}
		action.bShouldPostMessage = true;
		break;

	case WorkerUiFlowEvent::UiConsumesWakeup:
		if (!state.bWakeupPosted)
			break;
		state.bWakeupPosted = false;
		state.bPayloadAvailableToUi = state.bPayloadQueued;
		action.bShouldTransferPayloadToUi = state.bPayloadAvailableToUi;
		break;

	case WorkerUiFlowEvent::UiTakesPayload:
		if (!state.bPayloadAvailableToUi || state.nQueuedPayloads == 0)
			break;
		--state.nQueuedPayloads;
		state.bPayloadQueued = state.nQueuedPayloads != 0;
		state.bPayloadAvailableToUi = false;
		break;

	case WorkerUiFlowEvent::BeginClose:
		state.bTargetClosing = true;
		break;

	case WorkerUiFlowEvent::DiscardOwnerPayloads:
		if (state.nQueuedPayloads > 0) {
			state.nQueuedPayloads = 0;
			state.bPayloadQueued = false;
			state.bPayloadAvailableToUi = false;
			state.bPayloadDestroyed = true;
			action.bShouldDestroyPayload = true;
		}
		break;

	case WorkerUiFlowEvent::DestroyWindow:
		state.bWindowAlive = false;
		state.bWakeupPosted = false;
		break;

	case WorkerUiFlowEvent::Reset:
		state = CreateWorkerUiFlowState();
		break;
	}

	return action;
}
