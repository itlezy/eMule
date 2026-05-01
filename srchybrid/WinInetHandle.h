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

#include <wininet.h>

namespace WinInetUtil
{
/**
 * @brief Owns one WinInet HINTERNET handle and closes it on scope exit.
 */
class CInternetHandle
{
public:
	CInternetHandle() noexcept = default;
	explicit CInternetHandle(HINTERNET hHandle) noexcept
		: m_hHandle(hHandle)
	{
	}

	CInternetHandle(const CInternetHandle&) = delete;
	CInternetHandle& operator=(const CInternetHandle&) = delete;

	CInternetHandle(CInternetHandle&& rOther) noexcept
		: m_hHandle(rOther.Release())
	{
	}

	CInternetHandle& operator=(CInternetHandle&& rOther) noexcept
	{
		if (this != &rOther)
			Reset(rOther.Release());
		return *this;
	}

	~CInternetHandle()
	{
		Reset();
	}

	/**
	 * @brief Returns true when the wrapper owns a non-null handle.
	 */
	explicit operator bool() const noexcept
	{
		return m_hHandle != NULL;
	}

	/**
	 * @brief Returns the wrapped raw WinInet handle for API calls.
	 */
	HINTERNET Get() const noexcept
	{
		return m_hHandle;
	}

	/**
	 * @brief Replaces the wrapped handle, closing the previous handle first.
	 */
	void Reset(HINTERNET hHandle = NULL) noexcept
	{
		if (m_hHandle != NULL && m_hHandle != hHandle)
			::InternetCloseHandle(m_hHandle);
		m_hHandle = hHandle;
	}

	/**
	 * @brief Releases ownership without closing the wrapped handle.
	 */
	HINTERNET Release() noexcept
	{
		HINTERNET hHandle = m_hHandle;
		m_hHandle = NULL;
		return hHandle;
	}

private:
	HINTERNET m_hHandle = NULL;
};
}
