#pragma once

#include <atlstr.h>
#include "types.h"

namespace StatusBarInfo
{
	namespace Detail
	{
		/**
		 * Formats an IPv4 value using the app's stored byte order for dotted-decimal UI text.
		 */
		inline CString FormatStoredIPv4Address(const uint32 dwIp)
		{
			const BYTE *pucIp = reinterpret_cast<const BYTE*>(&dwIp);
			CString strIp;
			strIp.Format(_T("%u.%u.%u.%u")
				, pucIp[0]
				, pucIp[1]
				, pucIp[2]
				, pucIp[3]);
			return strIp;
		}

		/**
		 * Normalizes an empty bind selection to the user-facing "all interfaces" label.
		 */
		inline CString GetBindAddressDisplayValue(const CString &strBindAddress)
		{
			return strBindAddress.IsEmpty() ? CString(_T("Any")) : strBindAddress;
		}
	}

	/**
	 * Formats the compact bind/public address summary shown in the status bar.
	 */
	inline CString FormatNetworkAddressPaneText(const CString &strBindAddress, uint32 dwPublicIp)
	{
		CString strPaneText;
		strPaneText.Format(_T("B:%s|P:%s")
			, (LPCTSTR)Detail::GetBindAddressDisplayValue(strBindAddress)
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatStoredIPv4Address(dwPublicIp) : _T("?"));
		return strPaneText;
	}

	/**
	 * Formats the single-line tooltip text for the status bar address pane.
	 */
	inline CString FormatNetworkAddressPaneToolTip(const CString &strBindAddress, uint32 dwPublicIp)
	{
		CString strToolTip;
		strToolTip.Format(_T("Bind IP: %s | Public IP: %s")
			, strBindAddress.IsEmpty() ? _T("Any interface") : (LPCTSTR)strBindAddress
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatStoredIPv4Address(dwPublicIp) : _T("Unknown"));
		return strToolTip;
	}
}
