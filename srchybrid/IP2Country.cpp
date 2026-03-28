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
#include "stdafx.h"
#include <share.h>
#include <time.h>
#include <vector>
#include <map>
#include <string>
#include <urlmon.h>
#include "IP2Country.h"
#include "emule.h"
#include "Preferences.h"
#include "GZipFile.h"
#include "log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	const uint32 MAX_MMDB_FILE_SIZE = 64u * 1024u * 1024u;
	const BYTE s_aucMetadataMarker[] = {0xAB, 0xCD, 0xEF, 'M', 'a', 'x', 'M', 'i', 'n', 'd', '.', 'c', 'o', 'm'};
	const BYTE s_aucDataSectionSeparator[16] = {0};
	const LPCTSTR DFLT_IP2COUNTRY_DB_DOWNLOAD_URL = _T("https://download.db-ip.com/free/dbip-country-lite-%04u-%02u.mmdb.gz");
	const int FLAG_ICON_WIDTH = 18;
	const int FLAG_ICON_HEIGHT = 16;
	CIP2Country* s_pGeoEnumTarget = NULL;

	enum MMDBDataType
	{
		MMDB_TYPE_UNKNOWN = 0,
		MMDB_TYPE_POINTER = 1,
		MMDB_TYPE_STRING = 2,
		MMDB_TYPE_DOUBLE = 3,
		MMDB_TYPE_BYTES = 4,
		MMDB_TYPE_UINT16 = 5,
		MMDB_TYPE_UINT32 = 6,
		MMDB_TYPE_MAP = 7,
		MMDB_TYPE_INT32 = 8,
		MMDB_TYPE_UINT64 = 9,
		MMDB_TYPE_UINT128 = 10,
		MMDB_TYPE_ARRAY = 11,
		MMDB_TYPE_CACHE_CONTAINER = 12,
		MMDB_TYPE_END_MARKER = 13,
		MMDB_TYPE_BOOLEAN = 14,
		MMDB_TYPE_FLOAT = 15
	};

	struct MMDBFieldDescriptor
	{
		MMDBDataType eType;
		uint32 dwFieldSize;
		uint32 dwOffset;

		MMDBFieldDescriptor()
			: eType(MMDB_TYPE_UNKNOWN)
			, dwFieldSize(0)
			, dwOffset(0)
		{
		}
	};

	struct MMDBValue
	{
		MMDBDataType eType;
		std::string strValue;
		ULONGLONG ullValue;
		bool bValue;
		std::map<std::string, MMDBValue> mapValue;
		std::vector<MMDBValue> vecValue;

		MMDBValue()
			: eType(MMDB_TYPE_UNKNOWN)
			, ullValue(0)
			, bValue(false)
		{
		}
	};

	bool ShiftUtcMonth(SYSTEMTIME& st, int iMonths)
	{
		int iYear = st.wYear;
		int iMonth = st.wMonth + iMonths;
		while (iMonth <= 0) {
			iMonth += 12;
			--iYear;
		}
		while (iMonth > 12) {
			iMonth -= 12;
			++iYear;
		}

		if (iYear < 1970)
			return false;

		st.wYear = static_cast<WORD>(iYear);
		st.wMonth = static_cast<WORD>(iMonth);
		return true;
	}

	CString BuildDatabaseDownloadUrl(int iMonthsBack)
	{
		SYSTEMTIME stNow = {};
		::GetSystemTime(&stNow);
		if (!ShiftUtcMonth(stNow, -iMonthsBack))
			return CString();

		CString strUrl;
		strUrl.Format(DFLT_IP2COUNTRY_DB_DOWNLOAD_URL, stNow.wYear, stNow.wMonth);
		return strUrl;
	}

	CString FormatBuildDate(__time64_t tBuildEpoch)
	{
		if (tBuildEpoch <= 0)
			return _T("unknown");

		struct tm tmBuild = {};
		if (_gmtime64_s(&tmBuild, &tBuildEpoch) != 0)
			return _T("unknown");

		CString strDate;
		strDate.Format(_T("%04u-%02u-%02u %02u:%02u UTC"),
			static_cast<unsigned>(tmBuild.tm_year + 1900),
			static_cast<unsigned>(tmBuild.tm_mon + 1),
			static_cast<unsigned>(tmBuild.tm_mday),
			static_cast<unsigned>(tmBuild.tm_hour),
			static_cast<unsigned>(tmBuild.tm_min));
		return strDate;
	}

	bool DownloadUrlToFile(const CString& strUrl, const CString& strTargetPath, CString& strError)
	{
		const HRESULT hr = ::URLDownloadToFile(NULL, strUrl, strTargetPath, 0, NULL);
		if (FAILED(hr)) {
			strError.Format(_T("URLDownloadToFile failed (0x%08X)"), hr);
			(void)::DeleteFile(strTargetPath);
			return false;
		}

		return true;
	}

	bool IsRetryableDatabaseDownloadError(const CString& strError)
	{
		return (strError.Left(24) == _T("URLDownloadToFile failed"));
	}
}

