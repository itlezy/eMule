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
typedef struct
{
	SHORT	left;
	SHORT	top;
	SHORT	right;
	SHORT	bottom;
} RECT16;

typedef struct
{
	FOURCC		fccType;
	FOURCC		fccHandler;
	DWORD		dwFlags;
	WORD		wPriority;
	WORD		wLanguage;
	DWORD		dwInitialFrames;
	DWORD		dwScale;
	DWORD		dwRate;
	DWORD		dwStart;
	DWORD		dwLength;
	DWORD		dwSuggestedBufferSize;
	DWORD		dwQuality;
	DWORD		dwSampleSize;
	RECT16		rcFrame;
} AVIStreamHeader_fixed;

#ifndef AVIFILEINFO_NOPADDING
#define AVIFILEINFO_NOPADDING	0x0400 // from the SDK tool "RIFFWALK.EXE"
#endif

#ifndef AVIFILEINFO_TRUSTCKTYPE
#define AVIFILEINFO_TRUSTCKTYPE	0x0800 // from DirectX SDK "Types of DV AVI Files"
#endif

typedef struct
{
	AVIStreamHeader_fixed	hdr;
	DWORD					dwFormatLen;
	union
	{
		BITMAPINFOHEADER	*bmi;
		PCMWAVEFORMAT		*wav;
		LPBYTE				dat;
	} fmt;
	char					*nam;
} STREAMHEADER;

static bool ReadChunkHeader(int fd, FOURCC *pfccType, DWORD *pdwLength)
{
	return _read(fd, pfccType, sizeof * pfccType) == sizeof *pfccType
		&& _read(fd, pdwLength, sizeof * pdwLength) == sizeof *pdwLength;
}

static bool ParseStreamHeader(int hAviFile, DWORD dwLengthLeft, STREAMHEADER *pStrmHdr)
{
	FOURCC fccType;
	DWORD dwLength;
	while (dwLengthLeft >= sizeof(DWORD) * 2) {
		if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
			return false;

		dwLengthLeft -= sizeof(DWORD) * 2;
		if (dwLength > dwLengthLeft) {
			errno = 0;
			return false;
		}
		dwLengthLeft -= dwLength + (dwLength & 1);

		switch (fccType) {
		case ckidSTREAMHEADER:
			if (dwLength < sizeof pStrmHdr->hdr) {
				memset(&pStrmHdr->hdr, 0x00, sizeof pStrmHdr->hdr);
				if (_read(hAviFile, &pStrmHdr->hdr, dwLength) != (int)dwLength)
					return false;
				if (dwLength & 1 && _lseek(hAviFile, 1, SEEK_CUR) < 0)
					return false;
			} else {
				if (_read(hAviFile, &pStrmHdr->hdr, sizeof pStrmHdr->hdr) != sizeof pStrmHdr->hdr)
					return false;
				if (_lseek(hAviFile, (long)(dwLength + (dwLength & 1) - sizeof pStrmHdr->hdr), SEEK_CUR) < 0)
					return false;
			}
			dwLength = 0;
			break;

		case ckidSTREAMFORMAT:
			if (dwLength > 4096) // expect corrupt data
				return false;
			try {
				pStrmHdr->dwFormatLen = dwLength;
				pStrmHdr->fmt.dat = new BYTE[dwLength];
			} catch (...) {
				errno = ENOMEM;
				return false;
			}
			if (_read(hAviFile, pStrmHdr->fmt.dat, dwLength) != (int)dwLength)
				return false;
			if (dwLength & 1) {
				if (_lseek(hAviFile, 1, SEEK_CUR) < 0)
					return false;
			}
			dwLength = 0;
			break;
		case ckidSTREAMNAME:
			if (dwLength > 512) // expect corrupt data
				return false;
			try {
				pStrmHdr->nam = new char[dwLength + 1];
			} catch (...) {
				errno = ENOMEM;
				return false;
			}
			if (_read(hAviFile, pStrmHdr->nam, dwLength) != (int)dwLength)
				return false;
			pStrmHdr->nam[dwLength] = '\0';
			if (dwLength & 1 && _lseek(hAviFile, 1, SEEK_CUR) < 0)
				return false;

			dwLength = 0;
		}

		if (dwLength && _lseek(hAviFile, dwLength + (dwLength & 1), SEEK_CUR) < 0)
			return false;
	}

	return !(dwLengthLeft && _lseek(hAviFile, dwLengthLeft, SEEK_CUR) < 0);
}

