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
#include <memory>

#define FD_FORCEREAD (1<<15)
#define FD_DEFAULT (FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)

class CAsyncSocketExLayer;
struct CAsyncSocketPollEntry;
class CSocketPoller;

enum AsyncSocketExState : uint8
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

class CAsyncSocketEx : public CObject
{
	DECLARE_DYNAMIC(CAsyncSocketEx)
	friend class CSocketPoller;

public:
	CAsyncSocketEx();
	virtual ~CAsyncSocketEx();

	bool Create(UINT nSocketPort = 0
			, int nSocketType = SOCK_STREAM
			, long lEvent = FD_DEFAULT
			, const CString &sSocketAddress = CString()
			, ADDRESS_FAMILY nFamily = AF_INET
			, bool reusable = false);

	BOOL Attach(SOCKET hSocket, long lEvent = FD_DEFAULT);
	SOCKET Detach();

	static int GetLastError() { return WSAGetLastError(); }

	bool GetPeerName(CString &rPeerAddress, UINT &rPeerPort);
	BOOL GetPeerName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen);

	bool GetSockName(CString &rSocketAddress, UINT &rSocketPort) const;
	BOOL GetSockName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen) const;

	BOOL GetSockOpt(int nOptionName, void *lpOptionValue, int *lpOptionLen, int nLevel = SOL_SOCKET) const;
	BOOL SetSockOpt(int nOptionName, const void *lpOptionValue, int nOptionLen, int nLevel = SOL_SOCKET);

	ADDRESS_FAMILY GetFamily() const { return m_SocketData.nFamily; }
	bool SetFamily(ADDRESS_FAMILY nFamily);

	virtual BOOL Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr = NULL, int *lpSockAddrLen = NULL);
	BOOL AsyncSelect(long lEvent = FD_DEFAULT);
	bool Bind(UINT nSocketPort, const CString &sSocketAddress = CString());
	BOOL Bind(const LPSOCKADDR lpSockAddr, int nSockAddrLen);
	virtual void Close();
	virtual bool Connect(const CString &sHostAddress, UINT nHostPort);
	virtual BOOL Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen);
	BOOL IOCtl(long lCommand, DWORD *lpArgument);
	BOOL Listen(int nConnectionBacklog = 5);
	virtual int Receive(void *lpBuf, int nBufLen, int nFlags = 0);
	virtual int Send(const void *lpBuf, int nBufLen, int nFlags = 0);
	BOOL ShutDown(int nHow = CAsyncSocket::sends);

	virtual void OnAccept(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual bool OnHostNameResolved(const SOCKADDR_IN *pSockAddr);

	virtual void RemoveAllLayers();
	BOOL AddLayer(CAsyncSocketExLayer *pLayer);
	bool IsLayerAttached() const { return false; }

	SOCKET GetSocketHandle() const { return m_SocketData.hSocket; }
	BOOL TriggerEvent(long lEvent);

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext &dc) const;
#endif

protected:
	struct t_AsyncSocketExData
	{
		addrinfo *addrInfo;
		addrinfo *nextAddr;
		SOCKET hSocket;
		int nSocketIndex;
		ADDRESS_FAMILY nFamily;
		bool bIsClosing;
	} m_SocketData;

	bool InitAsyncSocketExInstance();
	void FreeAsyncSocketExInstance();
	void AttachHandle();
	void DetachHandle();
	bool TryNextProtocol();

#ifndef NOSOCKETSTATES
	AsyncSocketExState GetState() const { return m_nState; }
	void SetState(AsyncSocketExState nState) { m_nState = nState; }
#endif

	CString m_sSocketAddress;
	UINT m_nSocketPort;
	long m_lEvent;
	int m_nSocketType;
	bool m_bReusable;
	std::shared_ptr<CAsyncSocketPollEntry> m_pSocketEntry;

#ifndef NOSOCKETSTATES
	AsyncSocketExState m_nState;
#endif

private:
	bool CreateSocketHandle(int nSocketType, ADDRESS_FAMILY nFamily, int nProtocol = 0);
	bool AttachSocketHandle(SOCKET hSocket, ADDRESS_FAMILY nFamily);
	void WaitForCallbacksToDrain(const std::shared_ptr<CAsyncSocketPollEntry> &pEntry);
	static bool SetSocketNonBlocking(SOCKET hSocket);
};

inline CString Inet6AddrToString(const in6_addr &addr)
{
	CString buf;
	buf.Format(_T("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x")
		, addr.s6_bytes[0], addr.s6_bytes[1], addr.s6_bytes[2], addr.s6_bytes[3]
		, addr.s6_bytes[4], addr.s6_bytes[5], addr.s6_bytes[6], addr.s6_bytes[7]
		, addr.s6_bytes[8], addr.s6_bytes[9], addr.s6_bytes[10], addr.s6_bytes[11]
		, addr.s6_bytes[12], addr.s6_bytes[13], addr.s6_bytes[14], addr.s6_bytes[15]);
	return buf;
}