class CMmdbCountryDatabase
{
public:
		CMmdbCountryDatabase()
			: m_uIPVersion(0)
			, m_uRecordSize(0)
			, m_dwNodeCount(0)
			, m_dwNodeSize(0)
			, m_dwIndexSize(0)
			, m_tBuildEpoch(0)
		{
		}

		bool Load(const CString& strFilePath, CString& strError)
		{
			m_vecData.clear();
			m_mapCountryCodeByNode.clear();
			m_tBuildEpoch = 0;
			m_strDatabaseType.Empty();

			CFile file;
			if (!file.Open(strFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary)) {
				strError.Format(_T("Data file not found: %s"), (LPCTSTR)strFilePath);
				return false;
			}

			const ULONGLONG ullSize = file.GetLength();
			if (ullSize == 0 || ullSize > MAX_MMDB_FILE_SIZE) {
				strError.Format(_T("Unsupported database file size: %I64u"), ullSize);
				file.Close();
				return false;
			}

			m_vecData.resize(static_cast<size_t>(ullSize));
			if (file.Read(&m_vecData[0], static_cast<UINT>(m_vecData.size())) != static_cast<UINT>(m_vecData.size())) {
				strError = _T("Could not read database file.");
				file.Close();
				m_vecData.clear();
				return false;
			}
			file.Close();

			const MMDBValue metadata = ReadMetadata();
			if (!ParseMetadata(metadata, strError))
				return false;

			if (!LoadIndex(strError))
				return false;

			return true;
		}

		CString GetType() const
		{
			return m_strDatabaseType;
		}

		__time64_t GetBuildEpoch() const
		{
			return m_tBuildEpoch;
		}

		bool LookupCountryCode(uint32 dwIP, CString& strCountryCode, uint32& dwNodeID) const
		{
			strCountryCode.Empty();
			dwNodeID = 0;
			if (m_vecData.empty())
				return false;

			BYTE aucAddress[16] = {0};
			aucAddress[10] = 0xFF;
			aucAddress[11] = 0xFF;
			aucAddress[12] = static_cast<BYTE>(dwIP & 0xFF);
			aucAddress[13] = static_cast<BYTE>((dwIP >> 8) & 0xFF);
			aucAddress[14] = static_cast<BYTE>((dwIP >> 16) & 0xFF);
			aucAddress[15] = static_cast<BYTE>((dwIP >> 24) & 0xFF);

			uint32 dwNode = 0;
			for (int iByte = 0; iByte < _countof(aucAddress); ++iByte) {
				for (int iBit = 0; iBit < 8; ++iBit) {
					const bool bRight = (aucAddress[iByte] & (0x80 >> iBit)) != 0;
					const uint32 dwRecord = ReadNodeRecord(dwNode, bRight ? 1 : 0);
					if (dwRecord == m_dwNodeCount)
						return false;
					if (dwRecord > m_dwNodeCount) {
						strCountryCode = LookupCountryCodeByNode(dwRecord);
						dwNodeID = dwRecord;
						return !strCountryCode.IsEmpty();
					}
					dwNode = dwRecord;
				}
			}

			return false;
		}

