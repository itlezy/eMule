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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//GNU General Public License for more details.

#pragma once

#include "AsyncSocketExSeams.h"

/**
 * Maps the steady-state datagram readiness interest onto the legacy async event mask.
 */
inline long GetAsyncDatagramEventMask(bool bWriteInterestEnabled)
{
	return FD_READ | (bWriteInterestEnabled ? FD_WRITE : 0);
}
