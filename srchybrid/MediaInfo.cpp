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


CStringStream& CStringStream::operator<<(LPCTSTR psz)
{
	str += psz;
	return *this;
}

CStringStream& CStringStream::operator<<(char *psz)
{
	str += psz;
	return *this;
}

CStringStream& CStringStream::operator<<(UINT uVal)
{
	str.AppendFormat(_T("%u"), uVal);
	return *this;
}

CStringStream& CStringStream::operator<<(int iVal)
{
	str.AppendFormat(_T("%d"), iVal);
	return *this;
}

CStringStream& CStringStream::operator<<(double fVal)
{
	str.AppendFormat(_T("%.3f"), fVal);
	return *this;
}

static const struct
{
	UINT	uFmtTag;
	LPCTSTR	pszDefine;
	LPCTSTR	pszComment;
} s_WavFmtTag[] =
{
// START: Codecs from Windows SDK "mmreg.h" file
	{ 0x0000, _T(""),						_T("Unknown") },
	{ 0x0001, _T("PCM"),					_T("Uncompressed") },
	{ 0x0002, _T("ADPCM"),					_T("") },
	{ 0x0003, _T("IEEE_FLOAT"),				_T("") },
	{ 0x0004, _T("VSELP"),					_T("Compaq Computer Corp.") },
	{ 0x0005, _T("IBM_CVSD"),				_T("") },
	{ 0x0006, _T("ALAW"),					_T("") },
	{ 0x0007, _T("MULAW"),					_T("") },
	{ 0x0008, _T("DTS"),					_T("Digital Theater Systems") },
	{ 0x0009, _T("DRM"),					_T("") },
	{ 0x000A, _T("WMAVOICE9"),				_T("") },
	{ 0x000B, _T("WMAVOICE10"),				_T("") },
	{ 0x0010, _T("OKI_ADPCM"),				_T("") },
	{ 0x0011, _T("DVI_ADPCM"),				_T("Intel Corporation") },
	{ 0x0012, _T("MEDIASPACE_ADPCM"),		_T("Videologic") },
	{ 0x0013, _T("SIERRA_ADPCM"),			_T("") },
	{ 0x0014, _T("G723_ADPCM"),				_T("Antex Electronics Corporation") },
	{ 0x0015, _T("DIGISTD"),				_T("DSP Solutions, Inc.") },
	{ 0x0016, _T("DIGIFIX"),				_T("DSP Solutions, Inc.") },
	{ 0x0017, _T("DIALOGIC_OKI_ADPCM"),		_T("") },
	{ 0x0018, _T("MEDIAVISION_ADPCM"),		_T("") },
	{ 0x0019, _T("CU_CODEC"),				_T("Hewlett-Packard Company") },
	{ 0x0020, _T("YAMAHA_ADPCM"),			_T("") },
	{ 0x0021, _T("SONARC"),					_T("Speech Compression") },
	{ 0x0022, _T("DSPGROUP_TRUESPEECH"),	_T("") },
	{ 0x0023, _T("ECHOSC1"),				_T("Echo Speech Corporation") },
	{ 0x0024, _T("AUDIOFILE_AF36"),			_T("Virtual Music, Inc.") },
	{ 0x0025, _T("APTX"),					_T("Audio Processing Technology") },
	{ 0x0026, _T("AUDIOFILE_AF10"),			_T("Virtual Music, Inc.") },
	{ 0x0027, _T("PROSODY_1612"),			_T("Aculab plc") },
	{ 0x0028, _T("LRC"),					_T("Merging Technologies S.A.") },
	{ 0x0030, _T("DOLBY_AC2"),				_T("") },
	{ 0x0031, _T("GSM610"),					_T("") },
	{ 0x0032, _T("MSNAUDIO"),				_T("") },
	{ 0x0033, _T("ANTEX_ADPCME"),			_T("") },
	{ 0x0034, _T("CONTROL_RES_VQLPC"),		_T("") },
	{ 0x0035, _T("DIGIREAL"),				_T("DSP Solutions, Inc.") },
	{ 0x0036, _T("DIGIADPCM"),				_T("DSP Solutions, Inc.") },
	{ 0x0037, _T("CONTROL_RES_CR10"),		_T("") },
	{ 0x0038, _T("NMS_VBXADPCM"),			_T("Natural MicroSystems") },
	{ 0x0039, _T("CS_IMAADPCM"),			_T("Crystal Semiconductor IMA ADPCM") },
	{ 0x003A, _T("ECHOSC3"),				_T("Echo Speech Corporation") },
	{ 0x003B, _T("ROCKWELL_ADPCM"),			_T("") },
	{ 0x003C, _T("ROCKWELL_DIGITALK"),		_T("") },
	{ 0x003D, _T("XEBEC"),					_T("") },
	{ 0x0040, _T("G721_ADPCM"),				_T("Antex Electronics Corporation") },
	{ 0x0041, _T("G728_CELP"),				_T("Antex Electronics Corporation") },
	{ 0x0042, _T("MSG723"),					_T("") },
	{ 0x0050, _T("MP1"),					_T("MPEG-1, Layer 1") },
	{ 0x0051, _T("MP2"),					_T("MPEG-1, Layer 2") },
	{ 0x0052, _T("RT24"),					_T("InSoft, Inc.") },
	{ 0x0053, _T("PAC"),					_T("InSoft, Inc.") },
	{ 0x0055, _T("MP3"),					_T("MPEG-1, Layer 3") },
	{ 0x0059, _T("LUCENT_G723"),			_T("") },
	{ 0x0060, _T("CIRRUS"),					_T("") },
	{ 0x0061, _T("ESPCM"),					_T("ESS Technology") },
	{ 0x0062, _T("VOXWARE"),				_T("") },
	{ 0x0063, _T("CANOPUS_ATRAC"),			_T("") },
	{ 0x0064, _T("G726_ADPCM"),				_T("APICOM") },
	{ 0x0065, _T("G722_ADPCM"),				_T("APICOM") },
	{ 0x0067, _T("DSAT_DISPLAY"),			_T("") },
	{ 0x0069, _T("VOXWARE_BYTE_ALIGNED"),	_T("") },
	{ 0x0070, _T("VOXWARE_AC8"),			_T("") },
	{ 0x0071, _T("VOXWARE_AC10"),			_T("") },
	{ 0x0072, _T("VOXWARE_AC16"),			_T("") },
	{ 0x0073, _T("VOXWARE_AC20"),			_T("") },
	{ 0x0074, _T("VOXWARE_RT24"),			_T("") },
	{ 0x0075, _T("VOXWARE_RT29"),			_T("") },
	{ 0x0076, _T("VOXWARE_RT29HW"),			_T("") },
	{ 0x0077, _T("VOXWARE_VR12"),			_T("") },
	{ 0x0078, _T("VOXWARE_VR18"),			_T("") },
	{ 0x0079, _T("VOXWARE_TQ40"),			_T("") },
	{ 0x0080, _T("SOFTSOUND"),				_T("") },
	{ 0x0081, _T("VOXWARE_TQ60"),			_T("") },
	{ 0x0082, _T("MSRT24"),					_T("") },
	{ 0x0083, _T("G729A"),					_T("AT&T Labs, Inc.") },
	{ 0x0084, _T("MVI_MVI2"),				_T("Motion Pixels") },
	{ 0x0085, _T("DF_G726"),				_T("DataFusion Systems (Pty) (Ltd)") },
	{ 0x0086, _T("DF_GSM610"),				_T("DataFusion Systems (Pty) (Ltd)") },
	{ 0x0088, _T("ISIAUDIO"),				_T("Iterated Systems, Inc.") },
	{ 0x0089, _T("ONLIVE"),					_T("") },
	{ 0x0091, _T("SBC24"),					_T("Siemens Business Communications Sys") },
	{ 0x0092, _T("DOLBY_AC3_SPDIF"),		_T("Sonic Foundry") },
	{ 0x0093, _T("MEDIASONIC_G723"),		_T("") },
	{ 0x0094, _T("PROSODY_8KBPS"),			_T("Aculab plc") },
	{ 0x0097, _T("ZYXEL_ADPCM"),			_T("") },
	{ 0x0098, _T("PHILIPS_LPCBB"),			_T("") },
	{ 0x0099, _T("PACKED"),					_T("Studer Professional Audio AG") },
	{ 0x00A0, _T("MALDEN_PHONYTALK"),		_T("") },
	{ 0x0100, _T("RHETOREX_ADPCM"),			_T("") },
	{ 0x0101, _T("IRAT"),					_T("BeCubed Software Inc.") },
	{ 0x0111, _T("VIVO_G723"),				_T("") },
	{ 0x0112, _T("VIVO_SIREN"),				_T("") },
	{ 0x0123, _T("DIGITAL_G723"),			_T("Digital Equipment Corporation") },
	{ 0x0125, _T("SANYO_LD_ADPCM"),			_T("") },
	{ 0x0130, _T("SIPROLAB_ACEPLNET"),		_T("") },
	{ 0x0130, _T("SIPR"),					_T("Real Audio 4 (Sipro)") },
	{ 0x0131, _T("SIPROLAB_ACELP4800"),		_T("") },
	{ 0x0132, _T("SIPROLAB_ACELP8V3"),		_T("") },
	{ 0x0133, _T("SIPROLAB_G729"),			_T("") },
	{ 0x0134, _T("SIPROLAB_G729A"),			_T("") },
	{ 0x0135, _T("SIPROLAB_KELVIN"),		_T("") },
	{ 0x0140, _T("G726ADPCM"),				_T("Dictaphone Corporation") },
	{ 0x0150, _T("QUALCOMM_PUREVOICE"),		_T("") },
	{ 0x0151, _T("QUALCOMM_HALFRATE"),		_T("") },
	{ 0x0155, _T("TUBGSM"),					_T("Ring Zero Systems, Inc.") },
	{ 0x0160, _T("MSAUDIO1"),				_T("Microsoft Audio") },
	{ 0x0161, _T("WMAUDIO2"),				_T("Windows Media Audio") },
	{ 0x0162, _T("WMAUDIO3"),				_T("Windows Media Audio 9 Pro") },
	{ 0x0163, _T("WMAUDIO_LOSSLESS"),		_T("Windows Media Audio 9 Lossless") },
	{ 0x0164, _T("WMASPDIF"),				_T("Windows Media Audio Pro-over-S/PDIF") },
	{ 0x0170, _T("UNISYS_NAP_ADPCM"),		_T("") },
	{ 0x0171, _T("UNISYS_NAP_ULAW"),		_T("") },
	{ 0x0172, _T("UNISYS_NAP_ALAW"),		_T("") },
	{ 0x0173, _T("UNISYS_NAP_16K"),			_T("") },
	{ 0x0200, _T("CREATIVE_ADPCM"),			_T("") },
	{ 0x0202, _T("CREATIVE_FASTSPEECH8"),	_T("") },
	{ 0x0203, _T("CREATIVE_FASTSPEECH10"),	_T("") },
	{ 0x0210, _T("UHER_ADPCM"),				_T("") },
	{ 0x0220, _T("QUARTERDECK"),			_T("") },
	{ 0x0230, _T("ILINK_VC"),				_T("I-link Worldwide") },
	{ 0x0240, _T("RAW_SPORT"),				_T("Aureal Semiconductor") },
	{ 0x0241, _T("ESST_AC3"),				_T("ESS Technology, Inc.") },
	{ 0x0250, _T("IPI_HSX"),				_T("Interactive Products, Inc.") },
	{ 0x0251, _T("IPI_RPELP"),				_T("Interactive Products, Inc.") },
	{ 0x0260, _T("CS2"),					_T("Consistent Software") },
	{ 0x0270, _T("SONY_SCX"),				_T("") },
	{ 0x0300, _T("FM_TOWNS_SND"),			_T("Fujitsu Corp.") },
	{ 0x0400, _T("BTV_DIGITAL"),			_T("Brooktree Corporation") },
	{ 0x0401, _T("IMC"),					_T("Intel Music Coder for MSACM") },
	{ 0x0450, _T("QDESIGN_MUSIC"),			_T("") },
	{ 0x0680, _T("VME_VMPCM"),				_T("AT&T Labs, Inc.") },
	{ 0x0681, _T("TPC"),					_T("AT&T Labs, Inc.") },
	{ 0x1000, _T("OLIGSM"),					_T("Olivetti") },
	{ 0x1001, _T("OLIADPCM"),				_T("Olivetti") },
	{ 0x1002, _T("OLICELP"),				_T("Olivetti") },
	{ 0x1003, _T("OLISBC"),					_T("Olivetti") },
	{ 0x1004, _T("OLIOPR"),					_T("Olivetti") },
	{ 0x1100, _T("LH_CODEC"),				_T("Lernout & Hauspie") },
	{ 0x1400, _T("NORRIS"),					_T("") },
	{ 0x1500, _T("SOUNDSPACE_MUSICOMPRESS"),_T("AT&T Labs, Inc.") },
	{ 0x1600, _T("MPEG_ADTS_AAC"),			_T("") },
	{ 0x1601, _T("MPEG_RAW_AAC"),			_T("") },
	{ 0x1608, _T("NOKIA_MPEG_ADTS_AAC"),	_T("") },
	{ 0x1609, _T("NOKIA_MPEG_RAW_AAC"),		_T("") },
	{ 0x160A, _T("VODAFONE_MPEG_ADTS_AAC"),	_T("") },
	{ 0x160B, _T("VODAFONE_MPEG_RAW_AAC"),	_T("") },
	{ 0x2000, _T("AC3"),					_T("Dolby AC3") },
// END: Codecs from Windows SDK "mmreg.h" file

	{ 0x2001, _T("DTS"),					_T("Digital Theater Systems") },

// Real Audio (Baked) codecs
	{ 0x2002, _T("RA14"),					_T("RealAudio 1/2 14.4") },
	{ 0x2003, _T("RA28"),					_T("RealAudio 1/2 28.8") },
	{ 0x2004, _T("COOK"),					_T("RealAudio G2/8 Cook (Low Bitrate)") },
	{ 0x2005, _T("DNET"),					_T("RealAudio 3/4/5 Music (DNET)") },
	{ 0x2006, _T("RAAC"),					_T("RealAudio 10 AAC (RAAC)") },
	{ 0x2007, _T("RACP"),					_T("RealAudio 10 AAC+ (RACP)") }
};