	private:
		bool ParseMetadata(const MMDBValue& value, CString& strError)
		{
			if (value.eType != MMDB_TYPE_MAP) {
				strError = _T("Metadata error: metadata map not found.");
				return false;
			}

			ULONGLONG ullValue = 0;
			if (!GetMapUnsigned(value, "binary_format_major_version", ullValue) || ullValue != 2) {
				strError = _T("Unsupported database version.");
				return false;
			}

			if (!GetMapUnsigned(value, "ip_version", ullValue)) {
				strError = _T("Metadata error: ip_version missing.");
				return false;
			}
			m_uIPVersion = static_cast<uint16>(ullValue);
			if (m_uIPVersion != 6) {
				strError.Format(_T("Unsupported IP version: %u"), m_uIPVersion);
				return false;
			}

			if (!GetMapUnsigned(value, "record_size", ullValue)) {
				strError = _T("Metadata error: record_size missing.");
				return false;
			}
			m_uRecordSize = static_cast<uint16>(ullValue);
			if (m_uRecordSize != 24 && m_uRecordSize != 28 && m_uRecordSize != 32) {
				strError.Format(_T("Unsupported record size: %u"), m_uRecordSize);
				return false;
			}
			m_dwNodeSize = (m_uRecordSize == 24) ? 6u : ((m_uRecordSize == 28) ? 7u : 8u);

			if (!GetMapUnsigned(value, "node_count", ullValue)) {
				strError = _T("Metadata error: node_count missing.");
				return false;
			}
			m_dwNodeCount = static_cast<uint32>(ullValue);
			m_dwIndexSize = m_dwNodeCount * m_dwNodeSize;

			std::string strDatabaseType;
			if (!GetMapString(value, "database_type", strDatabaseType)) {
				strError = _T("Metadata error: database_type missing.");
				return false;
			}
			m_strDatabaseType = CString(strDatabaseType.c_str());

			if (!GetMapUnsigned(value, "build_epoch", ullValue)) {
				strError = _T("Metadata error: build_epoch missing.");
				return false;
			}
			m_tBuildEpoch = static_cast<__time64_t>(ullValue);
			return true;
		}

		bool LoadIndex(CString& strError) const
		{
			if (m_vecData.size() < (m_dwIndexSize + sizeof(s_aucDataSectionSeparator))) {
				strError = _T("Database corrupted: index is truncated.");
				return false;
			}
			if (memcmp(&m_vecData[m_dwIndexSize], s_aucDataSectionSeparator, sizeof(s_aucDataSectionSeparator)) != 0) {
				strError = _T("Database corrupted: no data section found.");
				return false;
			}
			return true;
		}

		MMDBValue ReadMetadata() const
		{
			if (m_vecData.size() < sizeof(s_aucMetadataMarker))
				return MMDBValue();

			size_t uOffset = m_vecData.size() - sizeof(s_aucMetadataMarker);
			for (;;) {
				if (memcmp(&m_vecData[uOffset], s_aucMetadataMarker, sizeof(s_aucMetadataMarker)) == 0) {
					uint32 dwDataOffset = static_cast<uint32>(uOffset + sizeof(s_aucMetadataMarker));
					return ReadDataField(dwDataOffset);
				}
				if (uOffset == 0)
					break;
				--uOffset;
			}
			return MMDBValue();
		}

		MMDBValue ReadDataField(uint32& dwOffset) const
		{
			MMDBFieldDescriptor descriptor;
			if (!ReadDataFieldDescriptor(dwOffset, descriptor))
				return MMDBValue();

			uint32 dwLocalOffset = dwOffset;
			bool bPointer = false;
			if (descriptor.eType == MMDB_TYPE_POINTER) {
				bPointer = true;
				dwLocalOffset = descriptor.dwOffset + m_dwIndexSize + sizeof(s_aucDataSectionSeparator);
				if (!ReadDataFieldDescriptor(dwLocalOffset, descriptor))
					return MMDBValue();
			}

			MMDBValue value;
			value.eType = descriptor.eType;
			switch (descriptor.eType) {
			case MMDB_TYPE_STRING:
				if (!CanRead(dwLocalOffset, descriptor.dwFieldSize))
					return MMDBValue();
				value.strValue.assign(reinterpret_cast<const char*>(&m_vecData[dwLocalOffset]), descriptor.dwFieldSize);
				dwLocalOffset += descriptor.dwFieldSize;
				break;
			case MMDB_TYPE_UINT16:
			case MMDB_TYPE_UINT32:
			case MMDB_TYPE_INT32:
			case MMDB_TYPE_UINT64:
			case MMDB_TYPE_UINT128:
				if (!ReadUnsignedInteger(dwLocalOffset, descriptor.dwFieldSize, value.ullValue))
					return MMDBValue();
				break;
			case MMDB_TYPE_MAP:
				if (!ReadMapValue(dwLocalOffset, descriptor.dwFieldSize, value))
					return MMDBValue();
				break;
			case MMDB_TYPE_ARRAY:
				if (!ReadArrayValue(dwLocalOffset, descriptor.dwFieldSize, value))
					return MMDBValue();
				break;
			case MMDB_TYPE_BOOLEAN:
				value.bValue = descriptor.dwFieldSize != 0;
				break;
			case MMDB_TYPE_BYTES:
			case MMDB_TYPE_DOUBLE:
			case MMDB_TYPE_FLOAT:
				if (!CanRead(dwLocalOffset, descriptor.dwFieldSize))
					return MMDBValue();
				dwLocalOffset += descriptor.dwFieldSize;
				break;
			default:
				return MMDBValue();
			}

			if (!bPointer)
				dwOffset = dwLocalOffset;
			return value;
		}

