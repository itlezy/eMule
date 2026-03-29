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
#ifdef HAVE_WMSDK_H
#include <wmsdk.h>
#endif//HAVE_WMSDK_H
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#ifdef HAVE_WMSDK_H

/** @brief Returns true if WMFSDK reported that an optional attribute is not exposed for the current stream. */
bool IsMissingWmAttributeResult(HRESULT hr)
{
	return hr == ASF_E_NOTFOUND
		|| hr == E_INVALIDARG;
}

template<class T, WMT_ATTR_DATATYPE attrTypeT>
bool GetAttributeT(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nValue)
{
	WMT_ATTR_DATATYPE attrType;
	WORD wValueSize = sizeof nValue;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, (BYTE*)&nValue, &wValueSize);
	if (hr == ASF_E_NOTFOUND)
		return false;
	if (hr != S_OK || attrType != attrTypeT) {
		ASSERT(0);
		return false;
	}
	return true;
}

template<class T> bool GetAttributeT(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData);

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, BOOL &nData)
{
	return GetAttributeT<BOOL, WMT_TYPE_BOOL>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, DWORD &nData)
{
	return GetAttributeT<DWORD, WMT_TYPE_DWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, QWORD &nData)
{
	return GetAttributeT<QWORD, WMT_TYPE_QWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CStringW &strValue)
{
	strValue.Empty(); //prepare for the worst
	WMT_ATTR_DATATYPE attrType;
	WORD wValueSize;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, NULL, &wValueSize);
	if (hr != S_OK || attrType != WMT_TYPE_STRING || wValueSize < sizeof(WCHAR) || (wValueSize % sizeof(WCHAR)) != 0) {
		ASSERT(IsMissingWmAttributeResult(hr));
		return false;
	}

	// empty string?
	if (wValueSize == sizeof(WCHAR))
		return false;

	hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, (BYTE*)strValue.GetBuffer(wValueSize / sizeof(WCHAR)), &wValueSize);
	strValue.ReleaseBuffer();
	if (hr != S_OK || attrType != WMT_TYPE_STRING) {
		ASSERT(0);
		return false;
	}

	if (strValue.IsEmpty())
		return false;

	// SDK states that MP3 files could contain a BOM - never seen
	if (strValue[0] == u'\xFFFE' || strValue[0] == u'\xFEFF') {
		ASSERT(0);
		strValue.Delete(0, 1);
	}

	return true;
}

bool GetAttributeIndices(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CTempBuffer<WORD> &aIndices, WORD &wLangIndex)
{
	WORD wIndexCount;
	HRESULT hr = pIWMHeaderInfo->GetAttributeIndices(wStream, pwszName, &wLangIndex, NULL, &wIndexCount);
	if (hr != S_OK) {
		ASSERT(IsMissingWmAttributeResult(hr));
		return false;
	}
	if (wIndexCount == 0)
		return false;

	hr = pIWMHeaderInfo->GetAttributeIndices(wStream, pwszName, &wLangIndex, aIndices.Allocate(wIndexCount), &wIndexCount);
	if (hr != S_OK) {
		ASSERT(IsMissingWmAttributeResult(hr));
		return false;
	}
	if (wIndexCount == 0)
		return false;

	return true;
}

template<class T, WMT_ATTR_DATATYPE attrTypeT>
bool GetAttributeExT(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData)
{
	// Certain attributes (e.g. WM/StreamTypeInfo, WM/PeakBitrate) can not get read with "IWMHeaderInfo::GetAttributeByName",
	// those attributes are only returned with "IWMHeaderInfo3::GetAttributeByIndexEx".

	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	DWORD dwDataSize;
	WMT_ATTR_DATATYPE attrType;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwDataSize);
	if (hr != S_OK || attrType != attrTypeT || dwDataSize != sizeof nData) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, (BYTE*)&nData, &dwDataSize);
	if (hr != S_OK || attrType != attrTypeT || dwDataSize != sizeof nData) {
		ASSERT(0);
		return false;
	}
	return true;
}

