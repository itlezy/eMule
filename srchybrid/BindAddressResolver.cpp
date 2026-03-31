//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( devs@emule-project.net / https://www.emule-project.net )
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
#include "stdafx.h"
#include <iphlpapi.h>
#include "BindAddressResolver.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	static CString GetIPv4String(const SOCKADDR *pSockAddr)
	{
		if (pSockAddr == NULL || pSockAddr->sa_family != AF_INET)
			return CString();

		const SOCKADDR_IN *pSockAddrIn = reinterpret_cast<const SOCKADDR_IN*>(pSockAddr);
		return ipstr(pSockAddrIn->sin_addr);
	}

	static void AddUniqueAddress(std::vector<CString> &addresses, const CString &strAddress)
	{
		if (strAddress.IsEmpty())
			return;

		for (std::vector<CString>::const_iterator it = addresses.begin(); it != addresses.end(); ++it) {
			if (!it->CompareNoCase(strAddress))
				return;
		}
		addresses.push_back(strAddress);
	}
}

std::vector<BindableNetworkInterface> CBindAddressResolver::GetBindableInterfaces()
{
	std::vector<BindableNetworkInterface> interfaces;

	ULONG ulOutBufLen = 16 * 1024;
	std::vector<BYTE> buffer(ulOutBufLen);
	ULONG ulFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	DWORD dwResult = ERROR_BUFFER_OVERFLOW;

	for (int iRetry = 0; iRetry < 3 && dwResult == ERROR_BUFFER_OVERFLOW; ++iRetry) {
		dwResult = GetAdaptersAddresses(AF_INET, ulFlags, NULL
			, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &ulOutBufLen);
		if (dwResult == ERROR_BUFFER_OVERFLOW)
			buffer.resize(ulOutBufLen);
	}

	if (dwResult != NO_ERROR)
		return interfaces;

	for (const IP_ADAPTER_ADDRESSES *pAdapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buffer.data())
		; pAdapter != NULL; pAdapter = pAdapter->Next) {
		if (pAdapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || pAdapter->OperStatus != IfOperStatusUp)
			continue;

		BindableNetworkInterface iface;
		iface.strId = CString(CA2T(pAdapter->AdapterName));
		iface.strName = pAdapter->FriendlyName != NULL ? CString(pAdapter->FriendlyName) : CString();
		iface.strName.Trim();

		for (const IP_ADAPTER_UNICAST_ADDRESS *pAddress = pAdapter->FirstUnicastAddress
			; pAddress != NULL; pAddress = pAddress->Next) {
			AddUniqueAddress(iface.addresses, GetIPv4String(pAddress->Address.lpSockaddr));
		}

		if (iface.addresses.empty())
			continue;

		iface.strDisplayName = iface.strName.IsEmpty() ? iface.strId : iface.strName;
		if (!iface.addresses.front().IsEmpty())
			iface.strDisplayName.AppendFormat(_T(" (%s)"), (LPCTSTR)iface.addresses.front());

		interfaces.push_back(iface);
	}

	return interfaces;
}

EBindAddressResolveResult CBindAddressResolver::ResolveBindAddress(const CString &strInterfaceId
	, const CString &strConfiguredAddress
	, CString &strResolvedAddress
	, CString *pstrResolvedInterfaceName)
{
	strResolvedAddress.Empty();
	if (pstrResolvedInterfaceName != NULL)
		pstrResolvedInterfaceName->Empty();

	if (strInterfaceId.IsEmpty()) {
		if (strConfiguredAddress.IsEmpty())
			return BARR_Default;

		const std::vector<BindableNetworkInterface> interfaces = GetBindableInterfaces();
		for (std::vector<BindableNetworkInterface>::const_iterator it = interfaces.begin(); it != interfaces.end(); ++it) {
			for (std::vector<CString>::const_iterator itAddress = it->addresses.begin(); itAddress != it->addresses.end(); ++itAddress) {
				if (itAddress->CompareNoCase(strConfiguredAddress))
					continue;

				strResolvedAddress = *itAddress;
				if (pstrResolvedInterfaceName != NULL)
					*pstrResolvedInterfaceName = it->strName;
				return BARR_Resolved;
			}
		}

		return BARR_AddressNotFound;
	}

	const std::vector<BindableNetworkInterface> interfaces = GetBindableInterfaces();
	for (std::vector<BindableNetworkInterface>::const_iterator it = interfaces.begin(); it != interfaces.end(); ++it) {
		if (it->strId.CompareNoCase(strInterfaceId))
			continue;

		if (pstrResolvedInterfaceName != NULL)
			*pstrResolvedInterfaceName = it->strName;

		if (it->addresses.empty())
			return BARR_InterfaceHasNoAddress;

		if (strConfiguredAddress.IsEmpty()) {
			strResolvedAddress = it->addresses.front();
			return BARR_Resolved;
		}

		for (std::vector<CString>::const_iterator itAddress = it->addresses.begin(); itAddress != it->addresses.end(); ++itAddress) {
			if (!itAddress->CompareNoCase(strConfiguredAddress)) {
				strResolvedAddress = *itAddress;
				return BARR_Resolved;
			}
		}

		strResolvedAddress = it->addresses.front();
		return BARR_AddressNotFoundOnInterface;
	}

	return BARR_InterfaceNotFound;
}
