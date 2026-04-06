#pragma once

#include <windows.h>

/**
 * @brief Applies a text limit only when the target edit control exists.
 */
template <typename TEditControl>
inline bool SetEditLimitIfPresent(TEditControl *pEditControl, UINT nMaxText)
{
	if (pEditControl == NULL)
		return false;

	pEditControl->SetLimitText(nMaxText);
	return true;
}

/**
 * @brief Hides an optional dialog control only when it exists.
 */
template <typename TWindow>
inline bool HideWindowIfPresent(TWindow *pWindow)
{
	if (pWindow == NULL)
		return false;

	pWindow->ShowWindow(SW_HIDE);
	return true;
}

/**
 * @brief Enables or disables an optional dialog control only when it exists.
 */
template <typename TWindow>
inline bool EnableWindowIfPresent(TWindow *pWindow, BOOL bEnable)
{
	if (pWindow == NULL)
		return false;

	pWindow->EnableWindow(bEnable);
	return true;
}

/**
 * @brief Returns the current window height for an optional dialog control, or zero when missing.
 */
template <typename TWindow>
inline int GetWindowHeightOrZero(TWindow *pWindow)
{
	if (pWindow == NULL)
		return 0;

	RECT rect = {};
	pWindow->GetWindowRect(&rect);
	return rect.bottom - rect.top;
}

/**
 * @brief Applies a vertical splitter-position adjustment only when the control exists.
 */
template <typename TWindow, typename TChangePosFn>
inline bool ChangeWindowPosIfPresent(TWindow *pWindow, int dx, int dy, TChangePosFn pfnChangePos)
{
	if (pWindow == NULL)
		return false;

	pfnChangePos(pWindow, dx, dy);
	return true;
}

/**
 * @brief Applies a height adjustment only when the control exists.
 */
template <typename TWindow, typename TChangeHeightFn>
inline bool ChangeWindowHeightIfPresent(TWindow *pWindow, int nDeltaHeight, TChangeHeightFn pfnChangeHeight)
{
	if (pWindow == NULL)
		return false;

	pfnChangeHeight(pWindow, nDeltaHeight);
	return true;
}
