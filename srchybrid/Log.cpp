//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include <io.h>
#include <share.h>
#include "emule.h"
#include "Log.h"
#include "Opcodes.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "emuledlg.h"
#include "StringConversion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
namespace
{
CCriticalSection g_oracleUdpDumpLock;
CCriticalSection g_oracleEd2kTcpDumpLock;
static const UINT ORACLE_ED2K_TCP_HEADER_SIZE = 6;

CString BuildOracleUdpTimestamp()
{
	SYSTEMTIME st = {};
	::GetLocalTime(&st);
	CString strTimestamp;
	strTimestamp.Format(_T("%04u-%02u-%02uT%02u:%02u:%02u.%03u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	return strTimestamp;
}

CString BuildOracleEd2kTcpTimestampUtc()
{
	SYSTEMTIME st = {};
	::GetSystemTime(&st);
	CString strTimestamp;
	strTimestamp.Format(_T("%04u-%02u-%02uT%02u:%02u:%02u.%03uZ"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	return strTimestamp;
}

CString BuildOracleUdpHexString(const BYTE *pPayload, UINT uPayloadLen)
{
	if (pPayload == NULL || uPayloadLen == 0)
		return CString();

	CString strHex;
	LPTSTR pszHex = strHex.GetBuffer(static_cast<int>(uPayloadLen * 2));
	static const TCHAR s_szDigits[] = _T("0123456789ABCDEF");
	for (UINT uIndex = 0; uIndex < uPayloadLen; ++uIndex) {
		const BYTE byValue = pPayload[uIndex];
		pszHex[uIndex * 2] = s_szDigits[(byValue >> 4) & 0x0F];
		pszHex[uIndex * 2 + 1] = s_szDigits[byValue & 0x0F];
	}
	strHex.ReleaseBuffer(static_cast<int>(uPayloadLen * 2));
	return strHex;
}

CString EscapeOracleUdpJson(const CString &strValue)
{
	CString strEscaped;
	for (int i = 0; i < strValue.GetLength(); ++i) {
		const TCHAR ch = strValue[i];
		switch (ch) {
		case _T('\\'):
			strEscaped += _T("\\\\");
			break;
		case _T('"'):
			strEscaped += _T("\\\"");
			break;
		case _T('\r'):
			strEscaped += _T("\\r");
			break;
		case _T('\n'):
			strEscaped += _T("\\n");
			break;
		case _T('\t'):
			strEscaped += _T("\\t");
			break;
		default:
			if (static_cast<unsigned int>(ch) < 0x20u) {
				CString strCodePoint;
				strCodePoint.Format(_T("\\u%04X"), static_cast<unsigned int>(ch));
				strEscaped += strCodePoint;
			} else {
				strEscaped.AppendChar(ch);
			}
		}
	}
	return strEscaped;
}

CString ExtractOracleUdpTransportMode(LPCTSTR pszMetadata)
{
	if (pszMetadata == NULL || pszMetadata[0] == _T('\0'))
		return CString();

	const CString strMetadata(pszMetadata);
	const CString strNeedle(_T("transport_mode="));
	const int iStart = strMetadata.Find(strNeedle);
	if (iStart < 0)
		return CString();

	const int iValueStart = iStart + strNeedle.GetLength();
	int iValueEnd = strMetadata.Find(_T(' '), iValueStart);
	if (iValueEnd < 0)
		iValueEnd = strMetadata.GetLength();
	if (iValueEnd <= iValueStart)
		return CString();

	return strMetadata.Mid(iValueStart, iValueEnd - iValueStart);
}

LPCTSTR OracleEd2kProtocolName(uint8 byProtocol)
{
	switch (byProtocol) {
	case OP_EDONKEYPROT:
		return _T("ed2k");
	case OP_EMULEPROT:
		return _T("emule");
	case OP_PACKEDPROT:
		return _T("packed");
	default:
		return NULL;
	}
}

CString OracleEd2kOpcodeName(uint8 byProtocol, uint8 byOpcode)
{
	if (byProtocol == OP_EDONKEYPROT)
		return DbgGetDonkeyClientTCPOpcode(byOpcode);
	if (byProtocol == OP_EMULEPROT || byProtocol == OP_PACKEDPROT)
		return DbgGetMuleClientTCPOpcode(byOpcode);

	CString strOpcode;
	strOpcode.Format(_T("0x%02X"), byOpcode);
	return strOpcode;
}

// Mirror the Rust helper dump format so the oracle and agent TCP handshakes can
// be diffed directly without rebuilding packets from verbose logs.
void OracleEd2kTcpDumpImpl(LPCTSTR pszFlow, LPCTSTR pszPhase, LPCTSTR pszDirection, LPCTSTR pszPeerLabel, LPCTSTR pszTransportMode, uint8 byProtocol, uint8 byOpcode, const BYTE *pPayload, UINT uPayloadLen, LPCTSTR pszNote)
{
	if (!theOracleEd2kTcpDumpLog.IsOpen())
		return;

	const CString strTimestamp = EscapeOracleUdpJson(BuildOracleEd2kTcpTimestampUtc());
	const CString strFlow = EscapeOracleUdpJson(pszFlow != NULL ? pszFlow : _T("unknown"));
	const CString strPhase = EscapeOracleUdpJson(pszPhase != NULL ? pszPhase : _T("session"));
	const CString strDirection = EscapeOracleUdpJson(pszDirection != NULL ? pszDirection : _T("meta"));
	const CString strPeer = EscapeOracleUdpJson(pszPeerLabel != NULL ? pszPeerLabel : _T("unknown"));
	const CString strTransportMode = EscapeOracleUdpJson(pszTransportMode != NULL ? pszTransportMode : _T("unknown"));
	const CString strNote = (pszNote != NULL && pszNote[0] != _T('\0')) ? EscapeOracleUdpJson(CString(pszNote)) : CString();

	CString strJson;
	strJson.Format(
		_T("{\"schema\":\"ed2k_tcp_helper_v1\",\"source\":\"oracle\",\"ts_utc\":\"%s\",\"flow\":\"%s\",\"phase\":\"%s\",\"direction\":\"%s\",\"remote_addr\":\"%s\",\"transport_mode\":\"%s\""),
		(LPCTSTR)strTimestamp,
		(LPCTSTR)strFlow,
		(LPCTSTR)strPhase,
		(LPCTSTR)strDirection,
		(LPCTSTR)strPeer,
		(LPCTSTR)strTransportMode);

	if (pPayload != NULL) {
		const UINT uRawLen = uPayloadLen + ORACLE_ED2K_TCP_HEADER_SIZE;
		BYTE *pRawPacket = new BYTE[uRawLen];
		pRawPacket[0] = byProtocol;
		const uint32 uPacketLength = uPayloadLen + 1;
		pRawPacket[1] = static_cast<BYTE>(uPacketLength & 0xFF);
		pRawPacket[2] = static_cast<BYTE>((uPacketLength >> 8) & 0xFF);
		pRawPacket[3] = static_cast<BYTE>((uPacketLength >> 16) & 0xFF);
		pRawPacket[4] = static_cast<BYTE>((uPacketLength >> 24) & 0xFF);
		pRawPacket[5] = byOpcode;
		if (uPayloadLen > 0)
			memcpy(pRawPacket + ORACLE_ED2K_TCP_HEADER_SIZE, pPayload, uPayloadLen);
		const CString strRawHex = BuildOracleUdpHexString(pRawPacket, uRawLen);
		const CString strPayloadHex = BuildOracleUdpHexString(pPayload, uPayloadLen);
		delete[] pRawPacket;

		const LPCTSTR pszProtocolName = OracleEd2kProtocolName(byProtocol);
		if (pszProtocolName != NULL)
			strJson += _T(",\"protocol\":\"") + EscapeOracleUdpJson(pszProtocolName) + _T("\"");
		else
			strJson += _T(",\"protocol\":null");

		CString strMarkers;
		strMarkers.Format(_T(",\"protocol_marker\":%u,\"opcode\":%u,\"opcode_name\":\"%s\",\"raw_len\":%u,\"raw_hex\":\"%s\",\"payload_len\":%u,\"payload_hex\":\"%s\""),
			static_cast<UINT>(byProtocol),
			static_cast<UINT>(byOpcode),
			(LPCTSTR)EscapeOracleUdpJson(OracleEd2kOpcodeName(byProtocol, byOpcode)),
			uRawLen,
			(LPCTSTR)strRawHex,
			uPayloadLen,
			(LPCTSTR)strPayloadHex);
		strJson += strMarkers;
	} else {
		strJson += _T(",\"protocol\":null,\"protocol_marker\":null,\"opcode\":null,\"opcode_name\":null,\"raw_len\":null,\"raw_hex\":null,\"payload_len\":null,\"payload_hex\":null");
	}

	if (!strNote.IsEmpty())
		strJson += _T(",\"note\":\"") + strNote + _T("\"");
	else
		strJson += _T(",\"note\":null");
	strJson += _T("}\r\n");

	CSingleLock lock(&g_oracleEd2kTcpDumpLock, TRUE);
	theOracleEd2kTcpDumpLog.Log(strJson);
}

void OracleUdpDumpImpl(LPCTSTR pszDirection, LPCTSTR pszFamily, LPCTSTR pszPeerLabel, const BYTE *pPayload, UINT uPayloadLen, LPCTSTR pszMetadata, const BYTE *pDecodedPayload, UINT uDecodedPayloadLen)
{
	if (pPayload == NULL || uPayloadLen == 0 || !theOracleUdpDumpLog.IsOpen())
		return;

	const CString strTimestamp = EscapeOracleUdpJson(BuildOracleUdpTimestamp());
	const CString strDirection = EscapeOracleUdpJson(pszDirection != NULL ? pszDirection : _T("unknown"));
	const CString strFamily = EscapeOracleUdpJson(pszFamily != NULL ? pszFamily : _T("unknown"));
	const CString strPeer = EscapeOracleUdpJson(pszPeerLabel != NULL ? pszPeerLabel : _T("unknown"));
	const CString strWireHex = BuildOracleUdpHexString(pPayload, uPayloadLen);
	const CString strSummary = (pszMetadata != NULL && pszMetadata[0] != _T('\0')) ? EscapeOracleUdpJson(CString(pszMetadata)) : CString();
	const CString strTransportMode = EscapeOracleUdpJson(ExtractOracleUdpTransportMode(pszMetadata));
	const bool bHasDecoded = pDecodedPayload != NULL && uDecodedPayloadLen > 0;
	const CString strDecodedHex = bHasDecoded ? BuildOracleUdpHexString(pDecodedPayload, uDecodedPayloadLen) : CString();

	CString strJson;
	strJson.Format(
		_T("{\"schema\":\"udp_packet_v1\",\"source\":\"oracle\",\"ts\":\"%s\",\"direction\":\"%s\",\"family\":\"%s\",\"peer\":\"%s\",\"wire_len\":%u,\"wire_hex\":\"%s\""),
		(LPCTSTR)strTimestamp,
		(LPCTSTR)strDirection,
		(LPCTSTR)strFamily,
		(LPCTSTR)strPeer,
		uPayloadLen,
		(LPCTSTR)strWireHex);
	if (bHasDecoded) {
		CString strDecodedField;
		strDecodedField.Format(_T(",\"decoded_len\":%u,\"decoded_hex\":\"%s\""), uDecodedPayloadLen, (LPCTSTR)strDecodedHex);
		strJson += strDecodedField;
	} else {
		strJson += _T(",\"decoded_len\":null,\"decoded_hex\":null");
	}
	if (!strTransportMode.IsEmpty()) {
		strJson += _T(",\"transport_mode\":\"") + strTransportMode + _T("\"");
	} else {
		strJson += _T(",\"transport_mode\":null");
	}
	if (!strSummary.IsEmpty()) {
		strJson += _T(",\"summary\":\"") + strSummary + _T("\"");
	} else {
		strJson += _T(",\"summary\":null");
	}
	strJson += _T("}\r\n");

	CSingleLock lock(&g_oracleUdpDumpLock, TRUE);
	theOracleUdpDumpLog.Log(strJson);
}
} // namespace

void LogV(UINT uFlags, LPCTSTR pszFmt, va_list argp)
{
	AddLogTextV(uFlags, DLP_DEFAULT, pszFmt, argp);
}

///////////////////////////////////////////////////////////////////////////////
//

void Log(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_DEFAULT, pszFmt, argp);
	va_end(argp);
}

void LogError(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_ERROR, pszFmt, argp);
	va_end(argp);
}

void LogWarning(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_WARNING, pszFmt, argp);
	va_end(argp);
}

///////////////////////////////////////////////////////////////////////////////
//

void Log(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags, pszFmt, argp);
	va_end(argp);
}

