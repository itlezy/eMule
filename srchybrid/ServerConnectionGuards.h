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

#include "types.h"

/**
 * Reports whether the caller can safely dereference a cached current-server
 * snapshot after observing the connected state.
 */
inline bool HasConnectedServerSnapshot(bool bIsConnected, const void *pCurrentServer)
{
	return bIsConnected && pCurrentServer != NULL;
}

/**
 * Reports whether a capability read from the cached current-server snapshot is
 * usable after the connected-server null guard has been applied.
 */
inline bool HasConnectedServerCapability(bool bIsConnected, const void *pCurrentServer, bool bCapabilityFlag)
{
	return HasConnectedServerSnapshot(bIsConnected, pCurrentServer) && bCapabilityFlag;
}

/**
 * Reports whether the cached current-server endpoint matches the requested
 * server identity after the connected-server null guard has been applied.
 */
inline bool MatchesConnectedServerEndpoint(bool bIsConnected, const void *pCurrentServer, uint32 nCurrentServerIP, uint16 nCurrentServerPort, uint32 nServerIP, uint16 nServerPort)
{
	return HasConnectedServerSnapshot(bIsConnected, pCurrentServer)
		&& nCurrentServerIP == nServerIP
		&& nCurrentServerPort == nServerPort;
}
