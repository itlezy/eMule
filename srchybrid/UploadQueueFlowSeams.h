//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

#include "UploadQueueSeams.h"

struct UploadQueueFlowState
{
	bool bFoundInActiveList;
	bool bRetired;
	bool bHasClient;
	int nPendingIOBlocks;
	bool bReclaimed;
};

enum class UploadQueueFlowEvent
{
	AddLiveEntry,
	MarkRetired,
	DropClient,
	QueuePendingIO,
	CompletePendingIO,
	ReclaimIfSafe,
	RemoveEntry,
	Reset
};

struct UploadQueueFlowAction
{
	UploadQueueEntryAccessState eAccessState;
	bool bShouldReclaim;
};

inline UploadQueueFlowState CreateUploadQueueFlowState()
{
	return UploadQueueFlowState{};
}

inline UploadQueueFlowAction AdvanceUploadQueueFlow(UploadQueueFlowState &state, UploadQueueFlowEvent event)
{
	UploadQueueFlowAction action = {};

	switch (event) {
	case UploadQueueFlowEvent::AddLiveEntry:
		state.bFoundInActiveList = true;
		state.bRetired = false;
		state.bHasClient = true;
		state.bReclaimed = false;
		break;

	case UploadQueueFlowEvent::MarkRetired:
		state.bRetired = true;
		break;

	case UploadQueueFlowEvent::DropClient:
		state.bHasClient = false;
		break;

	case UploadQueueFlowEvent::QueuePendingIO:
		++state.nPendingIOBlocks;
		break;

	case UploadQueueFlowEvent::CompletePendingIO:
		if (state.nPendingIOBlocks > 0)
			--state.nPendingIOBlocks;
		break;

	case UploadQueueFlowEvent::ReclaimIfSafe:
		action.bShouldReclaim = CanReclaimUploadQueueEntry(state.bRetired, state.nPendingIOBlocks);
		if (action.bShouldReclaim)
			state.bReclaimed = true;
		break;

	case UploadQueueFlowEvent::RemoveEntry:
		state.bFoundInActiveList = false;
		state.bHasClient = false;
		state.bRetired = false;
		state.nPendingIOBlocks = 0;
		break;

	case UploadQueueFlowEvent::Reset:
		state = CreateUploadQueueFlowState();
		break;
	}

	action.eAccessState = ClassifyUploadQueueEntryAccess(state.bFoundInActiveList, state.bRetired, state.bHasClient);
	return action;
}
