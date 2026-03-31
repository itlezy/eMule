#include "MappedFileReader.h"

#include <algorithm>

namespace
{
	/** Binds each mapped view to a predictable size instead of mapping the whole file at once. */
	const SIZE_T MAPPED_FILE_WINDOW_BYTES = 8u * 1024u * 1024u;
}

bool VisitMappedFileRange(HANDLE hFile, ULONGLONG nOffset, ULONGLONG nLength, IMappedFileRangeVisitor &rVisitor, DWORD *pdwError)
{
	if (pdwError != NULL)
		*pdwError = ERROR_SUCCESS;

	if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
		if (pdwError != NULL)
			*pdwError = ERROR_INVALID_HANDLE;
		return false;
	}

	if (nLength == 0)
		return true;

	HANDLE hMapping = ::CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hMapping == NULL) {
		if (pdwError != NULL)
			*pdwError = ::GetLastError();
		return false;
	}

	SYSTEM_INFO systemInfo = {};
	::GetSystemInfo(&systemInfo);
	const ULONGLONG nAllocationGranularity = systemInfo.dwAllocationGranularity != 0 ? systemInfo.dwAllocationGranularity : 65536u;

	DWORD dwError = ERROR_SUCCESS;
	bool bSuccess = true;
	ULONGLONG nCurrentOffset = nOffset;
	ULONGLONG nRemaining = nLength;
	while (nRemaining != 0) {
		const ULONGLONG nViewOffset = nCurrentOffset - (nCurrentOffset % nAllocationGranularity);
		const SIZE_T nViewPadding = static_cast<SIZE_T>(nCurrentOffset - nViewOffset);
		const SIZE_T nRequestedBytes = static_cast<SIZE_T>(std::min<ULONGLONG>(nRemaining, MAPPED_FILE_WINDOW_BYTES));
		const SIZE_T nBytesToMap = nRequestedBytes + nViewPadding;

		void *pView = ::MapViewOfFile(hMapping, FILE_MAP_READ
			, static_cast<DWORD>(nViewOffset >> 32)
			, static_cast<DWORD>(nViewOffset & 0xFFFFFFFFu)
			, nBytesToMap);
		if (pView == NULL) {
			dwError = ::GetLastError();
			bSuccess = false;
			break;
		}

		rVisitor.OnMappedFileBytes(reinterpret_cast<const BYTE*>(pView) + nViewPadding, nRequestedBytes);

		if (!::UnmapViewOfFile(pView)) {
			dwError = ::GetLastError();
			bSuccess = false;
			break;
		}

		nCurrentOffset += nRequestedBytes;
		nRemaining -= nRequestedBytes;
	}

	if (!::CloseHandle(hMapping) && bSuccess) {
		dwError = ::GetLastError();
		bSuccess = false;
	}

	if (pdwError != NULL)
		*pdwError = dwError;

	return bSuccess;
}
