#pragma once

#include <atlstr.h>
#include <tchar.h>

#include "LongPathSeams.h"

namespace KnownFileMetadataSeams
{
inline CString BuildMetadataFilePath(const CString &rstrDirectory, const CString &rstrFileName)
{
	CString strFullPath(rstrDirectory);
	if (!strFullPath.IsEmpty() && strFullPath.Right(1) != _T("\\") && strFullPath.Right(1) != _T("/"))
		strFullPath += _T("\\");
	strFullPath += rstrFileName;
	return strFullPath;
}

inline int OpenMetadataReadOnlyDescriptor(LPCTSTR pszFilePath)
{
	return LongPathSeams::OpenCrtReadOnlyLongPath(pszFilePath);
}

/**
 * @brief Returns whether a filename should use the legacy MPEG-audio metadata fallback path.
 */
inline bool IsMpegAudioMetadataExtension(LPCTSTR pszFileName)
{
	if (pszFileName == NULL || *pszFileName == _T('\0'))
		return false;

	LPCTSTR pszExtension = _tcsrchr(pszFileName, _T('.'));
	pszExtension = (pszExtension != NULL && pszExtension[1] != _T('\0')) ? pszExtension + 1 : pszFileName;
	return _tcsicmp(pszExtension, _T("mp3")) == 0
		|| _tcsicmp(pszExtension, _T("mp2")) == 0
		|| _tcsicmp(pszExtension, _T("mp1")) == 0
		|| _tcsicmp(pszExtension, _T("mpa")) == 0;
}
}
