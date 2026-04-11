#pragma once

#include <atlstr.h>

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
}