		bool ReadDataFieldDescriptor(uint32& dwOffset, MMDBFieldDescriptor& descriptor) const
		{
			if (!CanRead(dwOffset, 1))
				return false;

			const BYTE* pucData = &m_vecData[dwOffset];
			const uint32 dwAvailable = static_cast<uint32>(m_vecData.size() - dwOffset);

			descriptor.eType = static_cast<MMDBDataType>((pucData[0] & 0xE0) >> 5);
			if (descriptor.eType == MMDB_TYPE_POINTER) {
				const int iSize = (pucData[0] & 0x18) >> 3;
				if (dwAvailable < static_cast<uint32>(iSize + 2))
					return false;

				if (iSize == 0)
					descriptor.dwOffset = ((pucData[0] & 0x07) << 8) + pucData[1];
				else if (iSize == 1)
					descriptor.dwOffset = ((pucData[0] & 0x07) << 16) + (pucData[1] << 8) + pucData[2] + 2048;
				else if (iSize == 2)
					descriptor.dwOffset = ((pucData[0] & 0x07) << 24) + (pucData[1] << 16) + (pucData[2] << 8) + pucData[3] + 526336;
				else
					descriptor.dwOffset = (pucData[1] << 24) + (pucData[2] << 16) + (pucData[3] << 8) + pucData[4];

				dwOffset += iSize + 2;
				return true;
			}

			descriptor.dwFieldSize = pucData[0] & 0x1F;
			if (descriptor.dwFieldSize <= 28) {
				if (descriptor.eType == MMDB_TYPE_UNKNOWN) {
					if (dwAvailable < 2)
						return false;
					descriptor.eType = static_cast<MMDBDataType>(pucData[1] + 7);
					dwOffset += 2;
				} else {
					dwOffset += 1;
				}
			} else if (descriptor.dwFieldSize == 29) {
				if (dwAvailable < 2)
					return false;
				descriptor.dwFieldSize = pucData[1] + 29;
				dwOffset += 2;
			} else if (descriptor.dwFieldSize == 30) {
				if (dwAvailable < 3)
					return false;
				descriptor.dwFieldSize = (pucData[1] << 8) + pucData[2] + 285;
				dwOffset += 3;
			} else {
				if (dwAvailable < 4)
					return false;
				descriptor.dwFieldSize = (pucData[1] << 16) + (pucData[2] << 8) + pucData[3] + 65821;
				dwOffset += 4;
			}

			return true;
		}

		bool ReadMapValue(uint32& dwOffset, uint32 dwCount, MMDBValue& value) const
		{
			value.eType = MMDB_TYPE_MAP;
			for (uint32 i = 0; i < dwCount; ++i) {
				const MMDBValue key = ReadDataField(dwOffset);
				if (key.eType != MMDB_TYPE_STRING)
					return false;

				const MMDBValue fieldValue = ReadDataField(dwOffset);
				if (fieldValue.eType == MMDB_TYPE_UNKNOWN)
					return false;

				value.mapValue[key.strValue] = fieldValue;
			}
			return true;
		}

		bool ReadArrayValue(uint32& dwOffset, uint32 dwCount, MMDBValue& value) const
		{
			value.eType = MMDB_TYPE_ARRAY;
			for (uint32 i = 0; i < dwCount; ++i) {
				const MMDBValue fieldValue = ReadDataField(dwOffset);
				if (fieldValue.eType == MMDB_TYPE_UNKNOWN)
					return false;
				value.vecValue.push_back(fieldValue);
			}
			return true;
		}

		bool ReadUnsignedInteger(uint32& dwOffset, uint32 dwLength, ULONGLONG& ullValue) const
		{
			if (dwLength > 8 || !CanRead(dwOffset, dwLength))
				return false;

			ullValue = 0;
			for (uint32 i = 0; i < dwLength; ++i)
				ullValue = (ullValue << 8) | m_vecData[dwOffset + i];
			dwOffset += dwLength;
			return true;
		}

		bool CanRead(uint32 dwOffset, uint32 dwLength) const
		{
			return (dwOffset <= m_vecData.size()) && (dwLength <= (m_vecData.size() - dwOffset));
		}

