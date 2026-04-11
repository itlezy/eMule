#pragma once

#include <atlstr.h>

namespace IPFilterSeams
{
enum PathHintType
{
	PathHintUnknown = 0,
	PathHintFilterDat = 1,
	PathHintPeerGuardian = 2
};

inline CString ExtractFileName(const CString &rstrFilePath)
{
	int iSeparator = rstrFilePath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrFilePath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;
	return rstrFilePath.Mid(iSeparator + 1);
}

inline CString ExtractFileExtension(const CString &rstrFilePath)
{
	int iSeparator = rstrFilePath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrFilePath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	const int iDot = rstrFilePath.ReverseFind(_T('.'));
	if (iDot <= iSeparator)
		return CString();
	return rstrFilePath.Mid(iDot);
}

inline PathHintType DetectFileTypeFromPath(const CString &rstrFilePath)
{
	const CString strFileName = ExtractFileName(rstrFilePath);
	const CString strExtension = ExtractFileExtension(strFileName);
	if (strExtension.CompareNoCase(_T(".p2p")) == 0 || strFileName.CompareNoCase(_T("guarding.p2p.txt")) == 0)
		return PathHintPeerGuardian;
	if (strExtension.CompareNoCase(_T(".prefix")) == 0)
		return PathHintFilterDat;
	return PathHintUnknown;
}
}
