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
#include "AsyncSocketEx.h"
#include "AsyncSocketExSeams.h"
#include "Log.h"
#include "OtherFunctions.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

constexpr int kSocketPollTimeoutMs = 50;

struct CAsyncSocketPollEntry
{
	explicit CAsyncSocketPollEntry(CAsyncSocketEx *pOwner, SOCKET hSocket, long lEvent)
		: pOwner(pOwner)
		, hSocket(hSocket)
		, lEventMask(lEvent)
		, callbacksInFlight()
	{
	}

	std::atomic<CAsyncSocketEx*> pOwner;
	SOCKET hSocket;
	std::atomic<long> lEventMask;
	std::atomic<long> callbacksInFlight;
};

class CSocketPoller
{
public:
	static CSocketPoller& Instance()
	{
		static CSocketPoller instance;
		return instance;
	}

	std::shared_ptr<CAsyncSocketPollEntry> Register(CAsyncSocketEx *pOwner, SOCKET hSocket, long lEvent)
	{
		EnsureRunning();
		std::shared_ptr<CAsyncSocketPollEntry> pEntry = std::make_shared<CAsyncSocketPollEntry>(pOwner, hSocket, lEvent);
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_entries.push_back(pEntry);
		}
		return pEntry;
	}

	void Unregister(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry)
	{
		if (!pEntry)
			return;

		pEntry->pOwner.store(NULL, std::memory_order_release);
		std::lock_guard<std::mutex> lock(m_mutex);
		m_entries.erase(std::remove(m_entries.begin(), m_entries.end(), pEntry), m_entries.end());
	}

	void UpdateEvents(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry, long lEvent)
	{
		if (pEntry)
			pEntry->lEventMask.store(lEvent, std::memory_order_release);
	}

	bool IsNetworkThread() const
	{
		return std::this_thread::get_id() == m_threadId.load(std::memory_order_acquire);
	}

