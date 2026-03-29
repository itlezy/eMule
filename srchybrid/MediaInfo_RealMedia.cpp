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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "resource.h"
#include "OtherFunctions.h"
#include "MediaInfo.h"
#include "SafeFile.h"
#include <io.h>
#include <fcntl.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#pragma pack(push, 1)
typedef struct
{
	DWORD	id;
	DWORD	size;
} SRmChunkHdr;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	DWORD   file_version;
	DWORD   num_headers;
} SRmRMF;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	DWORD   max_bit_rate;
	DWORD   avg_bit_rate;
	DWORD   max_packet_size;
	DWORD   avg_packet_size;
	DWORD   num_packets;
	DWORD   duration;
	DWORD   preroll;
	DWORD   index_offset;
	DWORD   data_offset;
	WORD    num_streams;
	WORD    flags;
} SRmPROP;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	WORD    stream_number;
	DWORD   max_bit_rate;
	DWORD   avg_bit_rate;
	DWORD   max_packet_size;
	DWORD   avg_packet_size;
	DWORD   start_time;
	DWORD   preroll;
	DWORD	duration;
	//BYTE                       stream_name_size;
	//BYTE[stream_name_size]     stream_name;
	//BYTE                       mime_type_size;
	//BYTE[mime_type_size]       mime_type;
	//DWORD                      type_specific_len;
	//BYTE[type_specific_len]    type_specific_data;
} SRmMDPR;
#pragma pack(pop)

struct SRmFileProp
{
	SRmFileProp() = default;
	SRmFileProp(const CStringA &rstrName, const CStringA &rstrValue)
		: strName(rstrName)
		, strValue(rstrValue)
	{
	}
	SRmFileProp(const SRmFileProp &r)
		: strName(r.strName)
		, strValue(r.strValue)
	{
	}

	SRmFileProp& operator=(const SRmFileProp &r)
	{
		strName = r.strName;
		strValue = r.strValue;
		return *this;
	}
	CStringA strName;
	CStringA strValue;
};

CStringA GetFOURCCString(DWORD fcc)
{
	return CStringA((LPCSTR)&fcc, sizeof(DWORD)).TrimRight();
}

struct SRmCodec
{
	LPCSTR	pszID;
	LPCTSTR pszDesc;
} g_aRealMediaCodecs[] = {
	{ "14.4", _T("Real Audio 1 (14.4)") },
	{ "14_4", _T("Real Audio 1 (14.4)") },
	{ "28.8", _T("Real Audio 2 (28.8)") },
	{ "28_8", _T("Real Audio 2 (28.8)") },
	{ "RV10", _T("Real Video 5") },
	{ "RV13", _T("Real Video 5") },
	{ "RV20", _T("Real Video G2") },
	{ "RV30", _T("Real Video 8") },
	{ "RV40", _T("Real Video 9") },
	{ "atrc", _T("Real & Sony Atrac3 Codec") },
	{ "cook", _T("Real Audio G2/7 Cook (Low Bitrate)") },
	{ "dnet", _T("Real Audio 3/4/5 Music (DNET)") },
	{ "lpcJ", _T("Real Audio 1 (14.4)") },
	{ "raac", _T("Real Audio 10 AAC (RAAC)") },
	{ "racp", _T("Real Audio 10 AAC+ (RACP)") },
	{ "ralf", _T("Real Audio Lossless Format") },
	{ "rtrc", _T("Real Audio 8 (RTRC)") },
	{ "rv10", _T("Real Video 5") },
	{ "rv20", _T("Real Video G2") },
	{ "rv30", _T("Real Video 8") },
	{ "rv40", _T("Real Video 9") },
	{ "sipr", _T("Real Audio 4 (Sipro)") },
};

static int __cdecl CmpRealMediaCodec(const void *p1, const void *p2) noexcept
{
	const SRmCodec *pCodec1 = reinterpret_cast<const SRmCodec*>(p1);
	const SRmCodec *pCodec2 = reinterpret_cast<const SRmCodec*>(p2);
	return strncmp(pCodec1->pszID, pCodec2->pszID, 4);
}

