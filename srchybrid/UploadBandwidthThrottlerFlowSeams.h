//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct UploadBandwidthFlowState
{
	unsigned int nPriorityQueueCount;
	unsigned int nNormalQueueCount;
	unsigned int nPendingPriorityQueueCount;
	unsigned int nPendingNormalQueueCount;
	bool bSchedulerActive;
};

enum class UploadBandwidthFlowEvent
{
	EnqueuePriority,
	EnqueueNormal,
	MergePending,
	PopNext,
	RemoveAllQueuedCopies,
	Shutdown,
	Reset
};

struct UploadBandwidthFlowAction
{
	bool bShouldWakeScheduler;
	bool bShouldSendPrioritySocket;
	bool bShouldSendNormalSocket;
	bool bShouldClearQueues;
};

inline UploadBandwidthFlowState CreateUploadBandwidthFlowState()
{
	return UploadBandwidthFlowState{};
}

inline UploadBandwidthFlowAction AdvanceUploadBandwidthFlow(UploadBandwidthFlowState &state, UploadBandwidthFlowEvent event)
{
	UploadBandwidthFlowAction action = {};

	switch (event) {
	case UploadBandwidthFlowEvent::EnqueuePriority:
		++state.nPendingPriorityQueueCount;
		action.bShouldWakeScheduler = !state.bSchedulerActive;
		state.bSchedulerActive = true;
		break;

	case UploadBandwidthFlowEvent::EnqueueNormal:
		++state.nPendingNormalQueueCount;
		action.bShouldWakeScheduler = !state.bSchedulerActive;
		state.bSchedulerActive = true;
		break;

	case UploadBandwidthFlowEvent::MergePending:
		state.nPriorityQueueCount += state.nPendingPriorityQueueCount;
		state.nNormalQueueCount += state.nPendingNormalQueueCount;
		state.nPendingPriorityQueueCount = 0;
		state.nPendingNormalQueueCount = 0;
		break;

	case UploadBandwidthFlowEvent::PopNext:
		if (state.nPriorityQueueCount > 0) {
			--state.nPriorityQueueCount;
			action.bShouldSendPrioritySocket = true;
		}
		else if (state.nNormalQueueCount > 0) {
			--state.nNormalQueueCount;
			action.bShouldSendNormalSocket = true;
		}
		if (state.nPriorityQueueCount == 0 && state.nNormalQueueCount == 0 &&
			state.nPendingPriorityQueueCount == 0 && state.nPendingNormalQueueCount == 0)
			state.bSchedulerActive = false;
		break;

	case UploadBandwidthFlowEvent::RemoveAllQueuedCopies:
	case UploadBandwidthFlowEvent::Shutdown:
		state.nPriorityQueueCount = 0;
		state.nNormalQueueCount = 0;
		state.nPendingPriorityQueueCount = 0;
		state.nPendingNormalQueueCount = 0;
		state.bSchedulerActive = false;
		action.bShouldClearQueues = true;
		break;

	case UploadBandwidthFlowEvent::Reset:
		state = CreateUploadBandwidthFlowState();
		break;
	}

	return action;
}