private:
	CSocketPoller()
		: m_running()
		, m_stopRequested()
	{
	}

	~CSocketPoller()
	{
		m_stopRequested.store(true, std::memory_order_release);
		if (m_thread.joinable())
			m_thread.join();
	}

	void EnsureRunning()
	{
		bool bExpected = false;
		if (m_running.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			m_thread = std::thread(&CSocketPoller::ThreadMain, this);
	}

	void ThreadMain()
	{
		m_threadId.store(std::this_thread::get_id(), std::memory_order_release);

		while (!m_stopRequested.load(std::memory_order_acquire)) {
			std::vector<std::shared_ptr<CAsyncSocketPollEntry>> entries;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				entries = m_entries;
			}

			if (entries.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(kSocketPollTimeoutMs));
				continue;
			}

			std::vector<WSAPOLLFD> pollFds;
			std::vector<std::shared_ptr<CAsyncSocketPollEntry>> readyEntries;
			pollFds.reserve(entries.size());
			readyEntries.reserve(entries.size());

			for (const std::shared_ptr<CAsyncSocketPollEntry> &pEntry : entries) {
				CAsyncSocketEx *pOwner = pEntry->pOwner.load(std::memory_order_acquire);
				if (!pOwner || pEntry->hSocket == INVALID_SOCKET)
					continue;

				const long lEvent = pEntry->lEventMask.load(std::memory_order_acquire);

#ifndef NOSOCKETSTATES
				const AsyncSocketExState state = pOwner->GetState();
#else
				const AsyncSocketExState state = connected;
#endif
				const short nPollEvents = GetAsyncSocketPollEvents(state, lEvent);

				if (nPollEvents == 0)
					continue;

				WSAPOLLFD pfd = {};
				pfd.fd = pEntry->hSocket;
				pfd.events = nPollEvents;
				pollFds.push_back(pfd);
				readyEntries.push_back(pEntry);
			}

			if (pollFds.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(kSocketPollTimeoutMs));
				continue;
			}

			const int rc = WSAPoll(pollFds.data(), static_cast<ULONG>(pollFds.size()), kSocketPollTimeoutMs);
			if (rc == 0)
				continue;
			if (HasAsyncSocketPollFailure(rc)) {
				const int nError = WSAGetLastError();
				DebugLogError(_T("WSAPoll failed in AsyncSocketEx poller: %s"), (LPCTSTR)GetErrorMessage(nError, 1));
				std::this_thread::sleep_for(std::chrono::milliseconds(kSocketPollTimeoutMs));
				continue;
			}

			for (size_t i = 0; i < pollFds.size(); ++i) {
				if (pollFds[i].revents == 0)
					continue;
				Dispatch(readyEntries[i], pollFds[i]);
			}
		}
	}

	static CAsyncSocketEx* AcquireOwner(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry)
	{
		CAsyncSocketEx *pOwner = pEntry->pOwner.load(std::memory_order_acquire);
		if (!pOwner)
			return NULL;

		pEntry->callbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
		pOwner = pEntry->pOwner.load(std::memory_order_acquire);
		if (!pOwner) {
			pEntry->callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
			return NULL;
		}
		return pOwner;
	}

	static void ReleaseOwner(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry)
	{
		pEntry->callbacksInFlight.fetch_sub(1, std::memory_order_acq_rel);
	}

	static int QuerySocketError(SOCKET hSocket)
	{
		int nError = 0;
		int nErrorLen = sizeof nError;
		if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&nError), &nErrorLen) == SOCKET_ERROR)
			return WSAGetLastError();
		return nError;
	}

	static void Dispatch(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry, const WSAPOLLFD &pfd)
	{
		CAsyncSocketEx *pOwner = AcquireOwner(pEntry);
		if (!pOwner)
			return;

		const long lEvent = pEntry->lEventMask.load(std::memory_order_acquire);

#ifndef NOSOCKETSTATES
		const AsyncSocketExState state = pOwner->GetState();
#else
		const AsyncSocketExState state = connected;
#endif

		if (ShouldCompleteAsyncSocketConnect(state, pfd.revents)) {
			const int nError = (pfd.revents & POLLNVAL) ? WSAENOTSOCK : QuerySocketError(pEntry->hSocket);
			if (nError != 0 && pOwner->TryNextProtocol()) {
				ReleaseOwner(pEntry);
				return;
			}

#ifndef NOSOCKETSTATES
			pOwner->SetState(nError == 0 ? connected : aborted);
#endif
			if (lEvent & FD_CONNECT)
				pOwner->OnConnect(nError);
			ReleaseOwner(pEntry);
			return;
		}

		if (ShouldDispatchAsyncSocketAccept(state, lEvent, pfd.revents)) {
			pOwner->OnAccept(0);
			ReleaseOwner(pEntry);
			return;
		}

		if (ShouldDispatchAsyncSocketRead(state, lEvent, pfd.revents))
			pOwner->OnReceive(0);

		ReleaseOwner(pEntry);

		pOwner = AcquireOwner(pEntry);
		if (!pOwner)
			return;

#ifndef NOSOCKETSTATES
		const AsyncSocketExState refreshedState = pOwner->GetState();
#else
		const AsyncSocketExState refreshedState = connected;
#endif

		if (ShouldDispatchAsyncSocketWrite(refreshedState, lEvent, pfd.revents))
			pOwner->OnSend(0);

		ReleaseOwner(pEntry);

		if (!HasAsyncSocketCloseSignal(pfd.revents))
			return;

		pOwner = AcquireOwner(pEntry);
		if (!pOwner)
			return;

		int nError = QuerySocketError(pEntry->hSocket);
		if ((pfd.revents & POLLNVAL) && nError == 0)
			nError = WSAENOTSOCK;

		DWORD nBytes = 0;
		const bool bHasPendingData = (lEvent & FD_READ) && pOwner->IOCtl(FIONREAD, &nBytes) && nBytes > 0;
		const AsyncSocketExCloseAction closeAction = ClassifyAsyncSocketClose(refreshedState, lEvent, pfd.revents, bHasPendingData);
		if (closeAction.bShouldReadDrain) {
			pOwner->m_SocketData.bIsClosing = true;
			pOwner->OnReceive(WSAESHUTDOWN);
			ReleaseOwner(pEntry);

			pOwner = AcquireOwner(pEntry);
			if (!pOwner)
				return;
		}

#ifndef NOSOCKETSTATES
		pOwner->SetState(nError != 0 ? aborted : closed);
#endif
		if (closeAction.bShouldClose)
			pOwner->OnClose(nError);
		ReleaseOwner(pEntry);
	}

	std::mutex m_mutex;
	std::vector<std::shared_ptr<CAsyncSocketPollEntry>> m_entries;
	std::thread m_thread;
	std::atomic<bool> m_running;
	std::atomic<bool> m_stopRequested;
	std::atomic<std::thread::id> m_threadId;
};

