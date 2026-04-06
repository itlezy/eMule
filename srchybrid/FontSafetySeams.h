#pragma once

#include <tchar.h>

/**
 * @brief Copies a face name into a fixed-size LOGFONT buffer using a bounds-checked destination size.
 */
template <size_t N>
inline errno_t CopyLogFontFaceName(TCHAR (&rFaceName)[N], LPCTSTR pszFaceName)
{
	return _tcscpy_s(rFaceName, N, pszFaceName);
}
