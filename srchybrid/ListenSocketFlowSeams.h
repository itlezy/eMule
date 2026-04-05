//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

struct ListenSocketFlowState
{
	bool bSocketOpen;
	bool bAcceptPending;
	bool bParseErrorSeen;
	bool bUnknownExceptionSeen;
	bool bConnectionClosed;
};

enum class ListenSocketFlowEvent
{
	AcceptReady,
	PacketParsed,
	InvalidPacket,
	UnknownException,
	CloseConnection,
	Reset
};

struct ListenSocketFlowAction
{
	bool bShouldAcceptConnection;
	bool bShouldKeepSocketOpen;
	bool bShouldReportParseError;
	bool bShouldFallbackToUnknownExceptionMessage;
};

inline ListenSocketFlowState CreateListenSocketFlowState()
{
	ListenSocketFlowState state = {};
	state.bSocketOpen = true;
	return state;
}

inline ListenSocketFlowAction AdvanceListenSocketFlow(ListenSocketFlowState &state, ListenSocketFlowEvent event)
{
	ListenSocketFlowAction action = {};

	switch (event) {
	case ListenSocketFlowEvent::AcceptReady:
		state.bAcceptPending = true;
		action.bShouldAcceptConnection = state.bSocketOpen;
		break;

	case ListenSocketFlowEvent::PacketParsed:
		state.bAcceptPending = false;
		action.bShouldKeepSocketOpen = state.bSocketOpen;
		break;

	case ListenSocketFlowEvent::InvalidPacket:
		state.bAcceptPending = false;
		state.bParseErrorSeen = true;
		action.bShouldReportParseError = true;
		action.bShouldKeepSocketOpen = state.bSocketOpen;
		break;

	case ListenSocketFlowEvent::UnknownException:
		state.bAcceptPending = false;
		state.bUnknownExceptionSeen = true;
		action.bShouldFallbackToUnknownExceptionMessage = true;
		action.bShouldKeepSocketOpen = state.bSocketOpen;
		break;

	case ListenSocketFlowEvent::CloseConnection:
		state.bAcceptPending = false;
		state.bConnectionClosed = true;
		break;

	case ListenSocketFlowEvent::Reset:
		state = CreateListenSocketFlowState();
		break;
	}

	return action;
}
