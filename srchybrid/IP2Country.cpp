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
#include "IP2Country.h"
#include "emule.h"
#include "otherfunctions.h"
#include "Preferences.h"
#include "log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	void FirstCharCap(CString& strTarget)
	{
		strTarget.TrimRight();
		if (strTarget.IsEmpty())
			return;

		strTarget.MakeLower();
		for (int iIdx = 0;;) {
			strTarget.SetAt(iIdx, strTarget.Mid(iIdx, 1).MakeUpper().GetAt(0));
			iIdx = strTarget.Find(_T(' '), iIdx) + 1;
			if (iIdx == 0)
				break;
		}
	}

	int __cdecl CmpIP2CountryByStartAddr(const void* p1, const void* p2)
	{
		const IPRange_Struct2* rng1 = *(IPRange_Struct2**)p1;
		const IPRange_Struct2* rng2 = *(IPRange_Struct2**)p2;
		return CompareUnsigned(rng1->IPstart, rng2->IPstart);
	}

	int __cdecl CmpIP2CountryByAddr(const void* pvKey, const void* pvElement)
	{
		const uint32 ip = *(uint32*)pvKey;
		const IPRange_Struct2* pIP2Country = *(IPRange_Struct2**)pvElement;

		if (ip < pIP2Country->IPstart)
			return -1;
		if (ip > pIP2Country->IPend)
			return 1;
		return 0;
	}
}

CIP2Country::CIP2Country()
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
}

void CIP2Country::Load()
{
	LoadFromFile();
}

void CIP2Country::Unload()
{
	RemoveAllIPs();
}

bool CIP2Country::LoadFromFile()
{
	DWORD dwStartTick = ::GetTickCount();
	bool bUsingNewFile = false;
	const CString ip2countryCSVfile = GetDefaultFilePath(bUsingNewFile);
	FILE* readFile = _tfsopen(ip2countryCSVfile, _T("rt"), _SH_DENYWR);
	if (readFile == NULL) {
		if (thePrefs.GetVerbose())
			AddDebugLogLine(false, _T("IP2Country: Data file not found: %s"), (LPCTSTR)ip2countryCSVfile);
		return false;
	}

	TCHAR szbuffer[520];
	int iCount = 0;
	int iLine = 0;
	int iDuplicate = 0;
	int iMerged = 0;

	while (!feof(readFile)) {
		if (_fgetts(szbuffer, _countof(szbuffer), readFile) == NULL)
			break;

		++iLine;
		CString sbuffer = szbuffer;
		sbuffer.Remove(L'"');

		if (bUsingNewFile) {
			// GeoIPCountryWhois.csv:
			// "1.0.0.0","1.0.0.255","16777216","16777471","AU","Australia"
			CString fields[6];
			int curPos = 0;
			bool bInvalidLine = false;
			for (int i = 0; i < _countof(fields); ++i) {
				fields[i] = sbuffer.Tokenize(_T(","), curPos);
				if (fields[i].IsEmpty() && i < 5) {
					bInvalidLine = true;
					break;
				}
			}

			if (bInvalidLine)
				continue;

			FirstCharCap(fields[5]);
			AddIPRange(_tcstoul(fields[2], NULL, 10), _tcstoul(fields[3], NULL, 10), fields[4], fields[5]);
			++iCount;
		} else {
			// ip-to-country.csv:
			// 0033996344,0033996351,GB,GBR,"UNITED KINGDOM"
			CString fields[5];
			int curPos = 0;
			bool bInvalidLine = false;
			for (int i = 0; i < _countof(fields); ++i) {
				fields[i] = sbuffer.Tokenize(_T(","), curPos);
				if (fields[i].IsEmpty() && i < 4) {
					bInvalidLine = true;
					break;
				}
			}

			if (bInvalidLine)
				continue;

			FirstCharCap(fields[4]);
			AddIPRange(_tcstoul(fields[0], NULL, 10), _tcstoul(fields[1], NULL, 10), fields[2], fields[4]);
			++iCount;
		}
	}

	fclose(readFile);

	qsort(m_iplist.GetData(), m_iplist.GetCount(), sizeof(m_iplist[0]), CmpIP2CountryByStartAddr);
	if (m_iplist.GetCount() >= 2) {
		IPRange_Struct2* pPrv = m_iplist[0];
		int i = 1;
		while (i < m_iplist.GetCount()) {
			IPRange_Struct2* pCur = m_iplist[i];
			const bool bOverlapping = pCur->IPstart >= pPrv->IPstart && pCur->IPstart <= pPrv->IPend;
			const bool bAdjacentSameCountry = pCur->IPstart == pPrv->IPend + 1
				&& pCur->ShortCountryName == pPrv->ShortCountryName
				&& pCur->LongCountryName == pPrv->LongCountryName;
			if (bOverlapping || bAdjacentSameCountry) {
				if (pCur->IPstart != pPrv->IPstart || pCur->IPend != pPrv->IPend) {
					if (pCur->IPend > pPrv->IPend)
						pPrv->IPend = pCur->IPend;
					++iMerged;
				} else {
					++iDuplicate;
				}

				delete pCur;
				m_iplist.RemoveAt(i);
				continue;
			}

			pPrv = pCur;
			++i;
		}
	}

	if (thePrefs.GetVerbose()) {
		AddDebugLogLine(false, _T("IP2Country: Loaded %u ranges from %s in %u ms."), m_iplist.GetCount(), (LPCTSTR)ip2countryCSVfile, ::GetTickCount() - dwStartTick);
		AddDebugLogLine(false, _T("IP2Country: Parsed %u lines, accepted %u, duplicates %u, merged %u."), iLine, iCount, iDuplicate, iMerged);
	}

	return true;
}

void CIP2Country::RemoveAllIPs()
{
	for (int i = 0; i < m_iplist.GetCount(); ++i)
		delete m_iplist[i];
	m_iplist.RemoveAll();
}

void CIP2Country::AddIPRange(uint32 ipFrom, uint32 ipTo, const CString& shortCountryName, const CString& longCountryName)
{
	IPRange_Struct2* pRange = new IPRange_Struct2;
	pRange->IPstart = ipFrom;
	pRange->IPend = ipTo;
	pRange->ShortCountryName = shortCountryName;
	pRange->LongCountryName = longCountryName;
	m_iplist.Add(pRange);
}

IPRange_Struct2* CIP2Country::GetCountryFromIP(uint32 ip)
{
	if (ip == 0 || m_iplist.IsEmpty())
		return &m_defaultIP2Country;

	// Client IPs are stored in network order; convert once so bsearch matches the CSV numeric ranges.
	const uint32 searchIP = htonl(ip);
	IPRange_Struct2** ppFound = reinterpret_cast<IPRange_Struct2**>(
		bsearch(&searchIP, m_iplist.GetData(), m_iplist.GetCount(), sizeof(m_iplist[0]), CmpIP2CountryByAddr));
	return ppFound != NULL ? *ppFound : &m_defaultIP2Country;
}

CString CIP2Country::GetCountryNameFromRef(const IPRange_Struct2* pCountry) const
{
	return pCountry != NULL ? pCountry->LongCountryName : m_defaultIP2Country.LongCountryName;
}

CString CIP2Country::GetDefaultFilePath(bool& bUsingNewFile) const
{
	const CString strConfigDir = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR);
	const CString strNewFile = strConfigDir + DFLT_IP2COUNTRY_FILENAME;
	if (::PathFileExists(strNewFile)) {
		bUsingNewFile = true;
		return strNewFile;
	}

	bUsingNewFile = false;
	return strConfigDir + DFLT_IP2COUNTRY_FILENAME_OLD;
}
