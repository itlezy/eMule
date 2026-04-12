#pragma once

#include <atlstr.h>
#include <tchar.h>
#include <windows.h>

namespace OtherFunctionsSeams
{
/**
 * @brief Describes which shell-delete path should be taken for the current request.
 */
enum ShellDeleteRoute
{
	shellDeleteNoOp = 0,
	shellDeleteDirect,
	shellDeleteRecycleBin
};

/**
 * @brief Chooses the runtime delete route from the current existence and recycle-bin settings.
 */
inline ShellDeleteRoute ResolveShellDeleteRoute(const bool bPathExists, const bool bRemoveToBin)
{
	if (!bPathExists)
		return shellDeleteNoOp;

	return bRemoveToBin ? shellDeleteRecycleBin : shellDeleteDirect;
}

/**
 * @brief Removes a Win32 long-path prefix before passing a path to shell parsing APIs.
 */
inline CString PreparePathForShellOperation(const CString &rstrFilePath)
{
	if (rstrFilePath.Left(8).CompareNoCase(_T("\\\\?\\UNC\\")) == 0)
		return CString(_T("\\\\")) + rstrFilePath.Mid(8);
	if (rstrFilePath.Left(4).CompareNoCase(_T("\\\\?\\")) == 0)
		return rstrFilePath.Mid(4);
	return rstrFilePath;
}

/**
 * @brief Runs the shell-delete flow through injected filesystem and recycle-bin callbacks.
 */
template <typename PathExistsFn, typename RecycleDeleteFn, typename DirectDeleteFn>
inline bool ExecuteShellDelete(
	LPCTSTR pszFilePath,
	const bool bRemoveToBin,
	HWND hOwnerWindow,
	PathExistsFn pathExistsFn,
	RecycleDeleteFn recycleDeleteFn,
	DirectDeleteFn directDeleteFn)
{
	const ShellDeleteRoute eRoute = ResolveShellDeleteRoute(pathExistsFn(pszFilePath), bRemoveToBin);
	switch (eRoute) {
		case shellDeleteNoOp:
			return true;
		case shellDeleteRecycleBin:
			return recycleDeleteFn(pszFilePath, hOwnerWindow);
		case shellDeleteDirect:
			return directDeleteFn(pszFilePath);
	}

	return false;
}
}

#define EMULE_TEST_HAVE_OTHER_FUNCTIONS_SEAMS 1