		uint32 ReadNodeRecord(uint32 dwNode, int iBranch) const
		{
			const uint32 dwOffset = dwNode * m_dwNodeSize;
			if (!CanRead(dwOffset, m_dwNodeSize))
				return m_dwNodeCount;

			const BYTE* p = &m_vecData[dwOffset];
			if (m_uRecordSize == 24) {
				const int iShift = iBranch * 3;
				return (static_cast<uint32>(p[iShift]) << 16)
					| (static_cast<uint32>(p[iShift + 1]) << 8)
					| static_cast<uint32>(p[iShift + 2]);
			}

			if (m_uRecordSize == 28) {
				if (iBranch == 0) {
					return (static_cast<uint32>(p[0]) << 20)
						| (static_cast<uint32>(p[1]) << 12)
						| (static_cast<uint32>(p[2]) << 4)
						| (static_cast<uint32>(p[3]) >> 4);
				}
				return ((static_cast<uint32>(p[3]) & 0x0F) << 24)
					| (static_cast<uint32>(p[4]) << 16)
					| (static_cast<uint32>(p[5]) << 8)
					| static_cast<uint32>(p[6]);
			}

			const int iShift = iBranch * 4;
			return (static_cast<uint32>(p[iShift]) << 24)
				| (static_cast<uint32>(p[iShift + 1]) << 16)
				| (static_cast<uint32>(p[iShift + 2]) << 8)
				| static_cast<uint32>(p[iShift + 3]);
		}

		CString LookupCountryCodeByNode(uint32 dwNodeID) const
		{
			std::map<uint32, std::string>::const_iterator itFound = m_mapCountryCodeByNode.find(dwNodeID);
			if (itFound != m_mapCountryCodeByNode.end())
				return CString(itFound->second.c_str());

			if (dwNodeID <= m_dwNodeCount)
				return CString();

			uint32 dwOffset = m_dwIndexSize + sizeof(s_aucDataSectionSeparator) + (dwNodeID - m_dwNodeCount - sizeof(s_aucDataSectionSeparator));
			const MMDBValue value = ReadDataField(dwOffset);
			if (value.eType != MMDB_TYPE_MAP)
				return CString();

			std::string strCountryCode;
			if (!ExtractCountryCode(value, "country", strCountryCode)
				&& !ExtractCountryCode(value, "registered_country", strCountryCode))
			{
				return CString();
			}

			m_mapCountryCodeByNode[dwNodeID] = strCountryCode;
			return CString(strCountryCode.c_str());
		}

		static bool GetMapUnsigned(const MMDBValue& value, const char* pszKey, ULONGLONG& ullValue)
		{
			std::map<std::string, MMDBValue>::const_iterator itFound = value.mapValue.find(pszKey);
			if (itFound == value.mapValue.end())
				return false;
			if (itFound->second.eType != MMDB_TYPE_UINT16 && itFound->second.eType != MMDB_TYPE_UINT32
				&& itFound->second.eType != MMDB_TYPE_UINT64 && itFound->second.eType != MMDB_TYPE_INT32
				&& itFound->second.eType != MMDB_TYPE_UINT128)
			{
				return false;
			}
			ullValue = itFound->second.ullValue;
			return true;
		}

		static bool GetMapString(const MMDBValue& value, const char* pszKey, std::string& strValue)
		{
			std::map<std::string, MMDBValue>::const_iterator itFound = value.mapValue.find(pszKey);
			if (itFound == value.mapValue.end() || itFound->second.eType != MMDB_TYPE_STRING)
				return false;
			strValue = itFound->second.strValue;
			return true;
		}

		static bool ExtractCountryCode(const MMDBValue& value, const char* pszCountryKey, std::string& strCountryCode)
		{
			std::map<std::string, MMDBValue>::const_iterator itCountry = value.mapValue.find(pszCountryKey);
			if (itCountry == value.mapValue.end() || itCountry->second.eType != MMDB_TYPE_MAP)
				return false;

			std::map<std::string, MMDBValue>::const_iterator itIsoCode = itCountry->second.mapValue.find("iso_code");
			if (itIsoCode == itCountry->second.mapValue.end() || itIsoCode->second.eType != MMDB_TYPE_STRING)
				return false;

			strCountryCode = itIsoCode->second.strValue;
			return !strCountryCode.empty();
		}

	mutable std::map<uint32, std::string> m_mapCountryCodeByNode;
	std::vector<BYTE> m_vecData;
	uint16 m_uIPVersion;
	uint16 m_uRecordSize;
	uint32 m_dwNodeCount;
	uint32 m_dwNodeSize;
	uint32 m_dwIndexSize;
	__time64_t m_tBuildEpoch;
	CString m_strDatabaseType;
};