bool GetRIFFHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsAVI, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	// Open AVI file
	int hAviFile = _topen(pszFileName, O_RDONLY | O_BINARY);
	if (hAviFile < 0)
		return false;

	DWORD dwLengthLeft;
	FOURCC fccType;
	DWORD dwLength;
	bool bSizeInvalid = false;
	int iStream = 0;
	DWORD dwMovieChunkSize = 0;
	DWORD uVideoFrames = 0;
	int	iNonAVStreams = 0;
	DWORD dwAllNonVideoAvgBytesPerSec = 0;

	bool bResult = false;

	//
	// Read 'RIFF' header
	//
	if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
		goto cleanup;
	if (fccType != FOURCC_RIFF)
		goto cleanup;
	if (dwLength < sizeof(DWORD)) {
		dwLength = 0xFFFFFFF0;
		bSizeInvalid = true;
	}
	dwLengthLeft = dwLength -= sizeof(DWORD);

	//
	// Read 'AVI ' or 'WAVE' header
	//
	FOURCC fccMain;
	if (_read(hAviFile, &fccMain, sizeof fccMain) != sizeof fccMain)
		goto cleanup;
	rbIsAVI = (fccMain == formtypeAVI);
	if (!rbIsAVI && fccMain != mmioFOURCC('W', 'A', 'V', 'E'))
		goto cleanup;

	// We need to read almost all streams (regardless of 'bFullInfo' mode) because we need to get the 'dwMovieChunkSize'
	bool bHaveReadAllStreams = false;
	while (!bHaveReadAllStreams && dwLengthLeft >= sizeof(DWORD) * 2) {
		if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
			goto inv_format_errno;
		if (fccType == 0 && dwLength == 0) {
			// We jumped right into a gap which is (still) filled with 0-bytes. If we
			// continue reading this until EOF we throw an error although we may have
			// already read valid data.
			if (mi->iVideoStreams > 0 || mi->iAudioStreams > 0)
				break; // already have valid data
			goto cleanup;
		}

		bool bInvalidLength = false;
		if (!bSizeInvalid) {
			dwLengthLeft -= sizeof(DWORD) * 2;
			if (dwLength > dwLengthLeft) {
				if (fccType != FOURCC_LIST)
					goto cleanup;
				bInvalidLength = true;
			}
			dwLengthLeft -= (dwLength + (dwLength & 1));
			if (dwLengthLeft == _UI32_MAX)
				dwLengthLeft = 0;
		}

		switch (fccType) {
		case FOURCC_LIST:
			if (_read(hAviFile, &fccType, sizeof fccType) != sizeof fccType)
				goto inv_format_errno;
			if (fccType != listtypeAVIHEADER && bInvalidLength)
				goto inv_format;

			// Some Premiere plugin is writing AVI files with an invalid size field in the LIST/hdrl chunk.
			if (dwLength < sizeof(DWORD) && fccType != listtypeAVIHEADER && (fccType != listtypeAVIMOVIE || !bSizeInvalid))
				goto inv_format;
			dwLength -= sizeof(DWORD);

			switch (fccType) {
			case listtypeAVIHEADER:
				dwLengthLeft += (dwLength + (dwLength & 1)) + 4;
				dwLength = 0;	// silently enter the header block
				break;
			case listtypeSTREAMHEADER:
				{
					bool bStreamRes;
					STREAMHEADER strmhdr = {};
					if ((bStreamRes = ParseStreamHeader(hAviFile, dwLength, &strmhdr)) != false) {
						double fSamplesSec = (strmhdr.hdr.dwScale != 0) ? strmhdr.hdr.dwRate / (double)strmhdr.hdr.dwScale : 0.0;
						double fLength = (fSamplesSec != 0.0) ? strmhdr.hdr.dwLength / (double)fSamplesSec : 0.0;
						if (strmhdr.hdr.fccType == streamtypeAUDIO) {
							++mi->iAudioStreams;
							if (mi->iAudioStreams == 1) {
								mi->fAudioLengthSec = fLength;
								if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
									*(PCMWAVEFORMAT*)&mi->audio = *strmhdr.fmt.wav;
									mi->strAudioFormat = GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag);
								}
							} else {
								// this works only for CBR
								//
								// TODO: Determine VBR audio...
								if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav)
									dwAllNonVideoAvgBytesPerSec += strmhdr.fmt.wav->wf.nAvgBytesPerSec;

								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams;
									if (strmhdr.nam && strmhdr.nam[0] != '\0')
										mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
									mi->strInfo << _T("\n");
									if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag) << _T("\n");

										if (strmhdr.fmt.wav->wf.nAvgBytesPerSec) {
											CString strBitrate;
											if (strmhdr.fmt.wav->wf.nAvgBytesPerSec == _UI32_MAX)
												strBitrate = _T("Variable");
											else
												strBitrate.Format(_T("%u %s"), (UINT)((strmhdr.fmt.wav->wf.nAvgBytesPerSec * 8 + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
											mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
										}

										if (strmhdr.fmt.wav->wf.nChannels) {
											mi->strInfo << _T("   ") << GetResString(IDS_CHANNELS) << _T(":\t");
											if (strmhdr.fmt.wav->wf.nChannels == 1)
												mi->strInfo << _T("1 (Mono)");
											else if (strmhdr.fmt.wav->wf.nChannels == 2)
												mi->strInfo << _T("2 (Stereo)");
											else if (strmhdr.fmt.wav->wf.nChannels == 5)
												mi->strInfo << _T("5.1 (Surround)");
											else
												mi->strInfo << strmhdr.fmt.wav->wf.nChannels;
											mi->strInfo << _T("\n");
										}

										if (strmhdr.fmt.wav->wf.nSamplesPerSec)
											mi->strInfo << _T("   ") << GetResString(IDS_SAMPLERATE) << _T(":\t") << strmhdr.fmt.wav->wf.nSamplesPerSec / 1000.0 << _T(" kHz\n");

										if (strmhdr.fmt.wav->wBitsPerSample)
											mi->strInfo << _T("   Bit/sample:\t") << strmhdr.fmt.wav->wBitsPerSample << _T(" Bit\n");
									}
									if (fLength)
										mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << SecToTimeLength((UINT)fLength) << _T("\n");
								}
							}
						} else if (strmhdr.hdr.fccType == streamtypeVIDEO) {
							++mi->iVideoStreams;
							if (mi->iVideoStreams == 1) {
								uVideoFrames = strmhdr.hdr.dwLength;
								mi->fVideoLengthSec = fLength;
								mi->fVideoFrameRate = fSamplesSec;
								if (strmhdr.fmt.bmi && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.bmi) {
									mi->video.bmiHeader = *strmhdr.fmt.bmi;
									mi->strVideoFormat = GetVideoFormatName(strmhdr.fmt.bmi->biCompression);
									if (strmhdr.fmt.bmi->biWidth && strmhdr.fmt.bmi->biHeight)
										mi->fVideoAspectRatio = fabs(strmhdr.fmt.bmi->biWidth / (double)strmhdr.fmt.bmi->biHeight);
								}
							} else {
								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams;
									if (strmhdr.nam && strmhdr.nam[0] != '\0')
										mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
									mi->strInfo << _T("\n");
									if (strmhdr.fmt.bmi && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.bmi) {
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(strmhdr.fmt.bmi->biCompression) << _T("\n");
										if (strmhdr.fmt.bmi->biWidth && strmhdr.fmt.bmi->biHeight) {
											mi->strInfo << _T("   ") << GetResString(IDS_WIDTH) << _T(" x ") << GetResString(IDS_HEIGHT) << _T(":\t") << abs(strmhdr.fmt.bmi->biWidth) << _T(" x ") << abs(strmhdr.fmt.bmi->biHeight) << _T("\n");
											float fAspectRatio = fabsf(strmhdr.fmt.bmi->biWidth / (float)strmhdr.fmt.bmi->biHeight);
											mi->strInfo << _T("   ") << GetResString(IDS_ASPECTRATIO) << _T(":\t") << fAspectRatio << _T("  (") << GetKnownAspectRatioDisplayString(fAspectRatio) << _T(")\n");
										}
									}
									if (fLength)
										mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << SecToTimeLength((UINT)fLength) << _T("\n");
								}
							}
						} else {
							++iNonAVStreams;
							if (bFullInfo && mi->strInfo.m_hWnd) {
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->OutputFileName();
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								mi->strInfo << _T("Unknown Stream #") << iStream;
								if (strmhdr.nam && *strmhdr.nam)
									mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
								mi->strInfo << _T("\n");
								if (fLength)
									mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << SecToTimeLength((UINT)fLength) << _T("\n");
							}
						}
					}
					delete[] strmhdr.fmt.dat;
					delete[] strmhdr.nam;
					if (!bStreamRes)
						goto inv_format_errno;
					++iStream;

					dwLength = 0;
				}
				break;
			case listtypeAVIMOVIE:
				dwMovieChunkSize = dwLength;
				if (!bFullInfo)
					bHaveReadAllStreams = true;
				break;
			case mmioFOURCC('I', 'N', 'F', 'O'):
				{
					if (dwLength < 0x10000) {
						bool bError = false;
						BYTE *pChunk = new BYTE[dwLength];
						if ((DWORD)_read(hAviFile, pChunk, dwLength) == dwLength) {
							CSafeMemFile ck(pChunk, dwLength);
							try {
								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
								}

								CString strValue;
								while (ck.GetPosition() < ck.GetLength()) {
									FOURCC ckId = ck.ReadUInt32();
									DWORD ckLen = ck.ReadUInt32();
									if (ckLen < 512) {
										CStringA strValueA;
										ck.Read(strValueA.GetBuffer(ckLen), ckLen);
										strValueA.ReleaseBuffer(ckLen);
										strValue = strValueA;
										strValue.Trim();
									} else {
										ck.Seek(ckLen, CFile::current);
										strValue.Empty();
									}
									strValue.Replace(_T('\r'), _T(' '));
									strValue.Replace(_T('\n'), _T(' '));
									switch (ckId) {
									case mmioFOURCC('I', 'N', 'A', 'M'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_TITLE) << _T(":\t") << strValue << _T("\n");
										mi->strTitle = strValue;
										break;
									case mmioFOURCC('I', 'S', 'B', 'J'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Subject") << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'A', 'R', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_AUTHOR) << _T(":\t") << strValue << _T("\n");
										mi->strAuthor = strValue;
										break;
									case mmioFOURCC('I', 'C', 'O', 'P'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Copyright") << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'C', 'M', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_COMMENT) << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'C', 'R', 'D'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DATE) << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'S', 'F', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Software") << _T(":\t") << strValue << _T("\n");
										break;
									}

									if (ckLen & 1)
										ck.Seek(1, CFile::current);
								}
							} catch (CException *ex) {
								ex->Delete();
								bError = true;
							}
						} else
							bError = true;

						delete[] pChunk;

						if (bError)
							bHaveReadAllStreams = true;
						else {
							if (dwLength & 1) {
								if (_lseek(hAviFile, 1, SEEK_CUR) < 0)
									bHaveReadAllStreams = true;
							}
							dwLength = 0;
						}
					}
				}
			}
			break;
		case ckidAVIMAINHDR:
			if (dwLength == sizeof(MainAVIHeader)) {
				MainAVIHeader avihdr;
				if (_read(hAviFile, &avihdr, sizeof avihdr) != sizeof avihdr)
					goto inv_format_errno;
				ASSERT(!(dwLength & 1)); //MainAVIHeader length is even; or uncomment next lines
				//if ((dwLength & 1) && _lseek(hAviFile, 1, SEEK_CUR) < 0)
				//	goto inv_format_errno;
				dwLength = 0;
			}
			break;
		case ckidAVINEWINDEX:	// idx1
			if (!bFullInfo)
				bHaveReadAllStreams = true;
			break;
		case mmioFOURCC('f', 'm', 't', ' '):
			if (fccMain == mmioFOURCC('W', 'A', 'V', 'E')) {
				STREAMHEADER strmhdr = {};
				if (dwLength > 4096) // expect corrupt data
					goto inv_format;
				try {
					strmhdr.fmt.dat = new BYTE[strmhdr.dwFormatLen = dwLength];
				} catch (...) {
					errno = ENOMEM;
					goto inv_format_errno;
				}
				if (_read(hAviFile, strmhdr.fmt.dat, dwLength) != (int)dwLength)
					goto inv_format_errno;
				if (dwLength & 1) {
					if (_lseek(hAviFile, 1, SEEK_CUR) < 0)
						goto inv_format_errno;
				}
				dwLength = 0;

				strmhdr.hdr.fccType = streamtypeAUDIO;
				if (strmhdr.dwFormatLen) {
					++mi->iAudioStreams;
					if (mi->iAudioStreams == 1) {
						if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
							*(PCMWAVEFORMAT*)&mi->audio = *strmhdr.fmt.wav;
							mi->strAudioFormat = GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag);
						}
					}
				}
				delete[] strmhdr.fmt.dat;
				delete[] strmhdr.nam;
				++iStream;
			}
			break;
		case mmioFOURCC('d', 'a', 't', 'a'):
			if (fccMain == mmioFOURCC('W', 'A', 'V', 'E')) {
				if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && mi->audio.nAvgBytesPerSec != 0 && mi->audio.nAvgBytesPerSec != _UI32_MAX) {
					mi->fAudioLengthSec = (double)dwLength / mi->audio.nAvgBytesPerSec;
					if (mi->fAudioLengthSec < 1.0)
						mi->fAudioLengthSec = 1.0; // compensate for very small files
				}
			}
		}

		if (bHaveReadAllStreams)
			break;
		if (dwLength) {
			if (_lseek(hAviFile, dwLength + (dwLength & 1), SEEK_CUR) < 0)
				goto inv_format_errno;
		}
	}

	if (fccMain == formtypeAVI) {
		mi->strFileFormat = _T("AVI");

		// NOTE: This video bit rate is published to ed2k servers and Kad, so, do everything to determine it right!
		if (mi->iVideoStreams == 1 /*&& mi->iAudioStreams <= 1*/ && iNonAVStreams == 0 && mi->fVideoLengthSec) {
			DWORD dwVideoFramesOverhead = (DWORD)(uVideoFrames * (sizeof(WORD) + sizeof(WORD) + sizeof(DWORD)));
			mi->video.dwBitRate = (DWORD)(((dwMovieChunkSize - dwVideoFramesOverhead) / mi->fVideoLengthSec - dwAllNonVideoAvgBytesPerSec/*mi->audio.nAvgBytesPerSec*/) * 8);
		}
	} else //if (fccMain == mmioFOURCC('W', 'A', 'V', 'E'))
		mi->strFileFormat = _T("WAV (RIFF)");
	//else
	//	mi->strFileFormat = _T("RIFF");

	mi->InitFileLength();
	bResult = true;

inv_format:
inv_format_errno:
cleanup:
	_close(hAviFile);
	return bResult;
}
