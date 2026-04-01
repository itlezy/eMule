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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

#include <condition_variable>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include "UploadBandwidthThrottler.h" // ZZ:UploadBandWithThrottler (UDP)
#include "AsyncDatagramSocket.h"
#include "EncryptedDatagramSocket.h"

class CServerConnect;
struct SServerUDPPacket;
struct SServerDNSRequest;
class Packet;
class CServer;

class CUDPSocket : public CAsyncDatagramSocket, public CEncryptedDatagramSocket, public ThrottledControlSocket // ZZ:UploadBandWithThrottler (UDP)
{
	friend class CServerConnect;

public:
	CUDPSocket();
	virtual	~CUDPSocket();
	CUDPSocket(const CUDPSocket&) = delete;
	CUDPSocket& operator=(const CUDPSocket&) = delete;

	bool Create();
	void DispatchQueuedWork();
	SocketSentBytes SendControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize); // ZZ:UploadBandWithThrottler (UDP)
	void SendPacket(Packet *packet, CServer *pServer, uint16 nSpecialPort = 0, BYTE *pInRawPacket = NULL, uint32 nRawLen = 0);

protected:
	virtual void OnDatagramSend(int nErrorCode) override;
	virtual void OnDatagramReceive(int nErrorCode) override;

private:
	/**
	 * @brief Flushes all completed DNS lookups onto the legacy server-state update path.
	 */
	void ProcessCompletedDnsRequests();
	void DnsResolverThreadMain();
	void ExpireDnsRequests(DWORD dwNow);
	void SendBuffer(uint32 nIP, uint16 nPort, BYTE *pPacket, UINT uSize);
	bool ProcessPacket(const BYTE *packet, UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort);
	void ProcessPacketError(UINT size, UINT opcode, uint32 nIP, uint16 nUDPPort, LPCTSTR pszError);
	bool IsBusy() const						{ return m_bWouldBlock; }
	int SendTo(BYTE *lpBuf, int nBufLen, uint32 dwIP, uint16 nPort);

	std::list<std::shared_ptr<SServerDNSRequest>> m_dnsRequests;
	std::deque<std::shared_ptr<SServerDNSRequest>> m_dnsWorkQueue;
	std::deque<std::shared_ptr<SServerDNSRequest>> m_dnsCompletedQueue;
	std::mutex m_dnsMutex;
	std::condition_variable m_dnsWorkReady;
	bool m_bStopDnsResolver;
	CTypedPtrList<CPtrList, SServerUDPPacket*> controlpacket_queue;
	CCriticalSection sendLocker; // ZZ:UploadBandWithThrottler (UDP)
	bool m_bWouldBlock;
	std::thread m_dnsResolverThread;
};
