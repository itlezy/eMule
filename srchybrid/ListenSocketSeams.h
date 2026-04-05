#pragma once

#include <tchar.h>

/**
 * @brief Returns the deterministic parse-boundary fallback used when an unknown exception escapes packet handling.
 */
inline LPCTSTR GetListenSocketUnknownPacketExceptionMessage()
{
	return _T("Unknown exception");
}
