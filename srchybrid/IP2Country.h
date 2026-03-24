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

#include <atlcoll.h>

struct IPRange_Struct2
{
	uint32  IPstart;
	uint32  IPend;
	CString ShortCountryName;
	CString LongCountryName;
};

#define DFLT_IP2COUNTRY_FILENAME_OLD _T("ip-to-country.csv")
#define DFLT_IP2COUNTRY_FILENAME     _T("GeoIPCountryWhois.csv")

typedef CTypedPtrArray<CPtrArray, IPRange_Struct2*> CIP2CountryArray;

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
	CString GetDefaultFilePath(bool& bUsingNewFile) const;

private:
	bool LoadFromFile();
	void RemoveAllIPs();
	void AddIPRange(uint32 ipFrom, uint32 ipTo, const CString& shortCountryName, const CString& longCountryName);

	IPRange_Struct2 m_defaultIP2Country;
	CIP2CountryArray m_iplist;
};