IMPLEMENT_DYNAMIC(CAsyncSocketEx, CObject)

CAsyncSocketEx::CAsyncSocketEx()
	: m_SocketData{}
	, m_sSocketAddress()
	, m_nSocketPort()
	, m_lEvent(FD_DEFAULT)
	, m_nSocketType(SOCK_STREAM)
	, m_bReusable()
	, m_pSocketEntry()
#ifndef NOSOCKETSTATES
	, m_nState(notsock)
#endif
{
	m_SocketData.hSocket = INVALID_SOCKET;
}

CAsyncSocketEx::~CAsyncSocketEx()
{
	Close();
	FreeAsyncSocketExInstance();
}

bool CAsyncSocketEx::SetSocketNonBlocking(SOCKET hSocket)
{
	u_long nNonBlocking = 1;
	return ioctlsocket(hSocket, FIONBIO, &nNonBlocking) != SOCKET_ERROR;
}

bool CAsyncSocketEx::InitAsyncSocketExInstance()
{
	CSocketPoller::Instance();
	return true;
}

void CAsyncSocketEx::FreeAsyncSocketExInstance()
{
}

void CAsyncSocketEx::WaitForCallbacksToDrain(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry)
{
	if (!pEntry || CSocketPoller::Instance().IsNetworkThread())
		return;

	while (ShouldYieldForAsyncSocketCallbackDrain(pEntry->callbacksInFlight.load(std::memory_order_acquire)))
		::SwitchToThread();
}

bool CAsyncSocketEx::AttachSocketHandle(SOCKET hSocket, ADDRESS_FAMILY nFamily)
{
	m_SocketData.hSocket = hSocket;
	m_SocketData.nFamily = nFamily;
	AttachHandle();
	return m_SocketData.hSocket != INVALID_SOCKET;
}

bool CAsyncSocketEx::CreateSocketHandle(int nSocketType, ADDRESS_FAMILY nFamily, int nProtocol)
{
	const SOCKET hSocket = socket(nFamily, nSocketType, nProtocol);
	if (hSocket == INVALID_SOCKET)
		return false;

	if (!SetSocketNonBlocking(hSocket)) {
		const int nError = WSAGetLastError();
		closesocket(hSocket);
		WSASetLastError(nError);
		return false;
	}

	return AttachSocketHandle(hSocket, nFamily);
}

void CAsyncSocketEx::AttachHandle()
{
	if (m_SocketData.hSocket == INVALID_SOCKET)
		return;

	if (m_SocketData.nFamily == AF_UNSPEC) {
		SOCKADDR_STORAGE sockAddr = {};
		int nSockAddrLen = sizeof sockAddr;
		if (getsockname(m_SocketData.hSocket, reinterpret_cast<LPSOCKADDR>(&sockAddr), &nSockAddrLen) == 0)
			m_SocketData.nFamily = sockAddr.ss_family;
	}

	m_pSocketEntry = CSocketPoller::Instance().Register(this, m_SocketData.hSocket, m_lEvent);
#ifndef NOSOCKETSTATES
	SetState(attached);
#endif
}

void CAsyncSocketEx::DetachHandle()
{
	std::shared_ptr<CAsyncSocketPollEntry> pEntry = m_pSocketEntry;
	m_pSocketEntry.reset();
	if (pEntry) {
		CSocketPoller::Instance().Unregister(pEntry);
		WaitForCallbacksToDrain(pEntry);
	}

	m_SocketData.hSocket = INVALID_SOCKET;
	m_SocketData.bIsClosing = false;
#ifndef NOSOCKETSTATES
	SetState(notsock);
#endif
}