void LogError(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags | LOG_ERROR, pszFmt, argp);
	va_end(argp);
}

void LogWarning(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags | LOG_WARNING, pszFmt, argp);
	va_end(argp);
}

///////////////////////////////////////////////////////////////////////////////
//

void DebugLog(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_DEBUG, pszFmt, argp);
	va_end(argp);
}

void DebugLogError(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_DEBUG | LOG_ERROR, pszFmt, argp);
	va_end(argp);
}

void DebugLogWarning(LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(LOG_DEBUG | LOG_WARNING, pszFmt, argp);
	va_end(argp);
}

///////////////////////////////////////////////////////////////////////////////
//

void DebugLog(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags | LOG_DEBUG, pszFmt, argp);
	va_end(argp);
}

void DebugLogError(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags | LOG_DEBUG | LOG_ERROR, pszFmt, argp);
	va_end(argp);
}

void DebugLogWarning(UINT uFlags, LPCTSTR pszFmt, ...)
{
	va_list argp;
	va_start(argp, pszFmt);
	LogV(uFlags | LOG_DEBUG | LOG_WARNING, pszFmt, argp);
	va_end(argp);
}


///////////////////////////////////////////////////////////////////////////////
//

void AddLogLine(bool bAddToStatusBar, LPCTSTR pszLine, ...)
{
	ASSERT(pszLine != NULL);

	va_list argptr;
	va_start(argptr, pszLine);
	AddLogTextV(LOG_DEFAULT | (bAddToStatusBar ? LOG_STATUSBAR : 0), DLP_DEFAULT, pszLine, argptr);
	va_end(argptr);
}

