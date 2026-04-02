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

#include "stdafx.h"
#include "AsyncDatagramSocket.h"
#include "AsyncDatagramSocketSeams.h"
#include "WorkerUiMessageSeams.h"
#include "UserMsgs.h"
#include "emule.h"
#include "EmuleDlg.h"

CAsyncDatagramSocket::CAsyncDatagramSocket()
	: m_bWriteInterestEnabled()
	, m_bReceivePending()
	, m_bSendPending()
	, m_bDispatchPosted()
{
}

bool CAsyncDatagramSocket::CreateDatagram(UINT nSocketPort, const CString &sSocketAddress, ADDRESS_FAMILY nFamily)
{
	if (!Create(nSocketPort, SOCK_DGRAM, GetAsyncDatagramEventMask(false), sSocketAddress, nFamily))
		return false;

#ifndef NOSOCKETSTATES
	SetState(attached);
#endif
	return true;
}

void CAsyncDatagramSocket::Close()
{
	m_bWriteInterestEnabled.store(false, std::memory_order_release);
	m_bReceivePending.store(false, std::memory_order_release);
	m_bSendPending.store(false, std::memory_order_release);
	m_bDispatchPosted.store(false, std::memory_order_release);
	CAsyncSocketEx::Close();
}

void CAsyncDatagramSocket::SetWriteInterestEnabled(bool bEnabled)
{
	if (m_bWriteInterestEnabled.exchange(bEnabled, std::memory_order_acq_rel) == bEnabled)
		return;

	if (GetSocketHandle() != INVALID_SOCKET)
		AsyncSelect(GetAsyncDatagramEventMask(bEnabled));
}

bool CAsyncDatagramSocket::PostDatagramDispatchMessage()
{
	const HWND hMainWnd = theApp.emuledlg != NULL ? theApp.emuledlg->GetSafeHwnd() : NULL;
	return TryPostWorkerUiMessage(hMainWnd, UM_WSAPOLL_UDP_SOCKET);
}

void CAsyncDatagramSocket::QueueSocketEventDispatch()
{
	bool bExpected = false;
	if (m_bDispatchPosted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel)) {
		// Reset the posted gate when the UI window is already gone so future wakeups can retry cleanly.
		if (!PostDatagramDispatchMessage())
			m_bDispatchPosted.store(false, std::memory_order_release);
	}
}

void CAsyncDatagramSocket::OnReceive(int /*nErrorCode*/)
{
	m_bReceivePending.store(true, std::memory_order_release);
	QueueSocketEventDispatch();
}

void CAsyncDatagramSocket::OnSend(int /*nErrorCode*/)
{
	m_bSendPending.store(true, std::memory_order_release);
	QueueSocketEventDispatch();
}

void CAsyncDatagramSocket::DispatchQueuedSocketEvents()
{
	for (;;) {
		m_bDispatchPosted.store(false, std::memory_order_release);

		const bool bDispatchReceive = m_bReceivePending.exchange(false, std::memory_order_acq_rel);
		const bool bDispatchSend = m_bSendPending.exchange(false, std::memory_order_acq_rel);
		if (!bDispatchReceive && !bDispatchSend)
			break;

		if (bDispatchReceive)
			OnDatagramReceive(0);
		if (bDispatchSend)
			OnDatagramSend(0);
	}

	if (m_bReceivePending.load(std::memory_order_acquire) || m_bSendPending.load(std::memory_order_acquire))
		QueueSocketEventDispatch();
}