bool CAsyncSocketEx::Create(UINT nSocketPort, int nSocketType, long lEvent, const CString &sSocketAddress, ADDRESS_FAMILY nFamily, bool reusable)
{
	if (GetSocketHandle() != INVALID_SOCKET) {
		WSASetLastError(WSAEALREADY);
		return false;
	}

	if (!InitAsyncSocketExInstance()) {
		WSASetLastError(WSANOTINITIALISED);
		return false;
	}

	m_lEvent = lEvent;
	m_nSocketPort = nSocketPort;
	m_sSocketAddress = sSocketAddress;
	m_nSocketType = nSocketType;
	m_bReusable = reusable;
	m_SocketData.nFamily = nFamily;

	if (nFamily == AF_UNSPEC) {
#ifndef NOSOCKETSTATES
		SetState(unconnected);
#endif
		return true;
	}

	if (!CreateSocketHandle(nSocketType, nFamily))
		return false;

	if (reusable && nSocketPort != 0) {
		BOOL value = TRUE;
		SetSockOpt(SO_REUSEADDR, &value, sizeof value);
	}

	if (!Bind(nSocketPort, sSocketAddress)) {
		Close();
		return false;
	}

#ifndef NOSOCKETSTATES
	SetState(unconnected);
#endif
	return true;
}

bool CAsyncSocketEx::OnHostNameResolved(const SOCKADDR_IN * /*pSockAddr*/)
{
	return true;
}

void CAsyncSocketEx::OnReceive(int /*nErrorCode*/)
{
}

void CAsyncSocketEx::OnSend(int /*nErrorCode*/)
{
}

void CAsyncSocketEx::OnConnect(int /*nErrorCode*/)
{
}

void CAsyncSocketEx::OnAccept(int /*nErrorCode*/)
{
}

void CAsyncSocketEx::OnClose(int /*nErrorCode*/)
{
}

bool CAsyncSocketEx::Bind(UINT nSocketPort, const CString &sSocketAddress)
{
	m_nSocketPort = nSocketPort;
	m_sSocketAddress = sSocketAddress;

	if (m_SocketData.nFamily == AF_UNSPEC || m_SocketData.hSocket == INVALID_SOCKET)
		return true;

	const CStringA sAscii(sSocketAddress);
	if (sAscii.IsEmpty()) {
		if (m_SocketData.nFamily == AF_INET) {
			SOCKADDR_IN sockAddr = {};
			sockAddr.sin_family = AF_INET;
			sockAddr.sin_addr.s_addr = INADDR_ANY;
			sockAddr.sin_port = htons(static_cast<u_short>(nSocketPort));
			return Bind(reinterpret_cast<LPSOCKADDR>(&sockAddr), sizeof sockAddr);
		}

		if (m_SocketData.nFamily == AF_INET6) {
			SOCKADDR_IN6 sockAddr6 = {};
			sockAddr6.sin6_family = AF_INET6;
			sockAddr6.sin6_addr = in6addr_any;
			sockAddr6.sin6_port = htons(static_cast<u_short>(nSocketPort));
			return Bind(reinterpret_cast<LPSOCKADDR>(&sockAddr6), sizeof sockAddr6);
		}
		return false;
	}

	addrinfo hints = {};
	hints.ai_family = m_SocketData.nFamily;
	hints.ai_socktype = m_nSocketType;
	CStringA port;
	port.Format("%u", nSocketPort);
	addrinfo *res0 = NULL;
	if (getaddrinfo(sAscii, port, &hints, &res0))
		return false;

	bool bResult = false;
	for (addrinfo *res = res0; res; res = res->ai_next)
		if (Bind(res->ai_addr, static_cast<int>(res->ai_addrlen))) {
			bResult = true;
			break;
		}

	freeaddrinfo(res0);
	return bResult;
}

BOOL CAsyncSocketEx::Bind(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	return bind(m_SocketData.hSocket, lpSockAddr, nSockAddrLen) != SOCKET_ERROR;
}

void CAsyncSocketEx::Close()
{
	if (m_SocketData.addrInfo) {
		freeaddrinfo(m_SocketData.addrInfo);
		m_SocketData.addrInfo = NULL;
		m_SocketData.nextAddr = NULL;
	}

	if (m_SocketData.hSocket != INVALID_SOCKET) {
		const SOCKET hSocket = m_SocketData.hSocket;
		DetachHandle();
		closesocket(hSocket);
	}

	m_SocketData.nFamily = AF_UNSPEC;
	m_sSocketAddress.Empty();
	m_nSocketPort = 0;
	m_SocketData.bIsClosing = false;
	RemoveAllLayers();
#ifndef NOSOCKETSTATES
	SetState(notsock);
#endif
}