void AddDebugLogLine(bool bAddToStatusBar, LPCTSTR pszLine, ...)
{
	ASSERT(pszLine != NULL);

	va_list argptr;
	va_start(argptr, pszLine);
	AddLogTextV(LOG_DEBUG | (bAddToStatusBar ? LOG_STATUSBAR : 0), DLP_DEFAULT, pszLine, argptr);
	va_end(argptr);
}

void AddDebugLogLine(EDebugLogPriority Priority, bool bAddToStatusBar, LPCTSTR pszLine, ...)
{
	// loglevel needs to be merged with LOG_WARNING and LOG_ERROR later
	// (which only means 3 instead of 5 levels which you can select in the preferences)
	// makes no sense to implement two different priority indicators
	//
	// until there is some time todo this, It will convert DLP_VERYHIGH to ERRORs
	// and DLP_HIGH to LOG_WARNING in order to be able using the Loglevel and color indicator
	ASSERT(pszLine != NULL);

	va_list argptr;
	va_start(argptr, pszLine);
	uint32 nFlag;
	if (Priority == DLP_VERYHIGH)
		nFlag = LOG_ERROR;
	else if (Priority == DLP_HIGH)
		nFlag = LOG_WARNING;
	else
		nFlag = 0;

	AddLogTextV(LOG_DEBUG | nFlag | (bAddToStatusBar ? LOG_STATUSBAR : 0), Priority, pszLine, argptr);
	va_end(argptr);
}

