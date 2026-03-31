#pragma once

#include <atlstr.h>
#include <tchar.h>

namespace StartupConfigOverride
{
	/**
	 * @brief Reports whether the raw command-line token is the supported config-base override option.
	 */
	inline bool IsConfigBaseDirOptionToken(LPCTSTR pszArgument)
	{
		return pszArgument != NULL && (_tcsicmp(pszArgument, _T("-c")) == 0 || _tcsicmp(pszArgument, _T("/c")) == 0);
	}

	/**
	 * @brief Reports whether the supplied path is an absolute Win32 drive or UNC path.
	 */
	inline bool IsAbsoluteBaseDirPath(const CString &strPath)
	{
		return (strPath.GetLength() >= 3
				&& ((strPath[0] >= _T('A') && strPath[0] <= _T('Z')) || (strPath[0] >= _T('a') && strPath[0] <= _T('z')))
				&& strPath[1] == _T(':')
				&& (strPath[2] == _T('\\') || strPath[2] == _T('/')))
			|| (strPath.GetLength() >= 2 && strPath[0] == _T('\\') && strPath[1] == _T('\\'));
	}

	/**
	 * @brief Normalizes the override base directory into the canonical trailing-backslash form used by startup code.
	 */
	inline CString NormalizeBaseDir(const CString &strBaseDir)
	{
		CString strNormalized(strBaseDir);
		strNormalized.Trim();
		strNormalized.Replace(_T('/'), _T('\\'));
		while (strNormalized.GetLength() > 3 && strNormalized.Right(1) == _T("\\"))
			strNormalized.Truncate(strNormalized.GetLength() - 1);
		if (!strNormalized.IsEmpty() && strNormalized.Right(1) != _T("\\"))
			strNormalized.AppendChar(_T('\\'));
		return strNormalized;
	}

	/**
	 * @brief Builds the effective config directory path below the selected base directory.
	 */
	inline CString GetConfigDirectoryFromBaseDir(const CString &strBaseDir)
	{
		return NormalizeBaseDir(strBaseDir) + _T("config\\");
	}

	/**
	 * @brief Builds the effective log directory path below the selected base directory.
	 */
	inline CString GetLogDirectoryFromBaseDir(const CString &strBaseDir)
	{
		return NormalizeBaseDir(strBaseDir) + _T("logs\\");
	}

	/**
	 * @brief Builds the effective `preferences.ini` path below the selected base directory.
	 */
	inline CString GetPreferencesIniPathFromBaseDir(const CString &strBaseDir)
	{
		return GetConfigDirectoryFromBaseDir(strBaseDir) + _T("preferences.ini");
	}

	/**
	 * @brief Parses the supported `-c <base-dir>` override from the raw command line.
	 *
	 * The option may be specified at most once and requires an absolute base directory.
	 */
	inline bool TryParseConfigBaseDirOverride(int argc, TCHAR *argv[], CString &rstrBaseDir, CString &rstrError)
	{
		rstrBaseDir.Empty();
		rstrError.Empty();
		bool bSeenOverride = false;

		for (int i = 1; i < argc; ++i) {
			if (!IsConfigBaseDirOptionToken(argv[i]))
				continue;

			if (bSeenOverride) {
				rstrError = _T("The -c option may be specified only once.");
				return false;
			}
			if (++i >= argc) {
				rstrError = _T("The -c option requires an absolute eMule base directory.");
				return false;
			}

			CString strCandidate(argv[i]);
			strCandidate.Trim();
			if (strCandidate.IsEmpty() || !IsAbsoluteBaseDirPath(strCandidate)) {
				rstrError = _T("The -c option requires an absolute eMule base directory.");
				return false;
			}

			rstrBaseDir = NormalizeBaseDir(strCandidate);
			bSeenOverride = true;
		}
		return true;
	}
}
