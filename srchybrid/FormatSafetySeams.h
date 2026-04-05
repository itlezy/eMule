#pragma once

#include <atlstr.h>
#include "types.h"

namespace FormatSafetySeams
{
/**
 * @brief Formats an IPv4 endpoint from the app's stored display-order address and port.
 */
inline CString FormatStoredIPv4Endpoint(const uint32 dwDisplayIp, const uint16 nPort)
{
	const BYTE *pucIp = reinterpret_cast<const BYTE*>(&dwDisplayIp);
	CString strEndpoint;
	strEndpoint.Format(_T("%u.%u.%u.%u:%u")
		, pucIp[0]
		, pucIp[1]
		, pucIp[2]
		, pucIp[3]
		, static_cast<unsigned int>(nPort));
	return strEndpoint;
}

/**
 * @brief Formats a decimal ASCII port string for miniupnpc calls without relying on a fixed raw buffer.
 */
inline CStringA FormatDecimalPortValueA(const uint16 nPort)
{
	CStringA strPort;
	strPort.Format("%hu", nPort);
	return strPort;
}
}
