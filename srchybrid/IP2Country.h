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

#include <map>
#include <vector>

class CImageList;
class CMmdbCountryDatabase;

struct IPRange_Struct2
{
	uint32  IPstart;
	uint32  IPend;
	CString ShortCountryName;
	CString LongCountryName;
};

#define DFLT_IP2COUNTRY_DB_FILENAME _T("dbip-country-lite.mmdb")

class CIP2Country
{
public:
	CIP2Country();
	~CIP2Country();

	void Load();
	void Unload();

	IPRange_Struct2* GetDefaultIP2Country()						{ return &m_defaultIP2Country; }
	IPRange_Struct2* GetCountryFromIP(uint32 ip);
	CString GetCountryNameFromRef(const IPRange_Struct2* pCountry) const;
	CString GetCountryCodeFromRef(const IPRange_Struct2* pCountry) const;
	int GetFlagImageIndexFromRef(const IPRange_Struct2* pCountry);
	CImageList* GetFlagImageList();
	CString GetDatabaseFilePath() const;

private:
	struct CStringLess
	{
		bool operator()(const CString& left, const CString& right) const
		{
			return left.Compare(right) < 0;
		}
	};

	static BOOL CALLBACK EnumGeoInfoProc(GEOID geoId);

	bool LoadFromFile();
	bool DownloadDatabaseFile();
	bool DownloadAndInstallDatabaseFile(const CString& strUrl, CString& strError);
	bool IsDatabaseExpired() const;
	bool IsPrivateIPv4(uint32 ip) const;
	void RemoveAllCountries();
	void LoadCountryNameMap();
	CString LookupCountryName(const CString& strCountryCode);
	IPRange_Struct2* GetOrCreateCountry(const CString& strCountryCode, uint32 dwNodeID);
	int GetFlagImageIndex(const CString& strCountryCode);

	IPRange_Struct2 m_defaultIP2Country;
	CMmdbCountryDatabase* m_pDatabase;
	__time64_t m_tBuildEpoch;
	std::vector<IPRange_Struct2*> m_countryList;
	std::map<CString, IPRange_Struct2*, CStringLess> m_countryByCode;
	std::map<CString, CString, CStringLess> m_countryNames;
	std::map<CString, int, CStringLess> m_flagIndexByCode;
	std::map<uint32, IPRange_Struct2*> m_countryByNode;
	CImageList* m_pFlagImageList;
	bool m_bCountryNamesLoaded;
};