void AddLogTextV(UINT uFlags, EDebugLogPriority dlpPriority, LPCTSTR pszLine, va_list argptr)
{
	ASSERT(pszLine != NULL);

	if ((uFlags & LOG_DEBUG) && !(thePrefs.GetVerbose() && dlpPriority >= thePrefs.GetVerboseLogPriority()))
		return;

	TCHAR szLogLine[1000];
	_vsntprintf(szLogLine, _countof(szLogLine), pszLine, argptr);
	szLogLine[_countof(szLogLine) - 1] = _T('\0');

	if (theApp.emuledlg)
		theApp.emuledlg->AddLogText(uFlags, szLogLine);
	else {
		TRACE(_T("App Log: %s\n"), szLogLine);

		TCHAR szFullLogLine[1060];
		int iLen = _sntprintf(szFullLogLine, _countof(szFullLogLine), _T("%s: %s\r\n"), (LPCTSTR)CTime::GetCurrentTime().Format(thePrefs.GetDateTimeFormat4Log()), szLogLine);
		if (iLen > 0) {
			if (!(uFlags & LOG_DEBUG) && thePrefs.GetLog2Disk())
				theLog.Log(szFullLogLine, iLen);

			if (thePrefs.GetVerbose() && ((uFlags & LOG_DEBUG) || thePrefs.GetFullVerbose()))
				if (thePrefs.GetDebug2Disk())
					theVerboseLog.Log(szFullLogLine, iLen);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// CLogFile

CLogFile::CLogFile()
	: m_fp()
	, m_tStarted()
	, m_uBytesWritten()
	, m_uMaxFileSize(_UI32_MAX)
	, m_bInOpenCall()
	, m_eFileFormat(Unicode)
{
	ASSERT(Unicode == 0);
}

CLogFile::~CLogFile()
{
	Close();
}

const CString& CLogFile::GetFilePath() const
{
	return m_strFilePath;
}

bool CLogFile::SetFilePath(LPCTSTR pszFilePath)
{
	if (IsOpen())
		return false;
	m_strFilePath = pszFilePath;
	return true;
}

void CLogFile::SetMaxFileSize(UINT uMaxFileSize)
{
	if (uMaxFileSize < 0x10000u)
		m_uMaxFileSize = (uMaxFileSize == 0) ? _UI32_MAX : 0x10000u;
	else
		m_uMaxFileSize = uMaxFileSize;
}

bool CLogFile::SetFileFormat(const ELogFileFormat eFileFormat)
{
	if (eFileFormat != Unicode && eFileFormat != Utf8) {
		ASSERT(0);
		return false;
	}
	if (m_fp != NULL)
		return false; // can't change file format on-the-fly
	m_eFileFormat = eFileFormat;
	return true;
}

bool CLogFile::IsOpen() const
{
	return m_fp != NULL;
}

bool CLogFile::Create(LPCTSTR pszFilePath, UINT uMaxFileSize, const ELogFileFormat eFileFormat)
{
	Close();
	m_strFilePath = pszFilePath;
	m_uMaxFileSize = uMaxFileSize;
	m_eFileFormat = eFileFormat;
	return Open();
}

bool CLogFile::Open()
{
	if (m_fp != NULL)
		return true;

	m_fp = _tfsopen(m_strFilePath, _T("a+b"), _SH_DENYWR);
	if (m_fp != NULL) {
		m_tStarted = time(NULL);
		m_uBytesWritten = _filelength(_fileno(m_fp));
		if (m_uBytesWritten == 0) {
			if (m_eFileFormat == Unicode) {
				// write Unicode byte order mark 0xFEFF
				fputwc(0xFEFFui16, m_fp);
			} else {
				ASSERT(m_eFileFormat == Utf8);
				; // could write UTF-8 header.
			}
		} else if (m_uBytesWritten >= sizeof(WORD)) {
			// check for Unicode byte order mark 0xFEFF
			WORD wBOM;
			if (fread(&wBOM, sizeof wBOM, 1, m_fp) == 1) {
				if (wBOM == 0xFEFFui16 && m_eFileFormat == Unicode) {
					// log file already in Unicode format
					(void)fseek(m_fp, 0, SEEK_END); // actually not needed because file is opened in 'Append' mode.
				} else if (wBOM != 0xFEFFui16 && m_eFileFormat != Unicode) {
					// log file already in UTF-8 format
					(void)fseek(m_fp, 0, SEEK_END); // actually not needed because file is opened in 'Append' mode.
				} else {
					// log file does not have the required format, create a new one (with the req. format)
					ASSERT((m_eFileFormat == Unicode && wBOM != 0xFEFFui16) || (m_eFileFormat == Utf8 && wBOM == 0xFEFF));

					ASSERT(!m_bInOpenCall);
					if (!m_bInOpenCall) { // just for safety
						m_bInOpenCall = true;
						StartNewLogFile();
						m_bInOpenCall = false;
					}
				}
			}
		}
	}
	return m_fp != NULL;
}

bool CLogFile::Close()
{
	if (m_fp == NULL)
		return true;
	bool bResult = (fclose(m_fp) == 0);
	m_fp = NULL;
	m_tStarted = 0;
	m_uBytesWritten = 0;
	return bResult;
}

bool CLogFile::Log(LPCTSTR pszMsg, int iLen)
{
	if (m_fp == NULL)
		return false;

	size_t uWritten;
	if (m_eFileFormat == Unicode) {
		// don't use 'fputs' + '_filelength' -- gives poor performance
		size_t uToWrite = ((iLen == -1) ? _tcslen(pszMsg) : (size_t)iLen) * sizeof(TCHAR);
		uWritten = fwrite(pszMsg, 1, uToWrite, m_fp);
	} else {
		TUnicodeToUTF8<2048> utf8(pszMsg, iLen);
		uWritten = fwrite((LPCSTR)utf8, 1, utf8.GetLength(), m_fp);
	}
	bool bResult = !ferror(m_fp);
	m_uBytesWritten += uWritten;

	if (m_uBytesWritten >= m_uMaxFileSize)
		StartNewLogFile();
	else
		fflush(m_fp);

	return bResult;
}

bool CLogFile::Logf(LPCTSTR pszFmt, ...)
{
	if (m_fp == NULL)
		return false;

	va_list argp;
	va_start(argp, pszFmt);

	TCHAR szMsg[1024];
	_vsntprintf(szMsg, _countof(szMsg), pszFmt, argp);
	szMsg[_countof(szMsg) - 1] = _T('\0');
	va_end(argp);

	TCHAR szFullMsg[1060];
	int iLen = _sntprintf(szFullMsg, _countof(szFullMsg), _T("%s: %s\r\n"), (LPCTSTR)CTime::GetCurrentTime().Format(thePrefs.GetDateTimeFormat4Log()), szMsg);

	return (iLen > 0) && Log(szFullMsg, iLen);
}

void OracleUdpDump(LPCTSTR pszDirection, LPCTSTR pszFamily, uint32 dwIP, uint16 nPort, const BYTE *pPayload, UINT uPayloadLen, LPCTSTR pszMetadata, const BYTE *pDecodedPayload, UINT uDecodedPayloadLen)
{
	CString strPeer;
	strPeer.Format(_T("%s:%u"), (LPCTSTR)ipstr(dwIP), nPort);
	OracleUdpDumpImpl(pszDirection, pszFamily, strPeer, pPayload, uPayloadLen, pszMetadata, pDecodedPayload, uDecodedPayloadLen);
}

void OracleUdpDumpPeerLabel(LPCTSTR pszDirection, LPCTSTR pszFamily, LPCTSTR pszPeerLabel, const BYTE *pPayload, UINT uPayloadLen, LPCTSTR pszMetadata, const BYTE *pDecodedPayload, UINT uDecodedPayloadLen)
{
	OracleUdpDumpImpl(pszDirection, pszFamily, pszPeerLabel, pPayload, uPayloadLen, pszMetadata, pDecodedPayload, uDecodedPayloadLen);
}

void OracleEd2kTcpDumpMeta(LPCTSTR pszFlow, LPCTSTR pszPhase, LPCTSTR pszPeerLabel, LPCTSTR pszTransportMode, LPCTSTR pszNote)
{
	OracleEd2kTcpDumpImpl(pszFlow, pszPhase, _T("meta"), pszPeerLabel, pszTransportMode, 0, 0, NULL, 0, pszNote);
}

void OracleEd2kTcpDumpPacket(LPCTSTR pszFlow, LPCTSTR pszPhase, LPCTSTR pszDirection, LPCTSTR pszPeerLabel, LPCTSTR pszTransportMode, uint8 byProtocol, uint8 byOpcode, const BYTE *pPayload, UINT uPayloadLen, LPCTSTR pszNote)
{
	OracleEd2kTcpDumpImpl(pszFlow, pszPhase, pszDirection, pszPeerLabel, pszTransportMode, byProtocol, byOpcode, pPayload, uPayloadLen, pszNote);
}

void CLogFile::StartNewLogFile()
{
	time_t tStarted = m_tStarted;
	Close();

	TCHAR szDateLogStarted[40];
	_tcsftime(szDateLogStarted, _countof(szDateLogStarted), _T("%Y.%m.%d %H.%M.%S"), localtime(&tStarted));

	TCHAR szDrv[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szNam[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	_tsplitpath(m_strFilePath, szDrv, szDir, szNam, szExt);

	CString strLogBakNam;
	strLogBakNam.Format(_T("%s - %s"), szNam, szDateLogStarted);

	TCHAR szLogBakFilePath[MAX_PATH];
	_tmakepathlimit(szLogBakFilePath, szDrv, szDir, strLogBakNam, szExt);

	if (_trename(m_strFilePath, szLogBakFilePath) != 0)
		VERIFY(_tremove(m_strFilePath) == 0);

	Open();
}
