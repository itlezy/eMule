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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include <dbghelp.h>
#include "LongPathSeams.h"
#include "PathHelpers.h"
#include "mdump.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
										 CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
										 CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
										 CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

CMiniDumper theCrashDumper;
CString CMiniDumper::m_strAppName;
CString CMiniDumper::m_strDumpDir;

void CMiniDumper::Enable(LPCTSTR pszAppName, bool bShowErrors, LPCTSTR pszDumpDir)
{
	// This assert fires if you have two instances of CMiniDumper which is not allowed
	ASSERT(m_strAppName.IsEmpty());
	m_strAppName = (pszAppName != NULL) ? pszAppName : _T("");

	// eMule may not have the permission to create a DMP file in the directory where the "emule.exe" is located.
	// Need to pre-determine a valid directory.
	m_strDumpDir = PathHelpers::EnsureTrailingSeparator((pszDumpDir != NULL) ? CString(pszDumpDir) : CString());

	MINIDUMPWRITEDUMP pfnMiniDumpWriteDump;
	HMODULE hDbgHelpDll = GetDebugHelperDll((FARPROC*)&pfnMiniDumpWriteDump, bShowErrors);
	if (hDbgHelpDll) {
		if (pfnMiniDumpWriteDump)
			::SetUnhandledExceptionFilter(TopLevelFilter);
		::FreeLibrary(hDbgHelpDll);
	}
}

#define DBGHELP_HINT _T("The required DBGHELP.DLL may be obtained from \"Microsoft Download Center\" as a part of \"User Mode Process Dumper\".\r\n\r\n") \
	_T("DBGHELP.DLL should reside in Windows/System32 folder, and also 32-bit DLL in 64-bit OS in Windows/SysWOW64 folder.\r\n") \
	_T("Alternatively, DBGHELP.DLL may be copied to eMule executable's folder (DLL and executable must have the same bitness).")

HMODULE CMiniDumper::GetDebugHelperDll(FARPROC *ppfnMiniDumpWriteDump, bool bShowErrors)
{
	*ppfnMiniDumpWriteDump = NULL;
	HMODULE hDll = ::LoadLibrary(_T("DBGHELP.DLL"));
	if (hDll == NULL) {
		if (bShowErrors)
			// Do *NOT* localize that string (in fact, do not use MFC to load it)!
			MessageBox(NULL, _T("DBGHELP.DLL not found. Please install a DBGHELP.DLL.\r\n\r\n") DBGHELP_HINT, m_strAppName, MB_ICONSTOP | MB_OK);
	} else {
		*ppfnMiniDumpWriteDump = ::GetProcAddress(hDll, "MiniDumpWriteDump");
		if (*ppfnMiniDumpWriteDump == NULL && bShowErrors)
			// Do *NOT* localize that string (in fact, do not use MFC to load it)!
			MessageBox(NULL, _T("DBGHELP.DLL found is too old. Please upgrade to the current version of DBGHELP.DLL.\r\n\r\n") DBGHELP_HINT, m_strAppName, MB_ICONSTOP | MB_OK);
	}
	return hDll;
}

#define CRASHTEXT _T("eMule crashed :-(\r\n\r\n") \
	_T("A diagnostic file can be created which will help the author to resolve this problem.\r\n") \
	_T("This file will be saved on your Disk (and not sent).\r\n\r\n") \
	_T("Do you want to create this file now?")

LONG WINAPI CMiniDumper::TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo) noexcept
{
#ifdef _DEBUG
	LONG lRetValue = EXCEPTION_CONTINUE_SEARCH;
#endif
	MINIDUMPWRITEDUMP pfnMiniDumpWriteDump;
	HMODULE hDll = GetDebugHelperDll((FARPROC*)&pfnMiniDumpWriteDump, true);
	if (hDll) {
		if (pfnMiniDumpWriteDump) {
			SYSTEMTIME t;
			::GetLocalTime(&t); //time of this crash
			// Ask user to confirm writing a dump file
			// Do *NOT* localize that string (in fact, do not use MFC to load it)!
			if (theCrashDumper.uCreateCrashDump == 2 || MessageBox(NULL, CRASHTEXT, m_strAppName, MB_ICONSTOP | MB_YESNO) == IDYES) {
				CString strBaseName;
				strBaseName.Format(_T("%s_%4d%02d%02d-%02d%02d%02d")
					, (LPCTSTR)m_strAppName, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
				// Replace spaces and dots in file name.
				strBaseName.Replace(_T('.'), _T('-'));
				strBaseName.Replace(_T(' '), _T('_'));

				// Create full path for the dump file
				const CString strDumpPath(PathHelpers::AppendPathComponent(m_strDumpDir, strBaseName + _T(".dmp")));

				CString strResult;
				HANDLE hFile = LongPathSeams::CreateFile(strDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile != INVALID_HANDLE_VALUE) {
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo = _MINIDUMP_EXCEPTION_INFORMATION{GetCurrentThreadId(), pExceptionInfo, FALSE};
					BOOL bOK = (*pfnMiniDumpWriteDump)(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
					if (bOK) {
						// Do *NOT* localize this string (in fact, do not use MFC to load it)!
						strResult.Format(
							_T("Saved dump file to \"%s\".\r\n\r\n")
							_T("Please attach this file to a detailed bug report at forum.emule-project.net\r\n\r\n")
							_T("Thank you for helping to improve eMule!"),
							(LPCTSTR)strDumpPath);
#ifdef _DEBUG
						lRetValue = EXCEPTION_EXECUTE_HANDLER;
#endif
					} else {
						// Do *NOT* localize this string (in fact, do not use MFC to load it)!
						strResult.Format(_T("Failed to save dump file to \"%s\".\r\n\r\nError: %lu")
							, (LPCTSTR)strDumpPath, ::GetLastError());
					}
					::CloseHandle(hFile);
				} else {
					// Do *NOT* localize this string (in fact, do not use MFC to load it)!
					strResult.Format(_T("Failed to create dump file \"%s\".\r\n\r\nError: %lu")
						, (LPCTSTR)strDumpPath, ::GetLastError());
				}
				if (!strResult.IsEmpty())
					::MessageBox(NULL, strResult, m_strAppName, MB_ICONINFORMATION | MB_OK);
			}
		}
		::FreeLibrary(hDll);
	}

#ifndef _DEBUG
	// Exit the process only in release builds, so that in debug builds the exception
	// is passed to an installed debugger
	ExitProcess(0);
#else
	return lRetValue;
#endif
}