CString GetAudioFormatName(WORD wFormatTag, CString &rstrComment)
{
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i) {
		if (s_WavFmtTag[i].uFmtTag == wFormatTag) {
			rstrComment = s_WavFmtTag[i].pszComment;
			return CString(s_WavFmtTag[i].pszDefine);
		}
	}

	CString strCompression;
	strCompression.Format(_T("0x%04x (Unknown)"), wFormatTag);
	return strCompression;
}

CString GetAudioFormatName(WORD wFormatTag)
{
	CString strComment;
	CString strFormat(GetAudioFormatName(wFormatTag, strComment));
	if (!strComment.IsEmpty())
		strFormat.AppendFormat(_T(" (%s)"), (LPCTSTR)strComment);
	return strFormat;
}

CString GetAudioFormatCodecId(WORD wFormatTag)
{
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i)
		if (s_WavFmtTag[i].uFmtTag == wFormatTag)
			return s_WavFmtTag[i].pszDefine;

	return CString();
}

CString GetAudioFormatDisplayName(const CString &strCodecId)
{
	CString sFmt;
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i)
		if (_tcsicmp(s_WavFmtTag[i].pszDefine, strCodecId) == 0) {
			if (s_WavFmtTag[i].uFmtTag && *s_WavFmtTag[i].pszDefine && *s_WavFmtTag[i].pszComment)
				sFmt.Format(_T("%s (%s)"), s_WavFmtTag[i].pszDefine, s_WavFmtTag[i].pszComment);
			break;
		}

	return sFmt;
}

