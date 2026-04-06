#pragma once

#include <vector>

#include <windows.h>

/**
 * @brief Loads a whole file through wide-path Win32 IO so non-ASCII Windows paths stay lossless.
 */
inline bool TryLoadWidePathFileBytes(LPCTSTR pszPath, std::vector<unsigned char> &rBytes)
{
	rBytes.clear();
	if (pszPath == NULL || *pszPath == _T('\0'))
		return false;

	HANDLE hFile = ::CreateFile(pszPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	LARGE_INTEGER fileSize = {};
	const BOOL bHasSize = ::GetFileSizeEx(hFile, &fileSize);
	if (!bHasSize || fileSize.QuadPart < 0) {
		::CloseHandle(hFile);
		return false;
	}

	rBytes.resize(static_cast<size_t>(fileSize.QuadPart) + 1u, 0);
	DWORD dwBytesRead = 0;
	const BOOL bReadOk = (fileSize.QuadPart == 0)
		|| ::ReadFile(hFile, rBytes.data(), static_cast<DWORD>(fileSize.QuadPart), &dwBytesRead, NULL);
	::CloseHandle(hFile);

	return bReadOk != FALSE && dwBytesRead == static_cast<DWORD>(fileSize.QuadPart);
}