template<class T> bool GetAttributeExT(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData);

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, BOOL &nData)
{
	return GetAttributeExT<BOOL, WMT_TYPE_BOOL>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, DWORD &nData)
{
	return GetAttributeExT<DWORD, WMT_TYPE_DWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, QWORD &nData)
{
	return GetAttributeExT<QWORD, WMT_TYPE_QWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CTempBuffer<BYTE> &aData, DWORD &dwDataSize)
{
	// Certain attributes (e.g. WM/StreamTypeInfo, WM/PeakBitrate) can not get read with "IWMHeaderInfo::GetAttributeByName",
	// those attributes are only returned with "IWMHeaderInfo3::GetAttributeByIndexEx".

	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	WMT_ATTR_DATATYPE attrType;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwDataSize);
	if (hr != S_OK || attrType != WMT_TYPE_BINARY) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	if (dwDataSize == 0)
		return false;

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, aData.Allocate(dwDataSize), &dwDataSize);
	if (hr != S_OK || attrType != WMT_TYPE_BINARY) {
		ASSERT(0);
		return false;
	}
	return true;
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CStringW &strValue)
{
	strValue.Empty();
	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	WMT_ATTR_DATATYPE attrType;
	DWORD dwValueSize;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwValueSize);
	if (hr != S_OK || attrType != WMT_TYPE_STRING || dwValueSize < sizeof(WCHAR) || (dwValueSize % sizeof(WCHAR)) != 0) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	// empty string?
	if (dwValueSize == sizeof(WCHAR))
		return false;

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, (BYTE*)strValue.GetBuffer((int)(dwValueSize / sizeof(WCHAR))), &dwValueSize);
	strValue.ReleaseBuffer();
	if (hr != S_OK || attrType != WMT_TYPE_STRING) {
		ASSERT(0);
		return false;
	}

	if (strValue.IsEmpty())
		return false;

	// SDK states that MP3 files could contain a BOM - never seen
	if (strValue[0] == u'\xFFFE' || strValue[0] == u'\xFEFF') {
		ASSERT(0);
		strValue.Delete(0, 1);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////
// CFileStream - Implements IStream interface on a file

class CFileStream : public IStream
{
public:
	static HRESULT OpenFile(LPCTSTR pszFileName
			, IStream **ppStream
			, DWORD dwDesiredAccess = GENERIC_READ
			, DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE
			, DWORD dwCreationDisposition = OPEN_EXISTING
			, DWORD grfMode = STGM_READ | STGM_SHARE_DENY_NONE)
	{
		HANDLE hFile = ::CreateFile(pszFileName, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return HRESULT_FROM_WIN32(::GetLastError());
		CFileStream *pFileStream = new CFileStream(hFile, grfMode);
		return pFileStream->QueryInterface(__uuidof(*ppStream), (void**)ppStream);
	}

	///////////////////////////////////////////////////////////////////////////
	// IUnknown

	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject)
	{
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IStream) || iid == __uuidof(ISequentialStream)) {
			*ppvObject = static_cast<IStream*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return static_cast<ULONG>(::InterlockedIncrement(&m_lRefCount));
	}

	STDMETHODIMP_(ULONG) Release()
	{
		LONG lRefCount = ::InterlockedDecrement(&m_lRefCount);
		if (lRefCount == 0)
			delete this;
		return static_cast<ULONG>(lRefCount);
	}

	///////////////////////////////////////////////////////////////////////////
	// ISequentialStream

	STDMETHODIMP Read(void *pv, ULONG cb, ULONG *pcbRead)
	{
		// If the stream was opened for 'write-only', no read access is allowed.
		if ((m_grfMode & 3) == STGM_WRITE) {
			ASSERT(0);
			return E_FAIL;
		}

		if (!::ReadFile(m_hFile, pv, cb, pcbRead, NULL))
			return HRESULT_FROM_WIN32(::GetLastError());

		// The specification of the 'IStream' interface allows to indicate an
		// end-of-stream condition by returning:
		//
		//	HRESULT		*pcbRead
		//  ------------------------
		//	S_OK		<less-than> 'cb'
		//	S_FALSE		<not used>
		//	E_xxx		<not used>
		//
		// If that object is used by the 'WMSyncReader', it seems to be better to
		// return an error code instead of 'S_OK'.
		//
		if (cb != 0 && *pcbRead == 0)
			return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

		return S_OK;
	}

	STDMETHODIMP Write(void const *pv, ULONG cb, ULONG *pcbWritten)
	{
		// If the stream was opened for 'read-only', no write access is allowed.
		if ((m_grfMode & 3) == STGM_READ) {
			ASSERT(0);
			return E_FAIL;
		}

		ULONG cbWritten;
		if (!::WriteFile(m_hFile, pv, cb, &cbWritten, NULL))
			return HRESULT_FROM_WIN32(::GetLastError());

		if (pcbWritten != NULL)
			*pcbWritten = cbWritten;

		return S_OK;
	}

	///////////////////////////////////////////////////////////////////////////
	// IStream

	STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
	{
		// 'dwOrigin' can get mapped to 'dwMoveMethod'
		ASSERT(STREAM_SEEK_SET == FILE_BEGIN);
		ASSERT(STREAM_SEEK_CUR == FILE_CURRENT);
		ASSERT(STREAM_SEEK_END == FILE_END);

		LONG lNewFilePointerHi = dlibMove.HighPart;
		DWORD dwNewFilePointerLo = SetFilePointer(m_hFile, dlibMove.LowPart, &lNewFilePointerHi, dwOrigin);
		if (dwNewFilePointerLo == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR)
			return HRESULT_FROM_WIN32(::GetLastError());

		if (plibNewPosition != NULL) {
			plibNewPosition->HighPart = lNewFilePointerHi;
			plibNewPosition->LowPart = dwNewFilePointerLo;
		}

		return S_OK;
	}

	STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libNewSize);
		return E_NOTIMPL;
	}

	STDMETHODIMP CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(pstm);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(pcbRead);
		UNREFERENCED_PARAMETER(pcbWritten);
		return E_NOTIMPL;
	}

	STDMETHODIMP Commit(DWORD grfCommitFlags)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(grfCommitFlags);
		return E_NOTIMPL;
	}

	STDMETHODIMP Revert()
	{
		ASSERT(0);
		return E_NOTIMPL;
	}

	STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libOffset);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(dwLockType);
		return E_NOTIMPL;
	}

	STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libOffset);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(dwLockType);
		return E_NOTIMPL;
	}

	STDMETHODIMP Stat(STATSTG *pstatstg, DWORD grfStatFlag)
	{
		UNREFERENCED_PARAMETER(grfStatFlag);
		ASSERT(grfStatFlag == STATFLAG_NONAME);
		memset(pstatstg, 0, sizeof *pstatstg);
		BY_HANDLE_FILE_INFORMATION fileInfo;
		if (!GetFileInformationByHandle(m_hFile, &fileInfo))
			return HRESULT_FROM_WIN32(::GetLastError());
		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize.HighPart = fileInfo.nFileSizeHigh;
		pstatstg->cbSize.LowPart = fileInfo.nFileSizeLow;
		pstatstg->mtime = fileInfo.ftLastWriteTime;
		pstatstg->ctime = fileInfo.ftCreationTime;
		pstatstg->atime = fileInfo.ftLastAccessTime;
		pstatstg->grfMode = m_grfMode;
		return S_OK;
	}

	STDMETHODIMP Clone(IStream **ppstm)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(ppstm);
		return E_NOTIMPL;
	}

private:
	CFileStream(HANDLE hFile, DWORD grfMode)
	{
		m_lRefCount = 0;
		m_hFile = hFile;
		m_grfMode = grfMode;
	}

	~CFileStream()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
			::CloseHandle(m_hFile);
	}

	HANDLE m_hFile;
	DWORD m_grfMode;
	LONG m_lRefCount;
};

