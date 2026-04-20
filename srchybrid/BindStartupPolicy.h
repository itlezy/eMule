#pragma once

#include <atlstr.h>
#include "BindAddressResolver.h"

namespace BindStartupPolicy
{
	/**
	 * Reports whether the user explicitly selected a bind interface or bind IP.
	 */
	inline bool HasExplicitBindSelection(const CString &strInterfaceId, const CString &strConfiguredAddress)
	{
		return !strInterfaceId.IsEmpty() || !strConfiguredAddress.IsEmpty();
	}

	/**
	 * Decides whether startup networking must be blocked for the current session.
	 */
	inline bool ShouldBlockSessionNetworking(bool bEnabled
		, const CString &strInterfaceId
		, const CString &strConfiguredAddress
		, EBindAddressResolveResult eResult)
	{
		return bEnabled && HasExplicitBindSelection(strInterfaceId, strConfiguredAddress) && eResult != BARR_Resolved;
	}

	/**
	 * Formats the configured bind target for logs and diagnostics.
	 */
	inline CString FormatConfiguredBindTarget(const CString &strInterfaceName
		, const CString &strInterfaceId
		, const CString &strConfiguredAddress)
	{
		CString strTarget;
		if (!strInterfaceName.IsEmpty())
			strTarget = strInterfaceName;
		else if (!strInterfaceId.IsEmpty())
			strTarget = strInterfaceId;
		else
			strTarget = _T("Any interface");

		if (!strConfiguredAddress.IsEmpty()) {
			if (!strTarget.IsEmpty())
				strTarget.Append(_T(" / "));
			strTarget.Append(strConfiguredAddress);
		}
		return strTarget;
	}

	/**
	 * Explains why startup networking was blocked for the current bind selection.
	 */
	inline CString FormatStartupBlockReason(const CString &strInterfaceName
		, const CString &strInterfaceId
		, const CString &strConfiguredAddress
		, EBindAddressResolveResult eResult)
	{
		const CString strTarget = FormatConfiguredBindTarget(strInterfaceName, strInterfaceId, strConfiguredAddress);

		switch (eResult) {
		case BARR_InterfaceNotFound:
			return _T("Networking disabled for this session because the selected bind interface is no longer available: ") + strTarget;
		case BARR_InterfaceNameAmbiguous:
			return _T("Networking disabled for this session because the selected bind interface name matches multiple live adapters: ") + strTarget;
		case BARR_InterfaceHasNoAddress:
			return _T("Networking disabled for this session because the selected bind interface has no usable IPv4 address: ") + strTarget;
		case BARR_AddressNotFoundOnInterface:
			return _T("Networking disabled for this session because the selected bind IP is no longer present on the selected interface: ") + strTarget;
		case BARR_AddressNotFound:
			return _T("Networking disabled for this session because the selected bind IP is no longer present on any live interface: ") + strTarget;
		case BARR_Default:
		case BARR_Resolved:
		default:
			return CString();
		}
	}
}
