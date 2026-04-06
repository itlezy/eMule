#pragma once

#include <windows.h>

/**
 * @brief Converts a LOGFONT height into a point size using the supplied vertical DPI.
 */
inline int GetLogFontPointSize(int nLogFontHeight, int nLogPixelsY)
{
	return nLogPixelsY > 0 ? -::MulDiv(nLogFontHeight, 72, nLogPixelsY) : 0;
}
