//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

enum EOracleUdpTransportMode : uint8
{
	ORACLE_UDP_TRANSPORT_PLAINTEXT = 0,
	ORACLE_UDP_TRANSPORT_NODE_ID,
	ORACLE_UDP_TRANSPORT_RECEIVER_VERIFY_KEY,
	ORACLE_UDP_TRANSPORT_USER_HASH,
	ORACLE_UDP_TRANSPORT_SERVER_BASE_KEY
};

class CEncryptedDatagramSocket
{
public:
	CEncryptedDatagramSocket() = default;
	virtual	~CEncryptedDatagramSocket() = default;

protected:
	static int DecryptReceivedClient(BYTE *pbyBufIn, int nBufLen, BYTE **ppbyBufOut, uint32 dwIP, uint32 *nReceiverVerifyKey, uint32 *nSenderVerifyKey, EOracleUdpTransportMode *pTransportMode);
	static uint32 EncryptSendClient(uchar *pbyBuf, uint32 nBufLen, const uchar *pachClientHashOrKadID, bool bKad, uint32 nReceiverVerifyKey, uint32 nSenderVerifyKey, EOracleUdpTransportMode *pTransportMode);

	static int DecryptReceivedServer(BYTE *pbyBufIn, int nBufLen, BYTE **ppbyBufOut, uint32 dwBaseKey, uint32 dbgIP, EOracleUdpTransportMode *pTransportMode);
	static uint32 EncryptSendServer(uchar *pbyBuf, uint32 nBufLen, uint32 dwBaseKey, EOracleUdpTransportMode *pTransportMode);

	static int EncryptOverheadSize(bool bKad);
};
