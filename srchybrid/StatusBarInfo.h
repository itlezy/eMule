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

		/**
		 * Formats a single metric label/value pair used by the status bar pane summaries.
		 */
		inline CString FormatMetric(const CString &strLabel, const CString &strValue)
		{
			CString strMetric;
			strMetric.Format(_T("%s:%s"), (LPCTSTR)strLabel, (LPCTSTR)strValue);
			return strMetric;
		}

		/**
		 * Formats a split-network metric where eD2K and Kad both contribute visible counts.
		 */
		inline CString FormatDualNetworkMetric(const CString &strLabel, const CString &strPrimaryValue, const CString &strSecondaryValue)
		{
			CString strMetric;
			strMetric.Format(_T("%s:%s+%s"), (LPCTSTR)strLabel, (LPCTSTR)strPrimaryValue, (LPCTSTR)strSecondaryValue);
			return strMetric;
		}
	}

	/**
	 * Formats the compact bind/public address summary shown in the status bar.
	 */
	inline CString FormatNetworkAddressPaneText(const CString &strBindAddress, uint32 dwPublicIp, bool bBindBlocked = false)
	{
		if (bBindBlocked)
			return CString(_T("B:Blocked|P:Offline"));

		CString strPaneText;
		strPaneText.Format(_T("B:%s|P:%s")
			, (LPCTSTR)Detail::GetBindAddressDisplayValue(strBindAddress)
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatStoredIPv4Address(dwPublicIp) : _T("?"));
		return strPaneText;
	}

	/**
	 * Formats the single-line tooltip text for the status bar address pane.
	 */
	inline CString FormatNetworkAddressPaneToolTip(const CString &strBindAddress, uint32 dwPublicIp, bool bBindBlocked = false, const CString &strBindBlockedReason = CString())
	{
		if (bBindBlocked) {
			CString strToolTip(_T("Bind IP: Blocked | Public IP: Offline"));
			if (!strBindBlockedReason.IsEmpty()) {
				strToolTip.Append(_T(" | "));
				strToolTip.Append(strBindBlockedReason);
			}
			return strToolTip;
		}

		CString strToolTip;
		strToolTip.Format(_T("Bind IP: %s | Public IP: %s")
			, strBindAddress.IsEmpty() ? _T("Any interface") : (LPCTSTR)strBindAddress
			, dwPublicIp != 0 ? (LPCTSTR)Detail::FormatStoredIPv4Address(dwPublicIp) : _T("Unknown"));
		return strToolTip;
	}

	/**
	 * Formats the compact users/files summary while keeping dual-network contributions readable.
	 */
	inline CString FormatUsersPaneText(const CString &strUsersLabel
		, const CString &strFilesLabel
		, const CString &strEd2kUsers
		, const CString &strKadUsers
		, const CString &strEd2kFiles
		, const CString &strKadFiles
		, bool bHasEd2k
		, bool bHasKad)
	{
		if (bHasEd2k && bHasKad) {
			CString strPaneText;
			strPaneText.Format(_T("%s|%s")
				, (LPCTSTR)Detail::FormatDualNetworkMetric(strUsersLabel, strEd2kUsers, strKadUsers)
				, (LPCTSTR)Detail::FormatDualNetworkMetric(strFilesLabel, strEd2kFiles, strKadFiles));
			return strPaneText;
		}

		if (bHasEd2k) {
			CString strPaneText;
			strPaneText.Format(_T("%s|%s")
				, (LPCTSTR)Detail::FormatMetric(strUsersLabel, strEd2kUsers)
				, (LPCTSTR)Detail::FormatMetric(strFilesLabel, strEd2kFiles));
			return strPaneText;
		}

		if (bHasKad) {
			CString strPaneText;
			strPaneText.Format(_T("%s|%s")
				, (LPCTSTR)Detail::FormatMetric(strUsersLabel, strKadUsers)
				, (LPCTSTR)Detail::FormatMetric(strFilesLabel, strKadFiles));
			return strPaneText;
		}

		CString strPaneText;
		strPaneText.Format(_T("%s|%s")
			, (LPCTSTR)Detail::FormatMetric(strUsersLabel, _T("0"))
			, (LPCTSTR)Detail::FormatMetric(strFilesLabel, _T("0")));
		return strPaneText;
	}

	/**
	 * Formats the users/files tooltip with explicit eD2K and Kad attribution.
	 */
	inline CString FormatUsersPaneToolTip(const CString &strUsersLabel
		, const CString &strFilesLabel
		, const CString &strEd2kUsers
		, const CString &strKadUsers
		, const CString &strEd2kFiles
		, const CString &strKadFiles
		, bool bHasEd2k
		, bool bHasKad)
	{
		if (bHasEd2k && bHasKad) {
			CString strToolTip;
			strToolTip.Format(_T("%s eD2K:%s | Kad:%s | %s eD2K:%s | Kad:%s")
				, (LPCTSTR)strUsersLabel
				, (LPCTSTR)strEd2kUsers
				, (LPCTSTR)strKadUsers
				, (LPCTSTR)strFilesLabel
				, (LPCTSTR)strEd2kFiles
				, (LPCTSTR)strKadFiles);
			return strToolTip;
		}

		if (bHasEd2k) {
			CString strToolTip;
			strToolTip.Format(_T("%s eD2K:%s | %s eD2K:%s")
				, (LPCTSTR)strUsersLabel
				, (LPCTSTR)strEd2kUsers
				, (LPCTSTR)strFilesLabel
				, (LPCTSTR)strEd2kFiles);
			return strToolTip;
		}

		if (bHasKad) {
			CString strToolTip;
			strToolTip.Format(_T("%s Kad:%s | %s Kad:%s")
				, (LPCTSTR)strUsersLabel
				, (LPCTSTR)strKadUsers
				, (LPCTSTR)strFilesLabel
				, (LPCTSTR)strKadFiles);
			return strToolTip;
		}

		CString strToolTip;
		strToolTip.Format(_T("%s: 0 | %s: 0"), (LPCTSTR)strUsersLabel, (LPCTSTR)strFilesLabel);
		return strToolTip;
	}

	/**
	 * Formats the compact eD2K/Kad connectivity summary shown in the status bar.
	 */
	inline CString FormatConnectionPaneText(const CString &strEd2kState, const CString &strKadState)
	{
		CString strPaneText;
		strPaneText.Format(_T("eD2K:%s|Kad:%s"), (LPCTSTR)strEd2kState, (LPCTSTR)strKadState);
		return strPaneText;
	}

	/**
	 * Formats the single-line tooltip text for the status bar connection pane.
	 */
	inline CString FormatConnectionPaneToolTip(const CString &strEd2kState
		, const CString &strKadState
		, const CString &strServerLabel = CString()
		, const CString &strUsersLabel = CString()
		, const CString &strServerName = CString()
		, const CString &strServerUsers = CString()
		, const CString &strServerPing = CString())
	{
		CString strToolTip(FormatConnectionPaneText(strEd2kState, strKadState));
		if (!strServerName.IsEmpty()) {
			strToolTip.AppendFormat(_T(" | %s: %s"), (LPCTSTR)strServerLabel, (LPCTSTR)strServerName);
			if (!strServerUsers.IsEmpty() || !strServerPing.IsEmpty()) {
				strToolTip.Append(_T(" ("));
				if (!strServerUsers.IsEmpty()) {
					strToolTip.AppendFormat(_T("%s: %s"), (LPCTSTR)strUsersLabel, (LPCTSTR)strServerUsers);
					if (!strServerPing.IsEmpty())
						strToolTip.Append(_T(", "));
				}
				if (!strServerPing.IsEmpty())
					strToolTip.Append(strServerPing);
				strToolTip.Append(_T(")"));
			}
		}
		return strToolTip;
	}

	/**
	 * Formats the transfer-rate pane with one compact activity summary appended to the rate text.
	 */
	inline CString FormatTransferPaneText(const CString &strTransferRate, uint32 uDownloadingFiles, uint32 uActiveUploads, uint32 uUploadSlots)
	{
		CString strPaneText(strTransferRate);
		CString strActivity;
		const uint32 uVisibleUploads = (uUploadSlots > 0 && uActiveUploads >= uUploadSlots) ? uUploadSlots : uActiveUploads;
		if (uDownloadingFiles == 0 && uVisibleUploads == 0) {
			strActivity = _T("Idle");
		} else if (uUploadSlots > 0 && uVisibleUploads < uUploadSlots) {
			strActivity.Format(_T("D:%u U:%u/%u"), uDownloadingFiles, uVisibleUploads, uUploadSlots);
		} else {
			strActivity.Format(_T("D:%u U:%u"), uDownloadingFiles, uVisibleUploads);
		}
		strPaneText.Append(_T(" | "));
		strPaneText.Append(strActivity);
		return strPaneText;
	}

	/**
	 * Formats the transfer tooltip with current activity counts for downloads, uploads, and queue pressure.
	 */
	inline CString FormatTransferPaneToolTip(const CString &strTransferRate
		, const CString &strDownloadingLabel
		, const CString &strUploadingLabel
		, const CString &strQueueLabel
		, uint32 uDownloadingFiles
		, uint32 uTotalDownloads
		, uint32 uActiveUploads
		, uint32 uUploadSlots
		, uint32 uWaitingUsers)
	{
		CString strToolTip(strTransferRate);
		const uint32 uVisibleUploads = (uUploadSlots > 0 && uActiveUploads >= uUploadSlots) ? uUploadSlots : uActiveUploads;
		strToolTip.AppendFormat(_T(" | %s: %u/%u"), (LPCTSTR)strDownloadingLabel, uDownloadingFiles, uTotalDownloads);
		if (uUploadSlots > 0 && uVisibleUploads < uUploadSlots)
			strToolTip.AppendFormat(_T(" | %s: %u/%u"), (LPCTSTR)strUploadingLabel, uVisibleUploads, uUploadSlots);
		else
			strToolTip.AppendFormat(_T(" | %s: %u"), (LPCTSTR)strUploadingLabel, uVisibleUploads);
		strToolTip.AppendFormat(_T(" | %s: %u"), (LPCTSTR)strQueueLabel, uWaitingUsers);
		return strToolTip;
	}
}