class CWmvCoreDLL
{
public:
	CWmvCoreDLL()
	{
		m_bInitialized = false;
		m_hLib = NULL;
		m_pfnWMCreateEditor = NULL;
		m_pfnWMCreateSyncReader = NULL;
	}
	~CWmvCoreDLL()
	{
		if (m_hLib)
			::FreeLibrary(m_hLib);
	}

	bool Initialize()
	{
		if (!m_bInitialized) {
			m_bInitialized = true;

			// WM metadata support depends on the runtime WMVCORE component being present.
			m_hLib = ::LoadLibrary(_T("wmvcore.dll"));
			if (m_hLib != NULL) {
				(FARPROC &)m_pfnWMCreateEditor = ::GetProcAddress(m_hLib, "WMCreateEditor");
				(FARPROC &)m_pfnWMCreateSyncReader = ::GetProcAddress(m_hLib, "WMCreateSyncReader");
			}
		}
		return m_pfnWMCreateEditor != NULL;
	}

	bool m_bInitialized;
	HMODULE m_hLib;
	HRESULT(STDMETHODCALLTYPE *m_pfnWMCreateEditor)(IWMMetadataEditor **ppEditor);
	HRESULT(STDMETHODCALLTYPE *m_pfnWMCreateSyncReader)(IUnknown *pUnkCert, DWORD dwRights, IWMSyncReader **ppSyncReader);
};
static CWmvCoreDLL theWmvCoreDLL;

struct SWmCodecInfo
{
	WMT_CODEC_INFO_TYPE codecType;
	DWORD dwCodecId;
	CString strName;
	CString strDesc;
	bool bHasCodecId;
};

static void GetWmCodecInfos(IUnknown *pIUnkReader, bool bNeedText, CArray<SWmCodecInfo, const SWmCodecInfo&> &raCodecInfos)
{
	raCodecInfos.RemoveAll();

	CComQIPtr<IWMHeaderInfo2> pIWMHeaderInfo2(pIUnkReader);
	if (!pIWMHeaderInfo2)
		return;

	DWORD dwCodecInfos = 0;
	if (pIWMHeaderInfo2->GetCodecInfoCount(&dwCodecInfos) != S_OK)
		return;

	for (DWORD dwCodec = 0; dwCodec < dwCodecInfos; ++dwCodec) {
		WORD wNameSize = 0;
		WORD wDescSize = 0;
		WORD wCodecInfoSize = 0;
		WMT_CODEC_INFO_TYPE codecType = WMT_CODECINFO_UNKNOWN;
		if (pIWMHeaderInfo2->GetCodecInfo(dwCodec, &wNameSize, NULL, &wDescSize, NULL, &codecType, &wCodecInfoSize, NULL) != S_OK)
			continue;

		CString strName;
		CString strDesc;
		CTempBuffer<BYTE> aCodecInfo;
		if (pIWMHeaderInfo2->GetCodecInfo(dwCodec
			, &wNameSize, bNeedText ? strName.GetBuffer(wNameSize) : NULL
			, &wDescSize, bNeedText ? strDesc.GetBuffer(wDescSize) : NULL
			, &codecType, &wCodecInfoSize, aCodecInfo.Allocate(wCodecInfoSize)) != S_OK)
		{
			if (bNeedText) {
				strName.ReleaseBuffer();
				strDesc.ReleaseBuffer();
			}
			continue;
		}

		if (bNeedText) {
			strName.ReleaseBuffer();
			strName.Trim();
			strDesc.ReleaseBuffer();
			strDesc.Trim();
		}

		SWmCodecInfo codecInfo = {};
		codecInfo.codecType = codecType;
		codecInfo.dwCodecId = _UI32_MAX;
		codecInfo.strName = strName;
		codecInfo.strDesc = strDesc;
		codecInfo.bHasCodecId = false;
		if (codecType == WMT_CODECINFO_AUDIO && wCodecInfoSize == sizeof(WORD)) {
			codecInfo.dwCodecId = *reinterpret_cast<WORD*>(static_cast<BYTE*>(aCodecInfo));
			codecInfo.bHasCodecId = true;
		} else if (codecType == WMT_CODECINFO_VIDEO && wCodecInfoSize == sizeof(DWORD)) {
			codecInfo.dwCodecId = *reinterpret_cast<DWORD*>(static_cast<BYTE*>(aCodecInfo));
			codecInfo.bHasCodecId = true;
		}
		raCodecInfos.Add(codecInfo);
	}
}

static const SWmCodecInfo* FindWmCodecInfo(const CArray<SWmCodecInfo, const SWmCodecInfo&> &raCodecInfos, WMT_CODEC_INFO_TYPE codecType, UINT uStreamOrdinal, DWORD dwPreferredCodecId = _UI32_MAX)
{
	const SWmCodecInfo *pOrdinalMatch = NULL;
	const SWmCodecInfo *pOnlyMatch = NULL;
	UINT uCodecOrdinal = 0;
	UINT uMatches = 0;

	for (INT_PTR i = 0; i < raCodecInfos.GetCount(); ++i) {
		const SWmCodecInfo &codecInfo = raCodecInfos[i];
		if (codecInfo.codecType != codecType)
			continue;

		++uMatches;
		if (pOnlyMatch == NULL)
			pOnlyMatch = &codecInfo;
		if (codecInfo.bHasCodecId && dwPreferredCodecId != _UI32_MAX && codecInfo.dwCodecId == dwPreferredCodecId)
			return &codecInfo;
		if (uCodecOrdinal == uStreamOrdinal)
			pOrdinalMatch = &codecInfo;
		++uCodecOrdinal;
	}

	if (pOrdinalMatch != NULL)
		return pOrdinalMatch;
	if (uMatches == 1)
		return pOnlyMatch;
	return NULL;
}