int CAsyncSocketEx::Receive(void *lpBuf, int nBufLen, int nFlags)
{
	return recv(m_SocketData.hSocket, static_cast<char*>(lpBuf), nBufLen, nFlags);
}

int CAsyncSocketEx::ReceiveFrom(void *lpBuf, int nBufLen, LPSOCKADDR lpSockAddr, int *lpSockAddrLen, int nFlags)
{
	return recvfrom(m_SocketData.hSocket, static_cast<char*>(lpBuf), nBufLen, nFlags, lpSockAddr, lpSockAddrLen);
}

int CAsyncSocketEx::Send(const void *lpBuf, int nBufLen, int nFlags)
{
	return send(m_SocketData.hSocket, static_cast<const char*>(lpBuf), nBufLen, nFlags);
}

int CAsyncSocketEx::SendTo(const void *lpBuf, int nBufLen, const SOCKADDR *lpSockAddr, int nSockAddrLen, int nFlags)
{
	return sendto(m_SocketData.hSocket, static_cast<const char*>(lpBuf), nBufLen, nFlags, lpSockAddr, nSockAddrLen);
}

bool CAsyncSocketEx::Connect(const CString &sHostAddress, UINT nHostPort)
{
	const CStringA sAscii(sHostAddress);
	if (sAscii.IsEmpty()) {
		WSASetLastError(WSAEINVAL);
		return false;
	}

	if (m_SocketData.addrInfo) {
		freeaddrinfo(m_SocketData.addrInfo);
		m_SocketData.addrInfo = NULL;
		m_SocketData.nextAddr = NULL;
	}

	addrinfo hints = {};
	hints.ai_family = m_SocketData.nFamily == AF_UNSPEC ? AF_UNSPEC : m_SocketData.nFamily;
	hints.ai_socktype = m_nSocketType;
	CStringA port;
	port.Format("%u", nHostPort);
	if (getaddrinfo(sAscii, port, &hints, &m_SocketData.addrInfo))
		return false;

	m_SocketData.nextAddr = m_SocketData.addrInfo;
	return TryNextProtocol();
}

BOOL CAsyncSocketEx::Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	if (m_SocketData.hSocket == INVALID_SOCKET) {
		const ADDRESS_FAMILY nFamily = lpSockAddr ? lpSockAddr->sa_family : AF_UNSPEC;
		if (nFamily == AF_UNSPEC || !CreateSocketHandle(m_nSocketType, nFamily)) {
			WSASetLastError(WSAEINVAL);
			return FALSE;
		}
		if (m_bReusable && m_nSocketPort != 0) {
			BOOL value = TRUE;
			SetSockOpt(SO_REUSEADDR, &value, sizeof value);
		}
		if (!Bind(m_nSocketPort, m_sSocketAddress))
			return FALSE;
	}

	const int rc = connect(m_SocketData.hSocket, lpSockAddr, nSockAddrLen);
	const int nError = (rc == SOCKET_ERROR) ? WSAGetLastError() : 0;
	if (rc == 0 || nError == WSAEWOULDBLOCK || nError == WSAEINPROGRESS || nError == WSAEINVAL) {
#ifndef NOSOCKETSTATES
		SetState(connecting);
#endif
		if (rc == 0)
			WSASetLastError(WSAEWOULDBLOCK);
		return rc == 0;
	}
	return FALSE;
}

bool CAsyncSocketEx::TryNextProtocol()
{
	while (m_SocketData.nextAddr) {
		addrinfo *pAddr = m_SocketData.nextAddr;
		m_SocketData.nextAddr = m_SocketData.nextAddr->ai_next;

		if (m_SocketData.hSocket != INVALID_SOCKET)
			Close();

		m_nSocketType = pAddr->ai_socktype;
		if (!CreateSocketHandle(pAddr->ai_socktype, static_cast<ADDRESS_FAMILY>(pAddr->ai_family), pAddr->ai_protocol))
			continue;

		if (m_bReusable && m_nSocketPort != 0) {
			BOOL value = TRUE;
			SetSockOpt(SO_REUSEADDR, &value, sizeof value);
		}
		if (!Bind(m_nSocketPort, m_sSocketAddress)) {
			Close();
			continue;
		}

		const BOOL bConnected = Connect(pAddr->ai_addr, static_cast<int>(pAddr->ai_addrlen));
		const int nError = WSAGetLastError();
		if (bConnected || nError == WSAEWOULDBLOCK || nError == WSAEINPROGRESS || nError == WSAEINVAL)
			return true;
	}

	if (m_SocketData.addrInfo) {
		freeaddrinfo(m_SocketData.addrInfo);
		m_SocketData.addrInfo = NULL;
		m_SocketData.nextAddr = NULL;
	}
	return false;
}

