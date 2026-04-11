#pragma once

#include <atlstr.h>
#include <windows.h>

namespace PartFileCompletionSeams
{
inline bool ShouldWarnAboutDisabledLongPathSupport(const DWORD dwMoveResult, const CString &rstrDestinationPath, const bool bWin32LongPathsEnabled)
{
	if (bWin32LongPathsEnabled || rstrDestinationPath.GetLength() < MAX_PATH)
		return false;

	switch (dwMoveResult) {
		case ERROR_DIRECTORY:
		case ERROR_FILENAME_EXCED_RANGE:
		case ERROR_INVALID_NAME:
		case ERROR_PATH_NOT_FOUND:
			return true;
		default:
			return false;
	}
}
}
