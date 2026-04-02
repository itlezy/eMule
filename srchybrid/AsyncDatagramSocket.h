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

#include <atomic>
#include "AsyncSocketEx.h"

/**
 * Owns a nonblocking UDP socket on the shared `WSAPoll` backend while marshalling readiness back
 * to the main thread for legacy protocol handlers.
 */
class CAsyncDatagramSocket : public CAsyncSocketEx
{
public:
	CAsyncDatagramSocket();
	virtual ~CAsyncDatagramSocket() = default;

	bool CreateDatagram(UINT nSocketPort, const CString &sSocketAddress = CString(), ADDRESS_FAMILY nFamily = AF_INET);
	void Close() override;
	void DispatchQueuedSocketEvents();

protected:
	void SetWriteInterestEnabled(bool bEnabled);
	static bool PostDatagramDispatchMessage();

	virtual void OnDatagramReceive(int nErrorCode) = 0;
	virtual void OnDatagramSend(int nErrorCode) = 0;

private:
	void OnReceive(int nErrorCode) override;
	void OnSend(int nErrorCode) override;
	void QueueSocketEventDispatch();

	std::atomic<bool> m_bWriteInterestEnabled;
	std::atomic<bool> m_bReceivePending;
	std::atomic<bool> m_bSendPending;
	std::atomic<bool> m_bDispatchPosted;
};