CIP2Country::CIP2Country()
	: m_pDatabase(NULL)
	, m_tBuildEpoch(0)
	, m_pFlagImageList(NULL)
	, m_bCountryNamesLoaded(false)
{
	m_defaultIP2Country.IPstart = 0;
	m_defaultIP2Country.IPend = 0;
	m_defaultIP2Country.ShortCountryName = _T("N/A");
	m_defaultIP2Country.LongCountryName = _T("N/A");

	Load();
}

CIP2Country::~CIP2Country()
{
	Unload();
	if (m_pFlagImageList != NULL) {
		m_pFlagImageList->DeleteImageList();
		delete m_pFlagImageList;
		m_pFlagImageList = NULL;
	}
}

void CIP2Country::Load()
{
	Unload();

	bool bLoaded = LoadFromFile();
	if (bLoaded && IsDatabaseExpired()) {
		if (DownloadDatabaseFile()) {
			Unload();
			bLoaded = LoadFromFile();
		}
	} else if (!bLoaded) {
		if (DownloadDatabaseFile())
			(void)LoadFromFile();
	}
}

void CIP2Country::Unload()
{
	delete m_pDatabase;
	m_pDatabase = NULL;
	m_tBuildEpoch = 0;
	RemoveAllCountries();
}

bool CIP2Country::LoadFromFile()
{
	const CString strDatabaseFilePath = GetDatabaseFilePath();
	CString strError;

	CMmdbCountryDatabase* pDatabase = new CMmdbCountryDatabase;
	if (!pDatabase->Load(strDatabaseFilePath, strError)) {
		delete pDatabase;
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("IP2Country: %s"), (LPCTSTR)strError);
		return false;
	}

	m_pDatabase = pDatabase;
	m_tBuildEpoch = m_pDatabase->GetBuildEpoch();
	AddLogLine(false, _T("IP2Country: Loaded %s (%s)."), (LPCTSTR)m_pDatabase->GetType(), (LPCTSTR)FormatBuildDate(m_tBuildEpoch));
	return true;
}

bool CIP2Country::DownloadDatabaseFile()
{
	CString strLastError;
	CString strReportedError;
	for (int iAttempt = 0; iAttempt < 12; ++iAttempt) {
		const CString strUrl = BuildDatabaseDownloadUrl(iAttempt);
		if (strUrl.IsEmpty())
			continue;
		CString strAttemptError;
		if (DownloadAndInstallDatabaseFile(strUrl, strAttemptError))
			return true;

		strLastError = strAttemptError;
		if (strReportedError.IsEmpty() || (IsRetryableDatabaseDownloadError(strReportedError) && !IsRetryableDatabaseDownloadError(strAttemptError)))
			strReportedError = strAttemptError;
		if (!IsRetryableDatabaseDownloadError(strAttemptError))
			break;
	}

	if (!strReportedError.IsEmpty())
		AddLogLine(false, _T("IP2Country: Could not update DB-IP database (%s)."), (LPCTSTR)strReportedError);
	else if (!strLastError.IsEmpty())
		AddLogLine(false, _T("IP2Country: Could not update DB-IP database (%s)."), (LPCTSTR)strLastError);
	else
		AddLogLine(false, _T("IP2Country: Could not update DB-IP database."));
	return false;
}

