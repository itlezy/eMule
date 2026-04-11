#pragma once

#include <stdio.h>
#include <windows.h>
#include "Preferences.h"
#include "opcodes.h"
#include "kademlia/kademlia/Tag.h"
#include "kademlia/utils/UInt128.h"

namespace Kademlia
{
namespace OracleTrace
{
	inline bool IsPublishOpcode(uint8 byOpcode)
	{
		switch (byOpcode) {
		case KADEMLIA_PUBLISH_REQ:
		case KADEMLIA2_PUBLISH_KEY_REQ:
		case KADEMLIA2_PUBLISH_SOURCE_REQ:
		case KADEMLIA2_PUBLISH_NOTES_REQ:
		case KADEMLIA2_PUBLISH_RES:
		case KADEMLIA2_PUBLISH_RES_ACK:
			return true;
		}
		return false;
	}

	inline CStringA OpcodeName(uint8 byOpcode)
	{
		switch (byOpcode) {
		case KADEMLIA_PUBLISH_REQ:
			return "KADEMLIA_PUBLISH_REQ";
		case KADEMLIA2_PUBLISH_KEY_REQ:
			return "KADEMLIA2_PUBLISH_KEY_REQ";
		case KADEMLIA2_PUBLISH_SOURCE_REQ:
			return "KADEMLIA2_PUBLISH_SOURCE_REQ";
		case KADEMLIA2_PUBLISH_NOTES_REQ:
			return "KADEMLIA2_PUBLISH_NOTES_REQ";
		case KADEMLIA2_PUBLISH_RES:
			return "KADEMLIA2_PUBLISH_RES";
		case KADEMLIA2_PUBLISH_RES_ACK:
			return "KADEMLIA2_PUBLISH_RES_ACK";
		default:
			{
				CStringA s;
				s.Format("0x%02x", byOpcode);
				return s;
			}
		}
	}

	inline CStringA Hex128(const CUInt128 &uValue)
	{
		CString sHex = uValue.ToHexString();
		return CStringA(sHex);
	}

	inline CStringA HostPort(uint32 uIP, uint16 uPort)
	{
		CStringA s;
		s.Format("%u.%u.%u.%u:%u"
			, (uIP >> 24) & 0xFF
			, (uIP >> 16) & 0xFF
			, (uIP >> 8) & 0xFF
			, uIP & 0xFF
			, uPort);
		return s;
	}

	inline CStringA Quote(const CStringA &sValue)
	{
		CStringA sQuoted("\"");
		for (int i = 0; i < sValue.GetLength(); ++i) {
			const char ch = sValue[i];
			if (ch == '\\' || ch == '"')
				sQuoted.AppendChar('\\');
			sQuoted.AppendChar(ch);
		}
		sQuoted.AppendChar('"');
		return sQuoted;
	}

	inline CStringA TagValueSummary(const CKadTag &tag)
	{
		if (tag.IsInt()) {
			CStringA s;
			s.Format("%llu", static_cast<unsigned long long>(tag.GetInt()));
			return s;
		}
		if (tag.IsStr())
			return Quote(CStringA(tag.GetStr()));
		if (tag.IsBool())
			return tag.GetInt() ? "true" : "false";
		if (tag.IsFloat()) {
			CStringA s;
			s.Format("%.3f", static_cast<double>(tag.GetFloat()));
			return s;
		}
		if (tag.IsBsob()) {
			CStringA s;
			s.Format("<bsob:%u>", tag.GetBsobSize());
			return s;
		}
		if (tag.IsHash())
			return "<hash>";
		return "<unknown>";
	}

	inline CStringA TagSummary(const TagList &tagList)
	{
		CStringA s("[");
		bool bFirst = true;
		for (TagList::const_iterator it = tagList.begin(); it != tagList.end(); ++it) {
			const CKadTag *pTag = *it;
			if (pTag == NULL)
				continue;
			if (!bFirst)
				s += ",";
			bFirst = false;
			s += CStringA((LPCSTR)pTag->m_name);
			s += "=";
			s += TagValueSummary(*pTag);
		}
		s += "]";
		return s;
	}

	inline CStringW TraceFilePath()
	{
		const CString sLogDir = thePrefs.GetMuleDirectory(EMULE_LOGDIR, true);
		return CStringW(sLogDir) + L"emule-harness-kad-trace.log";
	}

	inline CStringA Timestamp()
	{
		SYSTEMTIME st = {};
		::GetLocalTime(&st);
		CStringA s;
		s.Format("%04u-%02u-%02uT%02u:%02u:%02u.%03u"
			, st.wYear, st.wMonth, st.wDay
			, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		return s;
	}

	/// Appends one machine-readable trace line to the local oracle trace log.
	inline void Append(const CStringA &sEvent, const CStringA &sFields)
	{
		CStringA sLine;
		sLine.Format("ts=%s event=%s %s\r\n"
			, (LPCSTR)Timestamp()
			, (LPCSTR)sEvent
			, (LPCSTR)sFields);

		const CStringW sPath = TraceFilePath();
		FILE *pFile = _wfopen(sPath, L"ab");
		if (pFile != NULL) {
			fwrite((LPCSTR)sLine, 1, static_cast<size_t>(sLine.GetLength()), pFile);
			fclose(pFile);
		}
		::OutputDebugStringA(sLine);
	}
}
}
