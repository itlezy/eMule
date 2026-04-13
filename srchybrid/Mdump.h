#pragma once

struct _EXCEPTION_POINTERS;

class CMiniDumper
{
public:
	static void Enable(LPCTSTR pszAppName, bool bShowErrors, LPCTSTR pszDumpDir);
	unsigned uCreateCrashDump; //0 - no dump; 1 - create dump if user agrees; 2 - create without asking
private:
	static CString m_strAppName;
	static CString m_strDumpDir;

	static HMODULE GetDebugHelperDll(FARPROC *ppfnMiniDumpWriteDump, bool bShowErrors);
	static LONG WINAPI TopLevelFilter(struct _EXCEPTION_POINTERS *pExceptionInfo) noexcept;
};

extern CMiniDumper theCrashDumper;