bool CIP2Country::DownloadAndInstallDatabaseFile(const CString& strUrl, CString& strError)
{
	strError.Empty();
	const CString strConfigDir = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR);
	TCHAR szTempArchive[MAX_PATH] = {};
	TCHAR szTempDatabase[MAX_PATH] = {};
	if (!::GetTempFileName(strConfigDir, _T("gip"), 0, szTempArchive)) {
		strError.Format(_T("GetTempFileName failed for %s (%u)"), (LPCTSTR)strConfigDir, ::GetLastError());
		return false;
	}
	if (!::GetTempFileName(strConfigDir, _T("gid"), 0, szTempDatabase)) {
		strError.Format(_T("GetTempFileName failed for %s (%u)"), (LPCTSTR)strConfigDir, ::GetLastError());
		return false;
	}

	CString strArchivePath = szTempArchive;
	CString strExtractedPath = szTempDatabase;
	(void)::DeleteFile(strArchivePath);
	strArchivePath += _T(".gz");

	if (!DownloadUrlToFile(strUrl, strArchivePath, strError)) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("IP2Country: Download failed from %s: %s"), (LPCTSTR)strUrl, (LPCTSTR)strError);
		(void)::DeleteFile(strArchivePath);
		(void)::DeleteFile(strExtractedPath);
		return false;
	}

	CGZIPFile gzipFile;
	if (!gzipFile.Open(strArchivePath) || !gzipFile.Extract(strExtractedPath)) {
		strError.Format(_T("Failed to unpack %s"), (LPCTSTR)strArchivePath);
		AddDebugLogLine(false, _T("IP2Country: %s"), (LPCTSTR)strError);
		gzipFile.Close();
		(void)::DeleteFile(strArchivePath);
		(void)::DeleteFile(strExtractedPath);
		return false;
	}
	gzipFile.Close();

	CString strLoadError;
	CMmdbCountryDatabase dbValidator;
	if (!dbValidator.Load(strExtractedPath, strLoadError)) {
		strError.Format(_T("Downloaded database rejected: %s"), (LPCTSTR)strLoadError);
		AddDebugLogLine(false, _T("IP2Country: %s"), (LPCTSTR)strError);
		(void)::DeleteFile(strArchivePath);
		(void)::DeleteFile(strExtractedPath);
		return false;
	}

	const CString strDatabaseFilePath = GetDatabaseFilePath();
	if (!::MoveFileEx(strExtractedPath, strDatabaseFilePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		strError.Format(_T("Failed to install %s (%u)"), (LPCTSTR)strDatabaseFilePath, ::GetLastError());
		AddDebugLogLine(false, _T("IP2Country: %s"), (LPCTSTR)strError);
		(void)::DeleteFile(strArchivePath);
		(void)::DeleteFile(strExtractedPath);
		return false;
	}

	(void)::DeleteFile(strArchivePath);
	AddLogLine(false, _T("IP2Country: Updated %s (%s)."), (LPCTSTR)dbValidator.GetType(), (LPCTSTR)FormatBuildDate(dbValidator.GetBuildEpoch()));
	return true;
}

bool CIP2Country::IsDatabaseExpired() const
{
	if (m_tBuildEpoch <= 0)
		return true;

	__time64_t tNow = 0;
	_time64(&tNow);

	struct tm tmCurrent = {};
	struct tm tmBuild = {};
	if (_gmtime64_s(&tmCurrent, &tNow) != 0 || _gmtime64_s(&tmBuild, &m_tBuildEpoch) != 0)
		return false;

	if (tmCurrent.tm_mday <= 1)
		return false;

	return (tmCurrent.tm_year != tmBuild.tm_year) || (tmCurrent.tm_mon != tmBuild.tm_mon);
}

bool CIP2Country::IsPrivateIPv4(uint32 ip) const
{
	const BYTE b1 = static_cast<BYTE>(ip & 0xFF);
	const BYTE b2 = static_cast<BYTE>((ip >> 8) & 0xFF);

	if (ip == 0)
		return true;
	if (b1 == 10 || b1 == 127)
		return true;
	if (b1 == 169 && b2 == 254)
		return true;
	if (b1 == 172 && b2 >= 16 && b2 <= 31)
		return true;
	if (b1 == 192 && b2 == 168)
		return true;
	if (b1 >= 224)
		return true;
	return false;
}

void CIP2Country::RemoveAllCountries()
{
	for (size_t i = 0; i < m_countryList.size(); ++i)
		delete m_countryList[i];
	m_countryList.clear();
	m_countryByCode.clear();
	m_countryByNode.clear();
}

void CIP2Country::LoadCountryNameMap()
{
	if (m_bCountryNamesLoaded)
		return;

	s_pGeoEnumTarget = this;
	(void)::EnumSystemGeoID(GEOCLASS_NATION, 0, EnumGeoInfoProc);
	s_pGeoEnumTarget = NULL;

	m_countryNames[_T("A1")] = _T("Anonymous Proxy");
	m_countryNames[_T("A2")] = _T("Satellite Provider");
	m_countryNames[_T("EU")] = _T("European Union");
	m_countryNames[_T("XK")] = _T("Kosovo");
	m_bCountryNamesLoaded = true;
}

CString CIP2Country::LookupCountryName(const CString& strCountryCode)
{
	if (strCountryCode.IsEmpty())
		return m_defaultIP2Country.LongCountryName;

	LoadCountryNameMap();
	std::map<CString, CString, CStringLess>::const_iterator itFound = m_countryNames.find(strCountryCode);
	if (itFound != m_countryNames.end() && !itFound->second.IsEmpty())
		return itFound->second;
	return strCountryCode;
}