bool GetWMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsWM, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	if (!theWmvCoreDLL.Initialize())
		return false;

	CComPtr<IUnknown> pIUnkReader;
	try {
		HRESULT hr = E_FAIL;

		// 1st, try to read the file with the 'WMEditor'. This object tends to give more (stream) information than the 'WMSyncReader'.
		// Though the 'WMEditor' cannot read files that are open for 'writing'.
		if (pIUnkReader == NULL) {
			CComPtr<IWMMetadataEditor> pIWMMetadataEditor;
			if (theWmvCoreDLL.m_pfnWMCreateEditor != NULL && (hr = (*theWmvCoreDLL.m_pfnWMCreateEditor)(&pIWMMetadataEditor)) == S_OK) {
				CComQIPtr<IWMMetadataEditor2> pIWMMetadataEditor2(pIWMMetadataEditor);
				if (pIWMMetadataEditor2 && (hr = pIWMMetadataEditor2->OpenEx(pszFileName, GENERIC_READ, FILE_SHARE_READ)) == S_OK)
					pIUnkReader = pIWMMetadataEditor2;
				//This Open() call unpacks files compressed with "compact /exe:lzx"; assumed to be obsolete
				//else if ((hr = pIWMMetadataEditor->Open(pszFileName)) == S_OK)
				//	pIUnkReader = pIWMMetadataEditor;
			}
		}

		// 2nd, try to read the file with 'WMSyncReader'. This may give less (stream) information than using the 'WMEditor',
		// but it has at least the advantage that one can provide the file data via an 'IStream' - which is needed for
		// reading files which are currently opened for 'writing'.
		//
		// However, the 'WMSyncReader' may take a noticeable amount of time to parse a few bytes of a stream. E.g. reading
		// the short MP3 test files from ID3Lib takes suspicious long time. So, invoke that 'WMSyncReader' only if we know
		// that the file could not get opened due to that it is currently opened for 'writing'.
		//
		// Note also: If the file is DRM protected, 'IWMSyncReader' may return an 'NS_E_PROTECTED_CONTENT' error. This makes
		// sense, because 'IWMSyncReader' does not know that we want to open the file for reading the meta data only.
		// This error code could get used to indicate 'protected' files.
		//
		if (pIUnkReader == NULL && hr == NS_E_FILE_OPEN_FAILED) {
			CComPtr<IWMSyncReader> pIWMSyncReader;
			if (theWmvCoreDLL.m_pfnWMCreateSyncReader != NULL && (hr = (*theWmvCoreDLL.m_pfnWMCreateSyncReader)(NULL, 0, &pIWMSyncReader)) == S_OK) {
				CComPtr<IStream> pIStream;
				if (CFileStream::OpenFile(pszFileName, &pIStream) == S_OK) {
					if ((hr = pIWMSyncReader->OpenStream(pIStream)) == S_OK)
						pIUnkReader = pIWMSyncReader;
				}
			}
		} else
			ASSERT(hr == S_OK
				|| hr == NS_E_UNRECOGNIZED_STREAM_TYPE	// general: unknown file type
				|| hr == NS_E_INVALID_INPUT_FORMAT		// general: unknown file type
				|| hr == NS_E_INVALID_DATA				// general: unknown file type
				|| hr == NS_E_FILE_INIT_FAILED			// got for an SWF file?
				|| hr == NS_E_FILE_READ					// obviously if the file is too short
				);

		if (pIUnkReader) {
			CComQIPtr<IWMHeaderInfo> pIWMHeaderInfo(pIUnkReader);
			if (pIWMHeaderInfo) {
				// IWMHeaderInfo3 can expose additional stream properties, but the
				// presence of the interface does not guarantee WM/StreamTypeInfo.
				CComQIPtr<IWMHeaderInfo3> pIWMHeaderInfo3(pIUnkReader);
				CArray<SWmCodecInfo, const SWmCodecInfo&> aCodecInfos;
				GetWmCodecInfos(pIUnkReader, bFullInfo, aCodecInfos);

				WORD wStream = 0; // 0 = file level, 0xffff = all attributes from file and all streams

				DWORD dwContainer = _UI32_MAX;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMContainerFormat, dwContainer)) {
					if (dwContainer == WMT_Storage_Format_MP3) {
						// The file is either a real MP3 file or a WAV file with an embedded MP3 stream.
						//

						// NOTE: The detection for MP3 is *NOT* safe. There are some MKV test files which
						// are reported as MP3 files although they contain more than just a MP3 stream.
						// This is no surprise, MP3 can not get detected safely in couple of cases.
						//
						// If that function is invoked for getting meta data which is to get published,
						// special care has to be taken for publishing the audio/video codec info.
						// We do not want to announce non-MP3 files as MP3 files by accident.
						//
						if (!bFullInfo) {
							LPCTSTR pszExt = ::PathFindExtension(pszFileName);
							pszExt += static_cast<int>(*pszExt != _T('\0'));
							if (_tcsicmp(pszExt, _T("mp3")) && _tcsicmp(pszExt, _T("mpa")) && _tcsicmp(pszExt, _T("wav")))
								throw new CNotSupportedException();
						}
						mi->strFileFormat = _T("MP3");
					} else if (dwContainer == WMT_Storage_Format_V1) {
						mi->strFileFormat = _T("Windows Media");
						rbIsWM = true;
					} else
						ASSERT(0);
				} else
					ASSERT(0);

				// WMFSDK 7.0 does not support the "WM/ContainerFormat" attribute at all. Though,
				// v7.0 has no importance any longer - it was originally shipped with WinME.
				//
				// If that function is invoked for getting meta data which is to get published,
				// special care has to be taken for publishing the audio/video codec info.
				// e.g. We do not want to announce non-MP3 files as MP3 files by accident.
				//
				if (!bFullInfo && dwContainer == _UI32_MAX) {
					ASSERT(0);
					throw new CNotSupportedException();
				}

				QWORD qwDuration = 0;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMDuration, qwDuration))
					mi->fFileLengthSec = max(qwDuration / 10000000.0, 1.0);
				else
					ASSERT(0);

				CStringW strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMTitle, strValue))
					mi->strTitle = strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMAuthor, strValue))
					mi->strAuthor = strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMAlbumTitle, strValue))
					mi->strAlbum = strValue;

				if (bFullInfo && mi->strInfo.m_hWnd) {
					WORD wAttributes;
					if (pIWMHeaderInfo->GetAttributeCount(wStream, &wAttributes) == S_OK && wAttributes != 0) {
						bool bOutputHeader = true;
						for (WORD wAttr = 0; wAttr < wAttributes; ++wAttr) {
							WCHAR wszName[1024];
							WORD wNameSize = _countof(wszName);
							WORD wDataSize;
							WMT_ATTR_DATATYPE attrType;
							if (pIWMHeaderInfo->GetAttributeByIndex(wAttr, &wStream, wszName, &wNameSize, &attrType, NULL, &wDataSize) == S_OK) {
								if (attrType == WMT_TYPE_STRING) {
									if (GetAttribute(pIWMHeaderInfo, wStream, wszName, strValue)) {
										if (!strValue.Trim().IsEmpty()) {
											if (bOutputHeader) {
												bOutputHeader = false;
												if (!mi->strInfo.IsEmpty())
													mi->strInfo << _T("\n");
												mi->OutputFileName();
												mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
												mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
											}
											CString strName(wszName);
											if (strName.Left(3).Compare(_T("WM/")) == 0)
												strName.Delete(0, 3);
											mi->strInfo << _T("   ") << strName << _T(":\t") << strValue << _T("\n");
										}
									}
								} else if (attrType == WMT_TYPE_BOOL) {
									BOOL bValue;
									if (GetAttribute(pIWMHeaderInfo, wStream, wszName, bValue) && bValue) {
										if (bOutputHeader) {
											bOutputHeader = false;
											if (!mi->strInfo.IsEmpty())
												mi->strInfo << _T("\n");
											mi->OutputFileName();
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
											mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
										}
										CString strName(wszName);
										if (strName.Left(3).Compare(_T("WM/")) == 0)
											strName.Delete(0, 3);

										bool bWarnInRed = wcscmp(wszName, g_wszWMProtected) == 0;
										if (bWarnInRed)
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfRed);
										mi->strInfo << _T("   ") << strName << _T(":\t") << GetResString(IDS_YES) << _T("\n");
										if (bWarnInRed)
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfDef);
									}
								}
							}
						}
					}
				}

				while (wStream < 0x7F) {
					// Check if we reached the end of streams
					WORD wAttributes;
					if (pIWMHeaderInfo3) {
						if (pIWMHeaderInfo3->GetAttributeCountEx(wStream, &wAttributes) != S_OK)
							break;
					} else if (pIWMHeaderInfo->GetAttributeCount(wStream, &wAttributes) != S_OK)
						break;

					if (bFullInfo && mi->strAudioLanguage.IsEmpty()) {
						CString strLanguage;
						if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMLanguage, strLanguage))
							mi->strAudioLanguage = strLanguage;
					}

					bool bHaveStreamInfo = false;
					if (pIWMHeaderInfo3) {
						// WM/StreamTypeInfo is not guaranteed even when IWMHeaderInfo3 is available.
						CTempBuffer<BYTE> aStreamInfo;
						DWORD dwStreamInfoSize;
						if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMStreamTypeInfo, aStreamInfo, dwStreamInfoSize) && dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO)) {
							const WM_STREAM_TYPE_INFO *pStreamTypeInfo = (WM_STREAM_TYPE_INFO*)(BYTE*)aStreamInfo;
							if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Video) {
								bHaveStreamInfo = true;
								++mi->iVideoStreams;
								if (dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WMVIDEOINFOHEADER)
									&& pStreamTypeInfo->cbFormat >= sizeof(WMVIDEOINFOHEADER))
								{
									const WMVIDEOINFOHEADER *pVideoInfo = (WMVIDEOINFOHEADER*)(pStreamTypeInfo + 1);
									ASSERT(sizeof(VIDEOINFOHEADER) == sizeof(WMVIDEOINFOHEADER));
									if (pVideoInfo->bmiHeader.biSize >= sizeof pVideoInfo->bmiHeader) {
										if (mi->iVideoStreams == 1) {
											mi->video = *(VIDEOINFOHEADER*)pVideoInfo;
											if (mi->video.bmiHeader.biWidth && mi->video.bmiHeader.biHeight)
												mi->fVideoAspectRatio = fabs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
											mi->strVideoFormat = GetVideoFormatName(pVideoInfo->bmiHeader.biCompression);

											if (pVideoInfo->bmiHeader.biCompression == MAKEFOURCC('D', 'V', 'R', ' ')
												&& pVideoInfo->bmiHeader.biSize >= sizeof(pVideoInfo->bmiHeader) + sizeof(WMMPEG2VIDEOINFO)
												&& dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WMVIDEOINFOHEADER) + sizeof(WMMPEG2VIDEOINFO)
												&& pStreamTypeInfo->cbFormat >= sizeof(WMVIDEOINFOHEADER) + sizeof(WMMPEG2VIDEOINFO))
											{
												const WMMPEG2VIDEOINFO *pMPEG2VideoInfo = (WMMPEG2VIDEOINFO*)(pVideoInfo + 1);
												if (pMPEG2VideoInfo->hdr.bmiHeader.biSize >= sizeof pMPEG2VideoInfo->hdr.bmiHeader
													&& pMPEG2VideoInfo->hdr.bmiHeader.biCompression != 0
													&& pMPEG2VideoInfo->hdr.bmiHeader.biWidth == pVideoInfo->bmiHeader.biWidth
													&& pMPEG2VideoInfo->hdr.bmiHeader.biHeight == pVideoInfo->bmiHeader.biHeight)
												{
													if (!IsRectEmpty(&pMPEG2VideoInfo->hdr.rcSource))
														mi->video.rcSource = pMPEG2VideoInfo->hdr.rcSource;
													if (!IsRectEmpty(&pMPEG2VideoInfo->hdr.rcTarget))
														mi->video.rcTarget = pMPEG2VideoInfo->hdr.rcTarget;
													if (pMPEG2VideoInfo->hdr.dwBitRate)
														mi->video.dwBitRate = pMPEG2VideoInfo->hdr.dwBitRate;
													if (pMPEG2VideoInfo->hdr.dwBitErrorRate)
														mi->video.dwBitErrorRate = pMPEG2VideoInfo->hdr.dwBitErrorRate;
													if (pMPEG2VideoInfo->hdr.AvgTimePerFrame)
														mi->video.AvgTimePerFrame = pMPEG2VideoInfo->hdr.AvgTimePerFrame;
													mi->video.bmiHeader = pMPEG2VideoInfo->hdr.bmiHeader;
													mi->strVideoFormat = GetVideoFormatName(mi->video.bmiHeader.biCompression);
													if (pMPEG2VideoInfo->hdr.dwPictAspectRatioX != 0 && pMPEG2VideoInfo->hdr.dwPictAspectRatioY != 0)
														mi->fVideoAspectRatio = pMPEG2VideoInfo->hdr.dwPictAspectRatioX / (double)pMPEG2VideoInfo->hdr.dwPictAspectRatioY;
													else if (mi->video.bmiHeader.biWidth && mi->video.bmiHeader.biHeight)
														mi->fVideoAspectRatio = fabs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
												}
											}
											if (mi->fVideoFrameRate == 0.0 && mi->video.AvgTimePerFrame)
												mi->fVideoFrameRate = 1.0 / (mi->video.AvgTimePerFrame / 10000000.0);
										} else if (bFullInfo && mi->strInfo.m_hWnd) {
											mi->OutputFileName();
											if (!mi->strInfo.IsEmpty())
												mi->strInfo << _T("\n");
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
											mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");

											mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(pVideoInfo->bmiHeader.biCompression) << _T("\n");
											if (pVideoInfo->dwBitRate)
												mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((pVideoInfo->dwBitRate + 500) / 1000) << _T(" kbit/s\n");
											mi->strInfo << _T("   ") << GetResString(IDS_WIDTH) << _T(" x ") << GetResString(IDS_HEIGHT) << _T(":\t") << abs(pVideoInfo->bmiHeader.biWidth) << _T(" x ") << abs(pVideoInfo->bmiHeader.biHeight) << _T("\n");
											float fAspectRatio = fabsf(pVideoInfo->bmiHeader.biWidth / (float)pVideoInfo->bmiHeader.biHeight);
											mi->strInfo << _T("   ") << GetResString(IDS_ASPECTRATIO) << _T(":\t") << fAspectRatio << _T("  (") << GetKnownAspectRatioDisplayString(fAspectRatio) << _T(")\n");

											if (pVideoInfo->AvgTimePerFrame) {
												float fFrameRate = 1.0f / (pVideoInfo->AvgTimePerFrame / 10000000.0f);
												mi->strInfo << _T("   ") << GetResString(IDS_FPS) << _T(":\t") << fFrameRate << ("\n");
											}
										}
									}
								}
								if (mi->iVideoStreams == 1) {
									if (mi->video.dwBitRate == 0) {
										DWORD dwValue;
										if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue))
											mi->video.dwBitRate = dwValue;
									}
								}
							} else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Audio) {
								bHaveStreamInfo = true;
								++mi->iAudioStreams;
								if (dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WAVEFORMATEX)
									&& pStreamTypeInfo->cbFormat >= sizeof(WAVEFORMATEX))
								{
									const WAVEFORMATEX *pWaveFormatEx = (WAVEFORMATEX*)(pStreamTypeInfo + 1);
									ASSERT(sizeof(WAVEFORMAT) == sizeof(WAVEFORMATEX) - sizeof(WORD) * 2);
									if (mi->iAudioStreams == 1) {
										mi->audio = *(const WAVEFORMAT*)pWaveFormatEx;
										mi->strAudioFormat = GetAudioFormatName(pWaveFormatEx->wFormatTag);
									} else if (bFullInfo && mi->strInfo.m_hWnd) {
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->OutputFileName();
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName(pWaveFormatEx->wFormatTag) << _T("\n");

										if (pWaveFormatEx->nAvgBytesPerSec) {
											CString strBitrate;
											if (pWaveFormatEx->nAvgBytesPerSec == _UI32_MAX)
												strBitrate = _T("Variable");
											else
												strBitrate.Format(_T("%u %s"), (UINT)((pWaveFormatEx->nAvgBytesPerSec * 8 + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
											mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
										}

										if (pWaveFormatEx->nChannels) {
											mi->strInfo << _T("   ") << GetResString(IDS_CHANNELS) << _T(":\t");
											if (pWaveFormatEx->nChannels == 1)
												mi->strInfo << _T("1 (Mono)");
											else if (pWaveFormatEx->nChannels == 2)
												mi->strInfo << _T("2 (Stereo)");
											else if (pWaveFormatEx->nChannels == 5)
												mi->strInfo << _T("5.1 (Surround)");
											else
												mi->strInfo << pWaveFormatEx->nChannels;
											mi->strInfo << _T("\n");
										}

										if (pWaveFormatEx->nSamplesPerSec)
											mi->strInfo << _T("   ") << GetResString(IDS_SAMPLERATE) << _T(":\t") << pWaveFormatEx->nSamplesPerSec / 1000.0 << _T(" kHz\n");

										if (pWaveFormatEx->wBitsPerSample)
											mi->strInfo << _T("   Bit/sample:\t") << pWaveFormatEx->wBitsPerSample << _T(" Bit\n");
									}
								}
								if (mi->iAudioStreams == 1 && mi->audio.nAvgBytesPerSec == 0) {
									DWORD dwValue;
									if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue))
										mi->audio.nAvgBytesPerSec = dwValue / 8;
								}
							} else if (bFullInfo && mi->strInfo.m_hWnd) {
								bHaveStreamInfo = true;
								mi->OutputFileName();
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Script)
									mi->strInfo << _T("Script Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Image)
									mi->strInfo << _T("Image Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_FileTransfer)
									mi->strInfo << _T("File Transfer Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Text)
									mi->strInfo << _T("Text Stream #") << (UINT)wStream << _T("\n");
								else
									mi->strInfo << _T("Unknown Stream #") << (UINT)wStream << _T("\n");

								DWORD dwValue;
								if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue) && dwValue != 0)
									mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwValue + 500) / 1000) << _T(" kbit/s\n");
							}
						}
					}

					if (wStream > 0 && !bHaveStreamInfo) {
						WMT_CODEC_INFO_TYPE streamCodecType = WMT_CODECINFO_UNKNOWN;
						CString strStreamType;
						CString strStreamInfo;
						CString strDevTempl;
						if (GetAttribute(pIWMHeaderInfo, wStream, g_wszDeviceConformanceTemplate, strDevTempl)) {
							UINT uStreamType = 0;
							strStreamInfo = strDevTempl + _T(": ");
							if (strDevTempl == _T("L")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("All bit rates");
							} else if (strDevTempl == _T("L1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("64 - 160 kbit/s");
							} else if (strDevTempl == _T("L2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 160 kbit/s");
							} else if (strDevTempl == _T("L3")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 384 kbit/s");
							} else if (strDevTempl == _T("S1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 20 kbit/s");
							} else if (strDevTempl == _T("S2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 20 kbit/s");
							} else if (strDevTempl == _T("M")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("All bit rates");
							} else if (strDevTempl == _T("M1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 384 kbit/s, <= 48 kHz");
							} else if (strDevTempl == _T("M2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 768 kbit/s, <= 96 kHz");
							} else if (strDevTempl == _T("M3")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 1500 kbit/s, <= 96 kHz");
							} else if (strDevTempl == _T("SP@LL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Simple Profile, Low Level, <= 176 x 144, <= 96 kbit/s");
							} else if (strDevTempl == _T("SP@ML")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Simple Profile, Medium Level, <= 352 x 288, <= 384 kbit/s");
							} else if (strDevTempl == _T("MP@LL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, Low Level, <= 352 x 288, 2 Mbit/s");
							} else if (strDevTempl == _T("MP@ML")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, Medium Level, <= 720 x 576, 10 Mbit/s");
							} else if (strDevTempl == _T("MP@HL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, High Level, <= 1920 x 1080, 20 Mbit/s");
							} else if (strDevTempl == _T("CP")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Complex Profile");
							} else if (strDevTempl == _T("I1")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Video Image Level 1, <= 352 x 288, 192 Kbit/s");
							} else if (strDevTempl == _T("I2")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Video Image Level 2, <= 1024 x 768, 384 Kbit/s");
							} else if (strDevTempl == _T("I")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Generic Video Image");
							}
							strStreamType = GetResString(uStreamType);
						}

						DWORD dwCodecId = _UI32_MAX;
						CString strCodecName;
						CString strCodecDesc;
						if (streamCodecType != WMT_CODECINFO_UNKNOWN) {
							UINT uStreamOrdinal = (streamCodecType == WMT_CODECINFO_VIDEO)
								? static_cast<UINT>(mi->iVideoStreams)
								: static_cast<UINT>(mi->iAudioStreams);
							const SWmCodecInfo *pCodecInfo = FindWmCodecInfo(aCodecInfos, streamCodecType, uStreamOrdinal);
							if (pCodecInfo != NULL) {
								if (pCodecInfo->bHasCodecId)
									dwCodecId = pCodecInfo->dwCodecId;
								strCodecName = pCodecInfo->strName;
								strCodecDesc = pCodecInfo->strDesc;
							}
						}

						// Depending on the installed WMFSDK version and depending on the WMFSDK which
						// was used to create the WM file, we still may be missing the stream type info.
						// So, don't bother with printing 'Unknown' info.
						if (!strStreamType.IsEmpty()) {
							DWORD dwBitrate = 0;
							GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwBitrate);

							if (streamCodecType == WMT_CODECINFO_VIDEO) {
								++mi->iVideoStreams;
								if (mi->iVideoStreams == 1) {
									if (dwCodecId != _UI32_MAX) {
										mi->video.bmiHeader.biCompression = dwCodecId;
										mi->strVideoFormat = GetVideoFormatName(dwCodecId);
									}
									mi->video.dwBitRate = dwBitrate;
									if (bFullInfo && mi->strInfo.m_hWnd) {
										mi->OutputFileName();
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");
										if (!strCodecName.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
										if (!strCodecDesc.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
										if (!strStreamInfo.IsEmpty())
											mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									}
								} else if (bFullInfo && mi->strInfo.m_hWnd) {
									mi->OutputFileName();
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");
									if (dwCodecId != _UI32_MAX)
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(dwCodecId) << _T("\n");
									if (!strCodecName.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
									if (!strCodecDesc.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
									if (!strStreamInfo.IsEmpty())
										mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									if (dwBitrate)
										mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" Kbit/s\n");
								}
							} else if (streamCodecType == WMT_CODECINFO_AUDIO) {
								++mi->iAudioStreams;
								if (mi->iAudioStreams == 1) {
									if (dwCodecId != _UI32_MAX) {
										mi->audio.wFormatTag = (WORD)dwCodecId;
										mi->strAudioFormat = GetAudioFormatName((WORD)dwCodecId);
									}
									mi->audio.nAvgBytesPerSec = dwBitrate / 8;
									if (bFullInfo && mi->strInfo.m_hWnd) {
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->OutputFileName();
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
										if (!strCodecName.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
										if (!strCodecDesc.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
										if (!strStreamInfo.IsEmpty())
											mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									}
								} else if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
									if (dwCodecId != _UI32_MAX)
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName((WORD)dwCodecId) << _T("\n");
									if (!strCodecName.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
									if (!strCodecDesc.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
									if (!strStreamInfo.IsEmpty())
										mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									if (dwBitrate)
										mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" Kbit/s\n");
								}
							} else if (bFullInfo && mi->strInfo.m_hWnd) {
								mi->OutputFileName();
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								mi->strInfo << _T("Stream #") << (UINT)wStream << _T(": ") << strStreamType << _T("\n");
								if (!strStreamInfo.IsEmpty())
									mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
								if (dwBitrate)
									mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" Kbit/s\n");
							}
						}
					}

					++wStream;
				}

				// 'IWMHeaderInfo3' may not have returned any 'WM/StreamTypeInfo' attributes. If the WM file
				// indicates the existence of Audio/Video streams, try to query for the codec info.
				//
				if (mi->iAudioStreams == 0 && mi->iVideoStreams == 0) {
					BOOL bHasAudio = FALSE;
					GetAttribute(pIWMHeaderInfo, 0, g_wszWMHasAudio, bHasAudio);
					BOOL bHasVideo = FALSE;
					GetAttribute(pIWMHeaderInfo, 0, g_wszWMHasVideo, bHasVideo);
					if (bHasAudio || bHasVideo) {
						CComQIPtr<IWMHeaderInfo2> pIWMHeaderInfo2(pIUnkReader);
						if (pIWMHeaderInfo2) {
							DWORD dwCodecInfos;
							if ((hr = pIWMHeaderInfo2->GetCodecInfoCount(&dwCodecInfos)) == S_OK) {
								bool bAddedBakedAudioStream = false;
								bool bAddedBakedVideoStream = false;
								for (DWORD dwCodec = 0; dwCodec < dwCodecInfos; ++dwCodec) {
									WORD wNameSize;
									WORD wDescSize;
									WORD wCodecInfoSize;
									WMT_CODEC_INFO_TYPE codecType;
									hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec, &wNameSize, NULL, &wDescSize, NULL, &codecType, &wCodecInfoSize, NULL);
									if (hr == S_OK) {
										CString strName;
										CString strDesc;
										CTempBuffer<BYTE> aCodecInfo;
										hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec
											, &wNameSize, bFullInfo ? strName.GetBuffer(wNameSize) : NULL
											, &wDescSize, bFullInfo ? strDesc.GetBuffer(wDescSize) : NULL
											, &codecType, &wCodecInfoSize, aCodecInfo.Allocate(wCodecInfoSize));
										strName.ReleaseBuffer();
										strName.Trim();
										strDesc.ReleaseBuffer();
										strDesc.Trim();
										if (hr == S_OK) {
											if (bHasAudio && codecType == WMT_CODECINFO_AUDIO) {
												if (wCodecInfoSize == sizeof(WORD)) {
													if (!bAddedBakedAudioStream) {
														bAddedBakedAudioStream = true;
														++mi->iAudioStreams;
														mi->audio.wFormatTag = *(WORD*)(BYTE*)aCodecInfo;
														mi->strAudioFormat = GetAudioFormatName(mi->audio.wFormatTag);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_AUDIO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else if (wCodecInfoSize == 0 && dwContainer == WMT_Storage_Format_MP3) {
													// MP3 files: no codec info returned
													if (!bAddedBakedAudioStream) {
														bAddedBakedAudioStream = true;
														++mi->iAudioStreams;
														mi->audio.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
														mi->strAudioFormat = GetAudioFormatName(mi->audio.wFormatTag);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_AUDIO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else
													ASSERT(0);
											} else if (bHasVideo && codecType == WMT_CODECINFO_VIDEO) {
												if (wCodecInfoSize == sizeof(DWORD)) {
													if (!bAddedBakedVideoStream) {
														bAddedBakedVideoStream = true;
														++mi->iVideoStreams;
														mi->video.bmiHeader.biCompression = *(DWORD*)(BYTE*)aCodecInfo;
														mi->strVideoFormat = GetVideoFormatName(mi->video.bmiHeader.biCompression);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_VIDEO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else
													ASSERT(0);
											} else
												ASSERT(0);
										}
									}
								}
							}
						}
					}
				}

				if (dwContainer == WMT_Storage_Format_V1) {
					if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && (mi->audio.nAvgBytesPerSec == 0 || mi->audio.nAvgBytesPerSec == _UI32_MAX)) {
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->audio.nAvgBytesPerSec = dwCurrentBitrate / 8;
					} else if (mi->iAudioStreams == 0 && mi->iVideoStreams == 1 && (mi->video.dwBitRate == 0 || mi->video.dwBitRate == _UI32_MAX)) {
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->video.dwBitRate = dwCurrentBitrate;
					}
				} else if (dwContainer == WMT_Storage_Format_MP3) {
					if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && (mi->audio.nAvgBytesPerSec == 0 || mi->audio.nAvgBytesPerSec == _UI32_MAX)) {
						// CBR MP3 stream: The average bit rate is equal to the nominal bit rate
						//
						// VBR MP3 stream: The average bit rate is usually not equal to the nominal
						// bit rate (sometimes even quite way off). However, the average bit rate is
						// always a reasonable value, thus it is the preferred value for the bit rate.
						//
						// MP3 streams which are enveloped in a WAV file: some WM bit rates are simply 0!
						//
						// Conclusion: For MP3 files always prefer the average bit rate, if available.
						//
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->audio.nAvgBytesPerSec = dwCurrentBitrate / 8;
					}
				}
			}
		}
	} catch (CException *ex) {
		ASSERT(ex->IsKindOf(RUNTIME_CLASS(CNotSupportedException)));
		ex->Delete();
	}

	if (pIUnkReader) {
		CComQIPtr<IWMMetadataEditor> pIWMMetadataEditor(pIUnkReader);
		if (pIWMMetadataEditor)
			VERIFY(pIWMMetadataEditor->Close() == S_OK);
		else {
			CComQIPtr<IWMSyncReader> pIWMSyncReader(pIUnkReader);
			if (pIWMSyncReader)
				VERIFY(pIWMSyncReader->Close() == S_OK);
		}
	}

	return mi->iAudioStreams > 0
		|| mi->iVideoStreams > 0
		|| mi->fFileLengthSec > 0
		|| !mi->strAlbum.IsEmpty()
		|| !mi->strAuthor.IsEmpty()
		|| !mi->strTitle.IsEmpty();
}

#else//HAVE_WMSDK_H

bool GetWMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsWM, bool bFullInfo)
{
	UNREFERENCED_PARAMETER(pszFileName);
	UNREFERENCED_PARAMETER(mi);
	UNREFERENCED_PARAMETER(rbIsWM);
	UNREFERENCED_PARAMETER(bFullInfo);
	return false;
}

#endif//HAVE_WMSDK_H
