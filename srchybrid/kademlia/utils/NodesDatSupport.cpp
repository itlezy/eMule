#include "stdafx.h"

#include <cstdio>

#include "LongPathSeams.h"
#include "KadSupport.h"
#include "NodesDatSupport.h"

namespace
{
	const uint32 KNODESDAT_VERSION3 = 3u;
	const uint32 KNODESDAT_BOOTSTRAP_EDITION = 1u;
	const size_t KNODESDAT_HEADER_SIZE = sizeof(uint32);
	const size_t KNODESDAT_BOOTSTRAP_HEADER_SIZE = sizeof(uint32) * 3;
	const size_t KNODESDAT_V1_RECORD_SIZE = 16u + sizeof(uint32) + sizeof(uint16) + sizeof(uint16) + sizeof(uint8);
	const size_t KNODESDAT_V2_RECORD_SIZE = 16u + sizeof(uint32) + sizeof(uint16) + sizeof(uint16) + sizeof(uint8) + sizeof(uint32) + sizeof(uint32) + sizeof(uint8);

	/**
	 * @brief Reads a fixed number of bytes from a nodes.dat candidate.
	 */
	bool ReadExact(FILE *pFile, void *pBuffer, size_t uBytes)
	{
		return fread(pBuffer, 1, uBytes, pFile) == uBytes;
	}

	/**
	 * @brief Rejects obviously unusable IPv4 endpoints before touching the live routing table.
	 */
	bool IsStructurallyUsableIPv4(uint32 uHostIP, uint16 uUDPPort)
	{
		if (uUDPPort == 0 || uHostIP == 0 || uHostIP == 0xFFFFFFFFu)
			return false;

		const byte uA = static_cast<byte>((uHostIP >> 24) & 0xFF);
		const byte uB = static_cast<byte>((uHostIP >> 16) & 0xFF);

		if (uA == 0 || uA == 10 || uA == 127 || uA >= 224)
			return false;
		if (uA == 169 && uB == 254)
			return false;
		if (uA == 172 && uB >= 16 && uB <= 31)
			return false;
		if (uA == 192 && uB == 168)
			return false;

		return true;
	}

	/**
	 * @brief Converts the stored network-order IPv4 field into the host-order integer used by the client.
	 */
	uint32 NodesDatStoredIpToHostOrder(uint32 uStoredIP)
	{
		return ((uStoredIP & 0x000000FFu) << 24)
			| ((uStoredIP & 0x0000FF00u) << 8)
			| ((uStoredIP & 0x00FF0000u) >> 8)
			| ((uStoredIP & 0xFF000000u) >> 24);
	}

	/**
	 * @brief Confirms the file has enough remaining payload for the advertised record count.
	 */
	bool HasEnoughPayload(const __int64 nFileLength, const __int64 nFilePosition, uint32 uRecordCount, size_t uRecordSize)
	{
		return nFileLength >= nFilePosition && static_cast<unsigned __int64>(nFileLength - nFilePosition) >= static_cast<unsigned __int64>(uRecordCount) * uRecordSize;
	}

	bool InspectBootstrapNodesDat(FILE *pFile, const __int64 nFileLength, Kademlia::NodesDatFileInfo &rInfo)
	{
		uint32 uRecordCount = 0;
		if (!ReadExact(pFile, &uRecordCount, sizeof uRecordCount))
			return false;
		if (!HasEnoughPayload(nFileLength, _ftelli64(pFile), uRecordCount, KNODESDAT_V1_RECORD_SIZE))
			return false;

		rInfo.m_bBootstrapOnly = true;
		for (uint32 uIndex = 0; uIndex < uRecordCount; ++uIndex) {
			byte abyID[16];
			uint32 uStoredIP = 0;
			uint16 uUDPPort = 0;
			uint16 uTCPPort = 0;
			uint8 uContactVersion = 0;
			UNREFERENCED_PARAMETER(abyID);
			UNREFERENCED_PARAMETER(uTCPPort);

			if (!ReadExact(pFile, abyID, sizeof abyID)
				|| !ReadExact(pFile, &uStoredIP, sizeof uStoredIP)
				|| !ReadExact(pFile, &uUDPPort, sizeof uUDPPort)
				|| !ReadExact(pFile, &uTCPPort, sizeof uTCPPort)
				|| !ReadExact(pFile, &uContactVersion, sizeof uContactVersion))
				return false;

			const uint32 uHostIP = NodesDatStoredIpToHostOrder(uStoredIP);
			if (uContactVersion > 1 && IsStructurallyUsableIPv4(uHostIP, uUDPPort) && !(uUDPPort == 53 && uContactVersion <= 5))
				++rInfo.m_uUsableContacts;
		}
		return true;
	}
}

namespace Kademlia
{
	NodesDatFileInfo::NodesDatFileInfo()
		: m_uUsableContacts(0)
		, m_bBootstrapOnly(false)
	{
	}