bool CAsyncSocketEx::GetPeerName(CString &rPeerAddress, UINT &rPeerPort)
{
	if (m_SocketData.nFamily != AF_INET && m_SocketData.nFamily != AF_INET6)
		return false;

	const int nSockAddrLen = (m_SocketData.nFamily == AF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN);
	LPSOCKADDR lpSockAddr = reinterpret_cast<LPSOCKADDR>(new char[nSockAddrLen]());
	int nLen = nSockAddrLen;
	const bool bResult = GetPeerName(lpSockAddr, &nLen);
	if (bResult) {
		if (m_SocketData.nFamily == AF_INET6) {
			rPeerPort = ntohs(reinterpret_cast<LPSOCKADDR_IN6>(lpSockAddr)->sin6_port);
			rPeerAddress = Inet6AddrToString(reinterpret_cast<LPSOCKADDR_IN6>(lpSockAddr)->sin6_addr);
		} else {
			rPeerPort = ntohs(reinterpret_cast<LPSOCKADDR_IN>(lpSockAddr)->sin_port);
			rPeerAddress = ipstr(reinterpret_cast<LPSOCKADDR_IN>(lpSockAddr)->sin_addr);
		}
	}
	delete[] lpSockAddr;
	return bResult;
}

BOOL CAsyncSocketEx::GetPeerName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	return getpeername(m_SocketData.hSocket, lpSockAddr, lpSockAddrLen) != SOCKET_ERROR;
}

bool CAsyncSocketEx::GetSockName(CString &rSocketAddress, UINT &rSocketPort) const
{
	if (m_SocketData.nFamily != AF_INET && m_SocketData.nFamily != AF_INET6)
		return false;

	const int nSockAddrLen = (m_SocketData.nFamily == AF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN);
	LPSOCKADDR lpSockAddr = reinterpret_cast<LPSOCKADDR>(new char[nSockAddrLen]());
	int nLen = nSockAddrLen;
	const bool bResult = GetSockName(lpSockAddr, &nLen);
	if (bResult) {
		if (m_SocketData.nFamily == AF_INET6) {
			rSocketPort = ntohs(reinterpret_cast<LPSOCKADDR_IN6>(lpSockAddr)->sin6_port);
			rSocketAddress = Inet6AddrToString(reinterpret_cast<LPSOCKADDR_IN6>(lpSockAddr)->sin6_addr);
		} else {
			rSocketPort = ntohs(reinterpret_cast<LPSOCKADDR_IN>(lpSockAddr)->sin_port);
			rSocketAddress = ipstr(reinterpret_cast<LPSOCKADDR_IN>(lpSockAddr)->sin_addr);
		}
	}
	delete[] lpSockAddr;
	return bResult;
}

BOOL CAsyncSocketEx::GetSockName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen) const
{
	return getsockname(m_SocketData.hSocket, lpSockAddr, lpSockAddrLen) != SOCKET_ERROR;
}

BOOL CAsyncSocketEx::Shutdown(int nHow)
{
	return shutdown(m_SocketData.hSocket, nHow) != SOCKET_ERROR;
}

SOCKET CAsyncSocketEx::Detach()
{
	const SOCKET hSocket = m_SocketData.hSocket;
	DetachHandle();
	m_SocketData.nFamily = AF_UNSPEC;
	return hSocket;
}

