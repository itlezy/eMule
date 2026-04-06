#pragma once

#include <windows.h>

/**
 * @brief Computes the popup grid row while rejecting invalid indices and zero-column layouts.
 */
inline int GetColourPopupRow(int nIndex, int nNumColours, int nNumColumns, int nDefaultBoxValue, int nInvalidColourValue)
{
	if (nIndex == nDefaultBoxValue || nIndex == nDefaultBoxValue + 1)
		return nDefaultBoxValue;
	if (nIndex < 0 || nIndex >= nNumColours || nNumColumns <= 0)
		return nInvalidColourValue;
	return nIndex / nNumColumns;
}

/**
 * @brief Computes the popup grid column while rejecting invalid indices and zero-column layouts.
 */
inline int GetColourPopupColumn(int nIndex, int nNumColours, int nNumColumns, int nDefaultBoxValue, int nInvalidColourValue)
{
	if (nIndex == nDefaultBoxValue || nIndex == nDefaultBoxValue + 1)
		return nDefaultBoxValue;
	if (nIndex < 0 || nIndex >= nNumColours || nNumColumns <= 0)
		return nInvalidColourValue;
	return nIndex % nNumColumns;
}

/**
 * @brief Reports whether a popup parent window is still alive before notification.
 */
inline bool HasLivePopupParent(HWND hParentWnd)
{
	return hParentWnd != NULL && ::IsWindow(hParentWnd) != FALSE;
}
