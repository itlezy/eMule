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
#pragma once

#include "TaskbarNotifier.h"
#include <memory>

/**
 * @brief Optional Windows-native toast notifier with custom notifier fallback handled by the caller.
 */
class CWindowsToastNotifier
{
public:
	CWindowsToastNotifier();
	~CWindowsToastNotifier();

	/**
	 * @brief Shows one Windows toast and posts UM_WINDOWS_TOAST_CLICKED when it is activated.
	 */
	bool Show(HWND hWndNotify, LPCTSTR pszText, TbnMsg nMsgType, LPCTSTR pszLink);

	/**
	 * @brief Releases toast state before the main window is destroyed.
	 */
	void Shutdown();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