BOOL CAsyncSocketEx::Attach(SOCKET hSocket, long lEvent)
{
	if (hSocket == INVALID_SOCKET)
		return FALSE;

	m_lEvent = lEvent;
	if (!SetSocketNonBlocking(hSocket))
		return FALSE;

	SOCKADDR_STORAGE sockAddr = {};
	int nSockAddrLen = sizeof sockAddr;
	const ADDRESS_FAMILY nFamily = (getsockname(hSocket, reinterpret_cast<LPSOCKADDR>(&sockAddr), &nSockAddrLen) == 0)
		? sockAddr.ss_family
		: AF_INET;
	return AttachSocketHandle(hSocket, nFamily);
}

BOOL CAsyncSocketEx::AsyncSelect(long lEvent)
{
	m_lEvent = lEvent;
	CSocketPoller::Instance().UpdateEvents(m_pSocketEntry, lEvent);
	return TRUE;
}

BOOL CAsyncSocketEx::Listen(int nConnectionBacklog)
{
	if (listen(m_SocketData.hSocket, nConnectionBacklog) == SOCKET_ERROR)
		return FALSE;
#ifndef NOSOCKETSTATES
	SetState(listening);
#endif
	return TRUE;
}

BOOL CAsyncSocketEx::Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	ASSERT(rConnectedSocket.m_SocketData.hSocket == INVALID_SOCKET);

	const SOCKET hSocket = accept(m_SocketData.hSocket, lpSockAddr, lpSockAddrLen);
	if (hSocket == INVALID_SOCKET)
		return FALSE;

	if (!SetSocketNonBlocking(hSocket)) {
		const int nError = WSAGetLastError();
		closesocket(hSocket);
		WSASetLastError(nError);
		return FALSE;
	}

	rConnectedSocket.m_lEvent = FD_DEFAULT;
	rConnectedSocket.m_nSocketType = SOCK_STREAM;
	rConnectedSocket.m_bReusable = false;
	if (!rConnectedSocket.AttachSocketHandle(hSocket, GetFamily())) {
		closesocket(hSocket);
		return FALSE;
	}
#ifndef NOSOCKETSTATES
	rConnectedSocket.SetState(connected);
#endif
	return TRUE;
}

BOOL CAsyncSocketEx::IOCtl(long lCommand, DWORD *lpArgument)
{
	return ioctlsocket(m_SocketData.hSocket, lCommand, lpArgument) != SOCKET_ERROR;
}

BOOL CAsyncSocketEx::TriggerEvent(long lEvent)
{
	const int nErrorCode = 0;
	if (lEvent & FD_CONNECT)
		OnConnect(nErrorCode);
	if (lEvent & FD_ACCEPT)
		OnAccept(nErrorCode);
	if (lEvent & FD_READ)
		OnReceive(nErrorCode);
	if (lEvent & FD_WRITE)
		OnSend(nErrorCode);
	if (lEvent & FD_CLOSE)
		OnClose(nErrorCode);
	return TRUE;
}

void CAsyncSocketEx::RemoveAllLayers()
{
}

BOOL CAsyncSocketEx::GetSockOpt(int nOptionName, void *lpOptionValue, int *lpOptionLen, int nLevel) const
{
	return getsockopt(m_SocketData.hSocket, nLevel, nOptionName, static_cast<char*>(lpOptionValue), lpOptionLen) != SOCKET_ERROR;
}

BOOL CAsyncSocketEx::SetSockOpt(int nOptionName, const void *lpOptionValue, int nOptionLen, int nLevel)
{
	return setsockopt(m_SocketData.hSocket, nLevel, nOptionName, static_cast<const char*>(lpOptionValue), nOptionLen) != SOCKET_ERROR;
}

bool CAsyncSocketEx::SetFamily(ADDRESS_FAMILY nFamily)
{
	if (m_SocketData.nFamily != AF_UNSPEC)
		return false;
	m_SocketData.nFamily = nFamily;
	return true;
}

#ifdef _DEBUG
void CAsyncSocketEx::AssertValid() const
{
	CObject::AssertValid();
	ASSERT(m_SocketData.hSocket == INVALID_SOCKET || m_SocketData.nFamily != AF_UNSPEC);
}

void CAsyncSocketEx::Dump(CDumpContext &dc) const
{
	CObject::Dump(dc);
	dc << _T("socket=") << static_cast<UINT_PTR>(m_SocketData.hSocket)
		<< _T(" family=") << static_cast<int>(m_SocketData.nFamily)
		<< _T(" events=") << m_lEvent << _T("\n");
}
#endif