	bool InspectNodesDatFile(LPCTSTR pszFilename, NodesDatFileInfo &rInfo)
	{
		rInfo = NodesDatFileInfo();

		FILE *pFile = LongPathSeams::OpenFileStreamSharedReadLongPath(pszFilename, false);
		if (pFile == NULL)
			return false;

		if (_fseeki64(pFile, 0, SEEK_END) != 0) {
			fclose(pFile);
			return false;
		}
		const __int64 nFileLength = _ftelli64(pFile);
		if (nFileLength < static_cast<__int64>(KNODESDAT_HEADER_SIZE) || _fseeki64(pFile, 0, SEEK_SET) != 0) {
			fclose(pFile);
			return false;
		}

		uint32 uNumContacts = 0;
		uint32 uVersion = 0;
		if (!ReadExact(pFile, &uNumContacts, sizeof uNumContacts)) {
			fclose(pFile);
			return false;
		}

		if (uNumContacts == 0) {
			if (nFileLength < static_cast<__int64>(sizeof(uint32) * 2) || !ReadExact(pFile, &uVersion, sizeof uVersion)) {
				fclose(pFile);
				return false;
			}

			if (uVersion == KNODESDAT_VERSION3 && nFileLength >= static_cast<__int64>(KNODESDAT_BOOTSTRAP_HEADER_SIZE)) {
				uint32 uBootstrapEdition = 0;
				if (!ReadExact(pFile, &uBootstrapEdition, sizeof uBootstrapEdition)) {
					fclose(pFile);
					return false;
				}
				if (uBootstrapEdition == KNODESDAT_BOOTSTRAP_EDITION) {
					const bool bBootstrapSuccess = InspectBootstrapNodesDat(pFile, nFileLength, rInfo);
					fclose(pFile);
					return bBootstrapSuccess;
				}
			}

			if (uVersion < 1 || uVersion > 3 || !ReadExact(pFile, &uNumContacts, sizeof uNumContacts)) {
				fclose(pFile);
				return false;
			}
		}

		const size_t uRecordSize = (uVersion >= 2) ? KNODESDAT_V2_RECORD_SIZE : KNODESDAT_V1_RECORD_SIZE;
		if (!HasEnoughPayload(nFileLength, _ftelli64(pFile), uNumContacts, uRecordSize)) {
			fclose(pFile);
			return false;
		}

		for (uint32 uIndex = 0; uIndex < uNumContacts; ++uIndex) {
			byte abyID[16];
			uint32 uStoredIP = 0;
			uint16 uUDPPort = 0;
			uint16 uTCPPort = 0;
			byte byType = 0;
			uint8 uContactVersion = 0;
			UNREFERENCED_PARAMETER(abyID);
			UNREFERENCED_PARAMETER(uTCPPort);

			if (!ReadExact(pFile, abyID, sizeof abyID)
				|| !ReadExact(pFile, &uStoredIP, sizeof uStoredIP)
				|| !ReadExact(pFile, &uUDPPort, sizeof uUDPPort)
				|| !ReadExact(pFile, &uTCPPort, sizeof uTCPPort)) {
				fclose(pFile);
				return false;
			}

			if (uVersion >= 1) {
				if (!ReadExact(pFile, &uContactVersion, sizeof uContactVersion)) {
					fclose(pFile);
					return false;
				}
			} else if (!ReadExact(pFile, &byType, sizeof byType)) {
				fclose(pFile);
				return false;
			}

			if (uVersion >= 2) {
				uint32 uUDPKey = 0;
				uint32 uUDPKeyIP = 0;
				uint8 uVerified = 0;
				if (!ReadExact(pFile, &uUDPKey, sizeof uUDPKey)
					|| !ReadExact(pFile, &uUDPKeyIP, sizeof uUDPKeyIP)
					|| !ReadExact(pFile, &uVerified, sizeof uVerified)) {
					fclose(pFile);
					return false;
				}
			}

			if (uVersion == 0 && byType >= 4)
				continue;

			const uint32 uHostIP = NodesDatStoredIpToHostOrder(uStoredIP);
			if (IsStructurallyUsableIPv4(uHostIP, uUDPPort) && !(uUDPPort == 53 && uContactVersion <= 5))
				++rInfo.m_uUsableContacts;
		}

		fclose(pFile);
		return true;
	}

	bool ReplaceNodesDatFile(LPCTSTR pszSourceFilename, LPCTSTR pszTargetFilename)
	{
		if (LongPathSeams::MoveFileEx(pszSourceFilename, pszTargetFilename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			return true;
		}

		const DWORD dwError = ::GetLastError();
		if (LongPathSeams::MoveFileEx(pszSourceFilename, pszTargetFilename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			return true;
		::SetLastError(dwError);
		return false;
	}
}
