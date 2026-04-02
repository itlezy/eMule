//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//GNU General Public License for more details.

#pragma once

#include <cstdint>
#include <winsock2.h>

#define FD_FORCEREAD (1<<15)
#define FD_DEFAULT (FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)

enum AsyncSocketExState : uint8_t
{
	notsock,
	unconnected,
	connecting,
	listening,
	connected,
	closed,
	aborted,
	attached
};

struct AsyncSocketExCloseAction
{
	bool bShouldReadDrain;
	bool bShouldClose;
};

/**
 * Maps the requested async event interest and socket state onto the `WSAPoll` interest mask.
 */
inline short GetAsyncSocketPollEvents(AsyncSocketExState nState, long lEventMask)
{
	short nPollEvents = 0;
	if (nState == listening) {
		if (lEventMask & FD_ACCEPT)
			nPollEvents |= POLLIN;
		if (lEventMask & FD_CLOSE)
			nPollEvents |= POLLIN;
	} else if (nState == connecting) {
		nPollEvents |= POLLOUT;
		if (lEventMask & FD_READ)
			nPollEvents |= POLLIN;
	} else if (nState == connected || nState == attached) {
		if (lEventMask & FD_READ)
			nPollEvents |= POLLIN;
		if (lEventMask & FD_WRITE)
			nPollEvents |= POLLOUT;
		if (lEventMask & FD_CLOSE)
			nPollEvents |= POLLIN;
	}
	return nPollEvents;
}

/**
 * Reports whether the current poll result should complete a nonblocking connect attempt.
 */
inline bool ShouldCompleteAsyncSocketConnect(AsyncSocketExState nState, short nPollRevents)
{
	return nState == connecting && (nPollRevents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) != 0;
}

/**
 * Reports whether the current poll result should dispatch an accept callback.
 */
inline bool ShouldDispatchAsyncSocketAccept(AsyncSocketExState nState, long lEventMask, short nPollRevents)
{
	return nState == listening && (lEventMask & FD_ACCEPT) != 0 && (nPollRevents & POLLIN) != 0;
}

/**
 * Reports whether the current poll result should dispatch a readable callback.
 */
inline bool ShouldDispatchAsyncSocketRead(AsyncSocketExState nState, long lEventMask, short nPollRevents)
{
	return (nState == connected || nState == attached) && (lEventMask & FD_READ) != 0 && (nPollRevents & POLLIN) != 0;
}

/**
 * Reports whether the current poll result should dispatch a writable callback.
 */
inline bool ShouldDispatchAsyncSocketWrite(AsyncSocketExState nState, long lEventMask, short nPollRevents)
{
	return (nState == connected || nState == attached) && (lEventMask & FD_WRITE) != 0 && (nPollRevents & POLLOUT) != 0;
}

/**
 * Reports whether the current poll result contains a terminal close or error signal.
 */
inline bool HasAsyncSocketCloseSignal(short nPollRevents)
{
	return (nPollRevents & static_cast<short>(POLLERR | POLLHUP | POLLNVAL)) != 0;
}

/**
 * Decides whether a close signal should first drain pending readable bytes before `OnClose`.
 */
inline AsyncSocketExCloseAction ClassifyAsyncSocketClose(AsyncSocketExState nState, long lEventMask, short nPollRevents, bool bHasPendingData)
{
	if ((nState != connected && nState != attached) || !HasAsyncSocketCloseSignal(nPollRevents))
		return {false, false};

	if ((lEventMask & FD_READ) != 0 && bHasPendingData)
		return {true, false};

	return {false, true};
}

/**
 * Reports whether a `WSAPoll()` result represents a timeout, a dispatchable event batch, or a hard failure.
 */
inline bool HasAsyncSocketPollFailure(int nPollResult)
{
	return nPollResult == SOCKET_ERROR;
}

/**
 * @brief Reports whether callback-drain polling should keep yielding while callbacks remain in flight.
 */
inline bool ShouldYieldForAsyncSocketCallbackDrain(long nCallbacksInFlight)
{
	return nCallbacksInFlight > 0;
}