IPRange_Struct2* CIP2Country::GetOrCreateCountry(const CString& strCountryCode, uint32 dwNodeID)
{
	std::map<CString, IPRange_Struct2*, CStringLess>::const_iterator itFound = m_countryByCode.find(strCountryCode);
	if (itFound != m_countryByCode.end()) {
		m_countryByNode[dwNodeID] = itFound->second;
		return itFound->second;
	}

	IPRange_Struct2* pCountry = new IPRange_Struct2;
	pCountry->IPstart = 0;
	pCountry->IPend = 0;
	pCountry->ShortCountryName = strCountryCode;
	pCountry->LongCountryName = LookupCountryName(strCountryCode);

	m_countryList.push_back(pCountry);
	m_countryByCode[strCountryCode] = pCountry;
	m_countryByNode[dwNodeID] = pCountry;
	return pCountry;
}

IPRange_Struct2* CIP2Country::GetCountryFromIP(uint32 ip)
{
	if (m_pDatabase == NULL || IsPrivateIPv4(ip))
		return &m_defaultIP2Country;

	CString strCountryCode;
	uint32 dwNodeID = 0;
	if (!m_pDatabase->LookupCountryCode(ip, strCountryCode, dwNodeID))
		return &m_defaultIP2Country;

	strCountryCode.MakeUpper();
	if (strCountryCode.IsEmpty())
		return &m_defaultIP2Country;

	std::map<uint32, IPRange_Struct2*>::const_iterator itNode = m_countryByNode.find(dwNodeID);
	if (itNode != m_countryByNode.end())
		return itNode->second;

	return GetOrCreateCountry(strCountryCode, dwNodeID);
}

CString CIP2Country::GetCountryNameFromRef(const IPRange_Struct2* pCountry) const
{
	return (pCountry != NULL && !pCountry->LongCountryName.IsEmpty()) ? pCountry->LongCountryName : m_defaultIP2Country.LongCountryName;
}

CString CIP2Country::GetCountryCodeFromRef(const IPRange_Struct2* pCountry) const
{
	return (pCountry != NULL) ? pCountry->ShortCountryName : CString();
}

int CIP2Country::GetFlagImageIndexFromRef(const IPRange_Struct2* pCountry)
{
	return GetFlagImageIndex(GetCountryCodeFromRef(pCountry));
}

CImageList* CIP2Country::GetFlagImageList()
{
	if (m_pFlagImageList == NULL) {
		m_pFlagImageList = new CImageList;
		if (!m_pFlagImageList->Create(FLAG_ICON_WIDTH, FLAG_ICON_HEIGHT, ILC_COLOR32 | ILC_MASK, 0, 32)) {
			delete m_pFlagImageList;
			m_pFlagImageList = NULL;
		}
	}
	return m_pFlagImageList;
}

CString CIP2Country::GetDatabaseFilePath() const
{
	return thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + DFLT_IP2COUNTRY_DB_FILENAME;
}

int CIP2Country::GetFlagImageIndex(const CString& strCountryCode)
{
	CString strLookupCode = strCountryCode;
	strLookupCode.Trim();
	strLookupCode.MakeUpper();
	if (strLookupCode.IsEmpty() || strLookupCode == _T("N/A"))
		return -1;

	std::map<CString, int, CStringLess>::const_iterator itFound = m_flagIndexByCode.find(strLookupCode);
	if (itFound != m_flagIndexByCode.end())
		return itFound->second;

	CImageList* pFlagImageList = GetFlagImageList();
	if (pFlagImageList == NULL)
		return -1;

	const CString strResourceName = _T("FLAG_") + strLookupCode;
	HICON hIcon = reinterpret_cast<HICON>(::LoadImage(AfxGetResourceHandle(), strResourceName, IMAGE_ICON, FLAG_ICON_WIDTH, FLAG_ICON_HEIGHT, LR_DEFAULTCOLOR));
	int iImage = -1;
	if (hIcon != NULL) {
		iImage = pFlagImageList->Add(hIcon);
		::DestroyIcon(hIcon);
	}

	m_flagIndexByCode[strLookupCode] = iImage;
	return iImage;
}

BOOL CALLBACK CIP2Country::EnumGeoInfoProc(GEOID geoId)
{
	if (s_pGeoEnumTarget == NULL)
		return FALSE;

	TCHAR szCountryCode[8] = {};
	TCHAR szCountryName[128] = {};
	if (::GetGeoInfo(geoId, GEO_ISO2, szCountryCode, _countof(szCountryCode), 0) > 0
		&& ::GetGeoInfo(geoId, GEO_FRIENDLYNAME, szCountryName, _countof(szCountryName), 0) > 0)
	{
		CString strCountryCode = szCountryCode;
		strCountryCode.MakeUpper();
		s_pGeoEnumTarget->m_countryNames[strCountryCode] = szCountryName;
	}
	return TRUE;
}