bool IsEqualFOURCC(FOURCC fccA, FOURCC fccB)
{
	for (int i = 0; i < 4; ++i) {
		if (tolower((unsigned char)fccA) != tolower((unsigned char)fccB))
			return false;
		fccA >>= 8;
		fccB >>= 8;
	}
	return true;
}

#pragma warning(push)
#pragma warning(disable:4701) //local variable 'fcc'
CString GetVideoFormatDisplayName(DWORD biCompression)
{
	CString sName;
	LPCSTR pFormat;
	switch (biCompression) {
	case BI_RGB:
		pFormat = "RGB (Uncompressed)";
		break;
	case BI_RLE8:
		pFormat = "RLE8 (Run Length Encoded 8-bit)";
		break;
	case BI_RLE4:
		pFormat = "RLE4 (Run Length Encoded 4-bit)";
		break;
	case BI_BITFIELDS:
		pFormat = "Bitfields";
		break;
	case BI_JPEG:
		pFormat = "JPEG";
		break;
	case BI_PNG:
		pFormat = "PNG";
		break;
	default:
		{
			sName = CStringA((LPCSTR)&biCompression, 4);
			DWORD fcc;
			for (unsigned i = 0; i < sizeof(DWORD); ++i)
				((byte*)&fcc)[i] = (byte)toupper(((unsigned char*)&biCompression)[i]);

			switch (fcc) {
			case MAKEFOURCC('D', 'I', 'V', '3'):
				pFormat = " (DivX ;-) MPEG-4 v3 Low)";
				break;
			case MAKEFOURCC('D', 'I', 'V', '4'):
				pFormat = " (DivX ;-) MPEG-4 v3 Fast)";
				break;
			case MAKEFOURCC('D', 'I', 'V', 'X'):
				pFormat = " (DivX 4)";
				break;
			case MAKEFOURCC('D', 'X', '5', '0'):
				pFormat = " (DivX 5)";
				break;
			case MAKEFOURCC('M', 'P', 'G', '4'):
				pFormat = " (Microsoft MPEG-4 v1)";
				break;
			case MAKEFOURCC('M', 'P', '4', '2'):
				pFormat = " (Microsoft MPEG-4 v2)";
				break;
			case MAKEFOURCC('M', 'P', '4', '3'):
				pFormat = " (Microsoft MPEG-4 v3)";
				break;
			case MAKEFOURCC('D', 'X', 'S', 'B'):
				pFormat = " (Subtitle)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '1'):
				pFormat = " (Windows Media Video 7)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '2'):
				pFormat = " (Windows Media Video 8)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '3'):
				pFormat = " (Windows Media Video 9)";
				break;
			case MAKEFOURCC('W', 'V', 'C', '1'):
			case MAKEFOURCC('W', 'M', 'V', 'A'):
				pFormat = " (Windows Media Video 9 Advanced Profile / VC-1)";
				break;
			case MAKEFOURCC('R', 'V', '1', '0'):
			case MAKEFOURCC('R', 'V', '1', '3'):
				pFormat = " (Real Video 5)";
				break;
			case MAKEFOURCC('R', 'V', '2', '0'):
				pFormat = " (Real Video G2)";
				break;
			case MAKEFOURCC('R', 'V', '3', '0'):
				pFormat = " (Real Video 8)";
				break;
			case MAKEFOURCC('R', 'V', '4', '0'):
				pFormat = " (Real Video 9)";
				break;
			case MAKEFOURCC('A', 'V', 'C', '1'):
			case MAKEFOURCC('H', '2', '6', '4'):
				pFormat = " (MPEG-4 AVC)";
				break;
			case MAKEFOURCC('X', '2', '6', '4'):
				pFormat = " (x264 MPEG-4 AVC)";
				break;
			case MAKEFOURCC('H', '2', '6', '5'):
			case MAKEFOURCC('H', 'E', 'V', 'C'):
			case MAKEFOURCC('H', 'E', 'V', '1'):
			case MAKEFOURCC('H', 'V', 'C', '1'):
				pFormat = " (H.265 / HEVC)";
				break;
			case MAKEFOURCC('V', 'P', '8', '0'):
				pFormat = " (VP8)";
				break;
			case MAKEFOURCC('V', 'P', '9', '0'):
				pFormat = " (VP9)";
				break;
			case MAKEFOURCC('A', 'V', '0', '1'):
				pFormat = " (AV1)";
				break;
			case MAKEFOURCC('X', 'V', 'I', 'D'):
				pFormat = " (Xvid MPEG-4)";
				break;
			case MAKEFOURCC('T', 'S', 'C', 'C'):
				pFormat = " (TechSmith Screen Capture)";
				break;
			case MAKEFOURCC('M', 'J', 'P', 'G'):
				pFormat = " (M-JPEG)";
				break;
			case MAKEFOURCC('I', 'V', '3', '2'):
				pFormat = " (Intel Indeo Video 3.2)";
				break;
			case MAKEFOURCC('I', 'V', '4', '0'):
				pFormat = " (Intel Indeo Video 4.0)";
				break;
			case MAKEFOURCC('I', 'V', '5', '0'):
				pFormat = " (Intel Indeo Video 5.0)";
				break;
			case MAKEFOURCC('F', 'M', 'P', '4'):
				pFormat = " (MPEG-4)";
				break;
			case MAKEFOURCC('C', 'V', 'I', 'D'):
				pFormat = " (Cinepack)";
				break;
			case MAKEFOURCC('C', 'R', 'A', 'M'):
				pFormat = " (Microsoft Video 1)";
				break;
			default:
				pFormat = NULL;
			}
		}
	}
	if (pFormat)
		sName += pFormat;
	return sName;
}
#pragma warning(pop)

