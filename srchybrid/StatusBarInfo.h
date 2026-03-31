#pragma once

#include <atlstr.h>
#include "types.h"

namespace StatusBarInfo
{
	namespace Detail
	{
		/**
		 * Converts a host-order IPv4 address into a dotted-decimal string for status UI.
		 */
		inline CString FormatIPv4Address(const uint32 dwIp)
		{
			CString strIp;
			strIp.Format(_T("%u.%u.%u.%u")
				, (dwIp >> 24) & 0xFF
				, (dwIp >> 16) & 0xFF
				, (dwIp >> 8) & 0xFF
				, dwIp & 0xFF);
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
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatIPv4Address(dwPublicIp) : _T("?"));
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
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatIPv4Address(dwPublicIp) : _T("Unknown"));
		return strToolTip;
	}
}