CString GetRealMediaCodecInfo(LPCSTR pszCodecID)
{
	CString strInfo(GetFOURCCString(*reinterpret_cast<const DWORD*>(pszCodecID)));
	SRmCodec CodecSearch;
	CodecSearch.pszID = pszCodecID;
	const SRmCodec *pCodecFound = static_cast<SRmCodec*>(bsearch(&CodecSearch, g_aRealMediaCodecs, _countof(g_aRealMediaCodecs), sizeof g_aRealMediaCodecs[0], CmpRealMediaCodec));
	if (pCodecFound)
		strInfo.AppendFormat(_T(" (%s)"), pCodecFound->pszDesc);
	return strInfo;
}

bool GetRMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsRM, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	CSafeBufferedFile file;
	if (!file.Open(pszFileName, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary))
		return false;
	::setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	bool bReadPROP = false;
	bool bReadMDPR_Video = false;
	bool bReadMDPR_Audio = false;
	bool bReadCONT = false;
	UINT nFileBitrate = 0;
	CString strCopyright;
	CString strComment;
	CArray<SRmFileProp> aFileProps;
	try {
		ULONGLONG ullCurFilePos = 0ull;
		ULONGLONG ullChunkStartFilePos = ullCurFilePos;
		ULONGLONG ullChunkEndFilePos;

		SRmChunkHdr rmChunkHdr;
		file.Read(&rmChunkHdr, sizeof rmChunkHdr);
		DWORD dwID = _byteswap_ulong(rmChunkHdr.id);
		if (dwID != '.RMF')
			return false;
		DWORD dwSize = _byteswap_ulong(rmChunkHdr.size);
		if (dwSize < sizeof rmChunkHdr)
			return false;
		ullChunkEndFilePos = ullChunkStartFilePos + dwSize;
		dwSize -= sizeof rmChunkHdr;
		if (dwSize == 0)
			return false;

		WORD wVersion;
		file.Read(&wVersion, sizeof wVersion);
		wVersion = _byteswap_ushort(wVersion);
		if (wVersion >= 2)
			return false;

		SRmRMF rmRMF;
		file.Read(&rmRMF, sizeof rmRMF);

		rbIsRM = true;
		mi->strFileFormat = _T("Real Media");

		bool bReadMDPR_File = false;
		bool bBrokenFile = false;
		while (!bBrokenFile && (!bReadCONT || !bReadPROP || !bReadMDPR_Video || !bReadMDPR_Audio || (bFullInfo && !bReadMDPR_File))) {
			ullCurFilePos = file.GetPosition();
			if (ullCurFilePos > ullChunkEndFilePos)
				break; // expect broken DATA-chunk headers created by 'mplayer'
			if (ullCurFilePos < ullChunkEndFilePos)
				ullCurFilePos = file.Seek(ullChunkEndFilePos - ullCurFilePos, CFile::current);

			ullChunkStartFilePos = ullCurFilePos;

			file.Read(&rmChunkHdr, sizeof rmChunkHdr);
			dwID = _byteswap_ulong(rmChunkHdr.id);
			dwSize = _byteswap_ulong(rmChunkHdr.size);
			TRACE("%08x: id=%.4s  size=%u\n", (DWORD)ullCurFilePos, &rmChunkHdr.id, dwSize);
			if (dwSize < sizeof rmChunkHdr)
				break; // expect broken DATA-chunk headers created by 'mplayer'
			ullChunkEndFilePos = ullChunkStartFilePos + dwSize;
			dwSize -= sizeof rmChunkHdr;
			if (dwSize <= 0)
				continue;

			switch (dwID) {
			case 'PROP': // Properties Header
				file.Read(&wVersion, sizeof wVersion);
				wVersion = _byteswap_ushort(wVersion);
				if (wVersion == 0) {
					SRmPROP rmPROP;
					file.Read(&rmPROP, sizeof rmPROP);
					nFileBitrate = _byteswap_ulong(rmPROP.avg_bit_rate);
					mi->fFileLengthSec = (_byteswap_ulong(rmPROP.duration) + 500.0) / 1000.0;
					bReadPROP = true;
				}
				break;
			case 'MDPR': // Media Properties Header
				file.Read(&wVersion, sizeof wVersion);
				wVersion = _byteswap_ushort(wVersion);
				if (wVersion == 0) {
					SRmMDPR rmMDPR;
					file.Read(&rmMDPR, sizeof rmMDPR);

					// Read 'Stream Name'
					BYTE ucLen;
					CStringA strStreamName;
					file.Read(&ucLen, sizeof ucLen);
					file.Read(strStreamName.GetBuffer(ucLen), ucLen);
					strStreamName.ReleaseBuffer(ucLen);

					// Read 'MIME Type'
					file.Read(&ucLen, sizeof ucLen);
					CStringA strMimeType;
					file.Read(strMimeType.GetBuffer(ucLen), ucLen);
					strMimeType.ReleaseBuffer(ucLen);

					DWORD dwTypeDataLen;
					file.Read(&dwTypeDataLen, sizeof dwTypeDataLen);
					dwTypeDataLen = _byteswap_ulong(dwTypeDataLen);

					if (strMimeType == "video/x-pn-realvideo") {
						++mi->iVideoStreams;
						if (mi->iVideoStreams == 1) {
							mi->video.dwBitRate = _byteswap_ulong(rmMDPR.avg_bit_rate);
							mi->fVideoLengthSec = _byteswap_ulong(rmMDPR.duration) / 1000.0;

							if (dwTypeDataLen >= 22 + 2 && dwTypeDataLen < 8192) {
								BYTE *pucTypeData = new BYTE[dwTypeDataLen];
								try {
									file.Read(pucTypeData, dwTypeDataLen);
									if (_byteswap_ulong(*(DWORD*)(pucTypeData + 4)) == 'VIDO') {
										mi->strVideoFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 8));
										mi->video.bmiHeader.biCompression = *(DWORD*)(pucTypeData + 8);
										mi->video.bmiHeader.biWidth = _byteswap_ushort(*(WORD*)(pucTypeData + 12));
										mi->video.bmiHeader.biHeight = _byteswap_ushort(*(WORD*)(pucTypeData + 14));
										mi->fVideoAspectRatio = fabs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
										mi->fVideoFrameRate = _byteswap_ushort(*(WORD*)(pucTypeData + 22));
										bReadMDPR_Video = true;
									}
								} catch (CException *ex) {
									ex->Delete();
									delete[] pucTypeData;
									break;
								}
								delete[] pucTypeData;
							}
						}
					} else if (strMimeType == "audio/x-pn-realaudio") {
						++mi->iAudioStreams;
						if (mi->iAudioStreams == 1) {
							mi->audio.nAvgBytesPerSec = _byteswap_ulong(rmMDPR.avg_bit_rate) / 8;
							mi->fAudioLengthSec = _byteswap_ulong(rmMDPR.duration) / 1000.0;

							if (dwTypeDataLen >= 4 + 2 && dwTypeDataLen < 8192) {
								BYTE *pucTypeData = new BYTE[dwTypeDataLen];
								try {
									file.Read(pucTypeData, dwTypeDataLen);
									DWORD dwFourCC = *(DWORD*)(pucTypeData + 0);
									WORD wVer = _byteswap_ushort(*(WORD*)(pucTypeData + 4));
									if (dwFourCC == MAKEFOURCC('.', 'r', 'a', '\xFD')) {
										if (wVer == 3) {
											if (dwTypeDataLen >= 8 + 2) {
												mi->audio.nSamplesPerSec = 8000;
												mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 8));
												mi->strAudioFormat = _T(".ra3");
												bReadMDPR_Audio = true;
											}
										} else if (wVer == 4) { // RealAudio G2, RealAudio 8
											if (dwTypeDataLen >= 62 + 4) {
												mi->audio.nSamplesPerSec = _byteswap_ushort(*(WORD*)(pucTypeData + 48));
												mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 54));
												if (strncmp((LPCSTR)(pucTypeData + 62), "sipr", 4) == 0)
													mi->audio.wFormatTag = WAVE_FORMAT_SIPROLAB_ACEPLNET;
												else if (strncmp((LPCSTR)(pucTypeData + 62), "cook", 4) == 0)
													mi->audio.wFormatTag = 0x2004;
												mi->strAudioFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 62));
												bReadMDPR_Audio = true;
											}
										} else if (wVer == 5) {
											if (dwTypeDataLen >= 66 + 4) {
												mi->audio.nSamplesPerSec = _byteswap_ulong(*(DWORD*)(pucTypeData + 48));
												mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 60));
												if (strncmp((LPCSTR)(pucTypeData + 62), "sipr", 4) == 0)
													mi->audio.wFormatTag = WAVE_FORMAT_SIPROLAB_ACEPLNET;
												else if (strncmp((LPCSTR)(pucTypeData + 62), "cook", 4) == 0)
													mi->audio.wFormatTag = 0x2004;
												mi->strAudioFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 66));
												bReadMDPR_Audio = true;
											}
										}
									}
								} catch (CException *ex) {
									ex->Delete();
									delete[] pucTypeData;
									break;
								}
								delete[] pucTypeData;
							}
						}
					} else if (bFullInfo && strcmp(strMimeType, "logical-fileinfo") == 0) {
						DWORD dwLogStreamLen;
						file.Read(&dwLogStreamLen, sizeof dwLogStreamLen);
						dwLogStreamLen = _byteswap_ulong(dwLogStreamLen);

						file.Read(&wVersion, sizeof wVersion);
						wVersion = _byteswap_ushort(wVersion);
						if (wVersion == 0) {
							WORD wStreams;
							file.Read(&wStreams, sizeof wStreams);
							wStreams = _byteswap_ushort(wStreams);

							// Skip 'Physical Stream Numbers'
							file.Seek(wStreams * sizeof(WORD), CFile::current);

							// Skip 'Data Offsets'
							file.Seek(wStreams * sizeof(DWORD), CFile::current);

							WORD wRules;
							file.Read(&wRules, sizeof wRules);
							wRules = _byteswap_ushort(wRules);

							// Skip 'Rule to Physical Stream Number Map'
							file.Seek(wRules * sizeof(WORD), CFile::current);

							WORD wProperties;
							file.Read(&wProperties, sizeof wProperties);
							wProperties = _byteswap_ushort(wProperties);

							while (wProperties) {
								DWORD dwPropSize;
								file.Read(&dwPropSize, sizeof dwPropSize);
								dwPropSize = _byteswap_ulong(dwPropSize);

								WORD wPropVersion;
								file.Read(&wPropVersion, sizeof wPropVersion);
								wPropVersion = _byteswap_ushort(wPropVersion);

								if (wPropVersion == 0) {
									BYTE ucLen1;
									file.Read(&ucLen1, sizeof ucLen1);
									CStringA strPropNameA;
									file.Read(strPropNameA.GetBuffer(ucLen1), ucLen1);
									strPropNameA.ReleaseBuffer(ucLen1);

									DWORD dwPropType;
									file.Read(&dwPropType, sizeof dwPropType);
									dwPropType = _byteswap_ulong(dwPropType);

									WORD wPropValueLen;
									file.Read(&wPropValueLen, sizeof wPropValueLen);
									wPropValueLen = _byteswap_ushort(wPropValueLen);

									CStringA strPropValueA;
									if (dwPropType == 0 && wPropValueLen == sizeof(DWORD)) {
										DWORD dwPropValue;
										file.Read(&dwPropValue, sizeof dwPropValue);
										dwPropValue = _byteswap_ulong(dwPropValue);
										strPropValueA.Format("%u", dwPropValue);
									} else if (dwPropType == 2) {
										LPSTR pszA = strPropValueA.GetBuffer(wPropValueLen);
										file.Read(pszA, wPropValueLen);
										if (wPropValueLen > 0 && pszA[wPropValueLen - 1] == '\0')
											--wPropValueLen;
										strPropValueA.ReleaseBuffer(wPropValueLen);
									} else {
										file.Seek(wPropValueLen, CFile::current);
										strPropValueA.Format("<%u bytes>", wPropValueLen);
									}

									aFileProps.Add(SRmFileProp(strPropNameA, strPropValueA));
									TRACE("Prop: %s, typ=%u, size=%u, value=%s\n", (LPCSTR)strPropNameA, dwPropType, wPropValueLen, (LPCSTR)strPropValueA);
								} else
									file.Seek(dwPropSize - sizeof(DWORD) - sizeof(WORD), CFile::current);

								--wProperties;
							}
							bReadMDPR_File = true;
						}
					}
				}
				break;
			case 'CONT': // Content Description Header
				file.Read(&wVersion, sizeof wVersion);
				wVersion = _byteswap_ushort(wVersion);
				if (wVersion == 0) {
					WORD wLen;
					file.Read(&wLen, sizeof wLen);
					wLen = _byteswap_ushort(wLen);
					CStringA strA;
					file.Read(strA.GetBuffer(wLen), wLen);
					strA.ReleaseBuffer(wLen);
					mi->strTitle = strA;

					file.Read(&wLen, sizeof wLen);
					wLen = _byteswap_ushort(wLen);
					file.Read(strA.GetBuffer(wLen), wLen);
					strA.ReleaseBuffer(wLen);
					mi->strAuthor = strA;

					file.Read(&wLen, sizeof wLen);
					wLen = _byteswap_ushort(wLen);
					file.Read(strA.GetBuffer(wLen), wLen);
					strA.ReleaseBuffer(wLen);
					strCopyright = strA;

					file.Read(&wLen, sizeof wLen);
					wLen = _byteswap_ushort(wLen);
					file.Read(strA.GetBuffer(wLen), wLen);
					strA.ReleaseBuffer(wLen);
					strComment = strA;
					bReadCONT = true;
				}
				break;
			case 'DATA':
			case 'INDX':
			case 'RMMD':
			case 'RMJE':
				// Expect broken DATA-chunk headers created by 'mplayer'. Thus catch
				// all valid tags just to have chance to detect the broken ones.
				break;
			default:
				// Expect broken DATA-chunk headers created by 'mplayer'. Stop reading
				// the file on first broken chunk header.
				bBrokenFile = true;
			}
		}
	} catch (CException *ex) {
		ex->Delete();
	}

	// Expect broken DATA-chunk headers created by 'mplayer'. A broken DATA-chunk header
	// may not be a fatal error. We may have already successfully read the media properties
	// headers. Therefore we indicate 'success' if we read at least some valid headers.
	if (!bReadCONT && !bReadPROP && !bReadMDPR_Video && !bReadMDPR_Audio)
		return false;
	mi->InitFileLength();
	if (bFullInfo
		&& mi->strInfo.m_hWnd
		&& (nFileBitrate
			|| !mi->strTitle.IsEmpty()
			|| !mi->strAuthor.IsEmpty()
			|| !strCopyright.IsEmpty()
			|| !strComment.IsEmpty()
			|| !aFileProps.IsEmpty()))
	{
		if (!mi->strInfo.IsEmpty())
			mi->strInfo << _T("\n");
		mi->OutputFileName();
		mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
		mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");

		if (nFileBitrate) {
			CString strBitrate;
			strBitrate.Format(_T("%u %s"), (UINT)((nFileBitrate + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
			mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
		}
		if (!mi->strTitle.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_TITLE) << _T(":\t") << mi->strTitle << _T("\n");
		if (!mi->strAuthor.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_AUTHOR) << _T(":\t") << mi->strAuthor << _T("\n");
		if (!strCopyright.IsEmpty())
			mi->strInfo << _T("   ") << _T("Copyright") << _T(":\t") << strCopyright << _T("\n");
		if (!strComment.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_COMMENT) << _T(":\t") << strComment << _T("\n");
		for (INT_PTR i = 0; i < aFileProps.GetCount(); ++i)
			if (!aFileProps[i].strValue.IsEmpty())
				mi->strInfo << _T("   ") << (CString)aFileProps[i].strName << _T(":\t") << (CString)aFileProps[i].strValue << _T("\n");
	}

	return true;
}