CString GetVideoFormatName(DWORD biCompression)
{
	CString strFormat(GetVideoFormatDisplayName(biCompression));
	if (strFormat.IsEmpty()) {
		char szFourcc[5];
		*(LPDWORD)szFourcc = biCompression;
		szFourcc[4] = '\0';
		strFormat = szFourcc;
		strFormat.MakeUpper();
	}
	return strFormat;
}

CString GetKnownAspectRatioDisplayString(float fAspectRatio)
{
	if (fabsf(fAspectRatio - (4.0f / 3.0f)) < 0.03f)
		return CString(_T("4/3"));
	if (fabsf(fAspectRatio - (16.0f / 9.0f)) < 0.03f)
		return CString(_T("16/9"));
	if (fabsf(fAspectRatio - 2.2f) < 0.03f)
		return CString(_T("2.2"));
	if (fabsf(fAspectRatio - 2.25f) < 0.03f)
		return CString(_T("2.25"));
	if (fabsf(fAspectRatio - 2.35f) < 0.03f)
		return CString(_T("2.35"));
	return CString();
}


CString GetCodecDisplayName(const CString &strCodecId)
{
	static CMapStringToString s_mapCodecDisplayName;
	CString strCodecDisplayName;
	if (s_mapCodecDisplayName.Lookup(strCodecId, strCodecDisplayName))
		return strCodecDisplayName;

	if (strCodecId.GetLength() == 3 || strCodecId.GetLength() == 4) {
		bool bHaveFourCC = true;
		FOURCC fcc;
		if (strCodecId == _T("rgb"))
			fcc = BI_RGB;
		else if (strCodecId == _T("rle8"))
			fcc = BI_RLE8;
		else if (strCodecId == _T("rle4"))
			fcc = BI_RLE4;
		else if (strCodecId == _T("jpeg"))
			fcc = BI_JPEG;
		else if (strCodecId == _T("png"))
			fcc = BI_PNG;
		else {
			fcc = MAKEFOURCC(' ', ' ', ' ', ' ');
			LPSTR pcFourCC = (LPSTR)&fcc;
			for (int i = 0; i < strCodecId.GetLength(); ++i) {
				WCHAR wch = strCodecId[i];
				if (wch >= 0x100 || (!__iscsym((unsigned char)wch) && wch != L'.' && wch != L' ')) {
					bHaveFourCC = false;
					break;
				}
				pcFourCC[i] = (CHAR)toupper((unsigned char)wch);
			}
		}
		if (bHaveFourCC)
			strCodecDisplayName = GetVideoFormatDisplayName(fcc);
	}

	if (strCodecDisplayName.IsEmpty())
		strCodecDisplayName = GetAudioFormatDisplayName(strCodecId);
	if (strCodecDisplayName.IsEmpty()) {
		strCodecDisplayName = strCodecId;
		strCodecDisplayName.MakeUpper();
	}
	s_mapCodecDisplayName[strCodecId] = strCodecDisplayName;
	return strCodecDisplayName;
}

bool GetMimeType(LPCTSTR pszFilePath, CString &rstrMimeType)
{
	int fd = OpenCrtReadOnlyLongPath(pszFilePath);
	if (fd != -1) {
		BYTE buffer[8192];
		int iRead = _read(fd, buffer, sizeof buffer);

		_close(fd);

		if (iRead > 0) {
			// Supports only 26 hardcoded types - and some more (undocumented)
			// ---------------------------------------------------------------
			// x text/richtext					.rtf
			// x text/html						.html
			// x text/xml						.xml
			// . audio/x-aiff
			// . audio/basic
			// x audio/wav						.wav
			// x audio/mid						.mid
			// x image/gif						.gif
			// . image/jpeg						( never seen, all .jpg files are "image/pjpeg" )
			// x image/pjpeg
			// . image/tiff						( never seen, all .tif files failed ??? )
			// x image/x-png					.png
			// . image/x-xbitmap
			// x image/bmp						.bmp
			// . image/x-jg
			// x image/x-emf					.emf
			// x image/x-wmf					.wmf
			// x video/avi						.avi
			// x video/mpeg						.mpg
			// x application/postscript			.ps
			// x application/base64				.b64
			// x application/macbinhex40		.hqx
			// x application/pdf				.pdf
			// . application/x-compressed
			// x application/x-zip-compressed	.zip
			// x application/x-gzip-compressed	.gz
			// x application/java				.class
			// x application/x-msdownload		.exe .dll
			//
#define FMFD_DEFAULT				0x00000000	// No flags specified. Use default behavior for the function.
#define FMFD_URLASFILENAME			0x00000001	// Treat the specified pwzUrl as a file name.
#ifndef FMFD_ENABLEMIMESNIFFING
#define FMFD_ENABLEMIMESNIFFING		0x00000002	// Force content sniffing even when host feature policy would normally disable it.
#define FMFD_IGNOREMIMETEXTPLAIN	0x00000004	// Continue sniffing when the initial proposal is only "text/plain".
#endif
			// Don't pass the file name to 'FindMimeFromData'. In case 'FindMimeFromData' can not determine the MIME type
			// from sniffing the header data it will parse the passed file name's extension to guess the MIME type.
			// That's basically OK for browser mode, but we can't use that here.
			LPWSTR pwszMime = NULL;
			HRESULT hr = FindMimeFromData(NULL, NULL/*pszFilePath*/, buffer, iRead, NULL, FMFD_ENABLEMIMESNIFFING | FMFD_IGNOREMIMETEXTPLAIN, &pwszMime, 0);
			// "application/octet-stream"	... means general "binary" file
			// "text/plain"					... means general "text" file
			if (SUCCEEDED(hr) && pwszMime != NULL && wcscmp(pwszMime, L"application/octet-stream") != 0) {
				rstrMimeType = pwszMime;
				::CoTaskMemFree(pwszMime);
				return true;
			}
			::CoTaskMemFree(pwszMime);

			// RAR file type
			if (iRead >= 7 && buffer[0] == 0x52) {
				if ((buffer[1] == 0x45 && buffer[2] == 0x7e && buffer[3] == 0x5e)
					|| (buffer[1] == 0x61 && buffer[2] == 0x72 && buffer[3] == 0x21 && buffer[4] == 0x1a && buffer[5] == 0x07 && buffer[6] == 0x00))
				{
					rstrMimeType = _T("application/x-rar-compressed");
					return true;
				}
			}

			// bzip (BZ2) file type
			static const char _cBZipheader[] = "BZh19";
			if (iRead >= (int)_countof(_cBZipheader) - 1 && memcmp(buffer, _cBZipheader, _countof(_cBZipheader) - 1) == 0) {
				rstrMimeType = _T("application/x-bzip-compressed");
				return true;
			}

			// ACE file type
			static const char _cACEheader[] = "**ACE**";
			if (iRead >= 7 + (int)_countof(_cACEheader) - 1 && memcmp(&buffer[7], _cACEheader, _countof(_cACEheader) - 1) == 0) {
				rstrMimeType = _T("application/x-ace-compressed");
				return true;
			}

			// LHA/LZH file type
			static const char _cLZHheader[] = "-lh5-";
			if (iRead >= 2 + (int)_countof(_cLZHheader) - 1 && memcmp(&buffer[2], _cLZHheader, _countof(_cLZHheader) - 1) == 0) {
				rstrMimeType = _T("application/x-lha-compressed");
				return true;
			}
		}
	}
	return false;
}

